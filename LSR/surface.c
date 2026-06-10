#include "surface.h"

#include <stdlib.h>

static int surface_allocate(int w, int h, surface** outObj) {
    if (w < 0 || h < 0 || outObj == NULL) {
        return LSRERR_INVALID_ARGUMENT;
    }

    surface* s = (surface*)malloc(sizeof(surface));
    if (s == NULL) {
        return LSRERR_OUT_OF_MEMORY;
    }

    ZeroMemory(s, sizeof(surface));

    s->width = w;
    s->height = h;

    InitializeCriticalSection(&s->lock);

    *outObj = s;

    return LSRERR_OK;
}

static int surface_lock(surface* s) {
    if (s == NULL) {
        return LSRERR_INVALID_ARGUMENT;
    }

    EnterCriticalSection(&s->lock);

    return LSRERR_OK;
}

static int surface_unlock(surface* s) {
    if (s == NULL) {
        return LSRERR_INVALID_ARGUMENT;
    }

    LeaveCriticalSection(&s->lock);

    return LSRERR_OK;
}

int surface_create(HDC hdc, int w, int h, surface** outObj) {
    if (hdc == NULL || w < 0 || h < 0 || outObj == NULL) {
        return LSRERR_INVALID_ARGUMENT;
    }

    surface* s = NULL;
    int result = surface_allocate(w, h, &s);
    if (result != LSRERR_OK) {
        return result;
    }

    s->hdc = CreateCompatibleDC(hdc);
    if (s->hdc == NULL) {
        surface_release(s);
        return LSRERR_OUT_OF_MEMORY;
    }

    BITMAPINFO info;
    ZeroMemory(&info, sizeof(BITMAPINFO));

    info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    info.bmiHeader.biWidth = w == 0 ? 1 : w;
    info.bmiHeader.biHeight = h == 0 ? -1 : -h;
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = 32;
    info.bmiHeader.biCompression = BI_RGB;

    s->bitmap = CreateDIBSection(s->hdc, &info, DIB_RGB_COLORS, &s->data, NULL, 0);
    if (s->bitmap == NULL) {
        surface_release(s);
        return LSRERR_OUT_OF_MEMORY;
    }

    SelectObject(s->hdc, s->bitmap);

    *outObj = s;

    return result;
}

void surface_release(surface* s) {
    if (s != NULL) {
        DeleteCriticalSection(&s->lock);

        if (s->bitmap != NULL) {
            DeleteObject(s->bitmap);
        }

        if (s->hdc != NULL) {
            DeleteDC(s->hdc);
        }

        free(s);
    }
}

int surface_load_texture(surface* s, const texture* t) {
    if (s == NULL || t == NULL) {
        return LSRERR_INVALID_ARGUMENT;
    }

    const int w = s->width < t->width ? s->width : t->width;
    const int h = s->height < t->height ? s->height : t->height;

    surface_lock(s);

    for (int i = 0; i < h; i++) {
        const u32* src = (u32*)(t->data + i * t->width * sizeof(u32));
        u32* dst = (u32*)(s->data + i * s->width * sizeof(u32));
        CopyMemory(dst, src, w * sizeof(u32));
    }

    surface_unlock(s);

    return LSRERR_OK;
}

int surface_resize(surface* s, int w, int h) {
    if (s == NULL || w < 0 || h < 0) {
        return LSRERR_INVALID_ARGUMENT;
    }

    surface_lock(s);

    if (s->bitmap != NULL) {
        DeleteObject(s->bitmap);
    }

    s->width = w;
    s->height = h;

    BITMAPINFO info;
    ZeroMemory(&info, sizeof(BITMAPINFO));

    info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    info.bmiHeader.biWidth = w == 0 ? 1 : w;
    info.bmiHeader.biHeight = h == 0 ? -1 : -h;
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = 32;
    info.bmiHeader.biCompression = BI_RGB;

    s->bitmap = CreateDIBSection(s->hdc, &info, DIB_RGB_COLORS, &s->data, NULL, 0);
    if (s->bitmap == NULL) {
        return LSRERR_OUT_OF_MEMORY;
    }

    SelectObject(s->hdc, s->bitmap);
    surface_unlock(s);

    return LSRERR_OK;
}

int surface_blt(surface* s, const RECT* rect, HDC hdc, const POINT* point) {
    if (s == NULL) {
        return LSRERR_INVALID_ARGUMENT;
    }

    const int sl = rect == NULL ? 0 : rect->left;
    const int st = rect == NULL ? 0 : rect->top;
    const int sr = rect == NULL ? 0 : rect->right;
    const int sb = rect == NULL ? 0 : rect->bottom;

    const int px = point == NULL ? 0 : point->x;
    const int py = point == NULL ? 0 : point->y;

    surface_lock(s);
    BitBlt(hdc, sl, st, sr, sb, s->hdc, px, py, SRCCOPY);
    surface_unlock(s);

    return LSRERR_OK;
}
