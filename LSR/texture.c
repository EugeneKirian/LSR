#include "texture.h"

#include <stdlib.h>

static int texture_allocate(const char* name, int w, int h, texture** outObj) {
    if (w < 0 || h < 0 || outObj == NULL) {
        return LSRERR_INVALID_ARGUMENT;
    }

    texture* t = (texture*)malloc(sizeof(texture));
    if (t == NULL) {
        return LSRERR_OUT_OF_MEMORY;
    }

    ZeroMemory(t, sizeof(texture));

    t->width = w;
    t->height = h;

    if (name != NULL) {
        t->name = (char*)malloc(strlen(name) + 1);
        if (t->name == NULL) {
            texture_release(t);
            return LSRERR_OUT_OF_MEMORY;
        }

        strcpy(t->name, name);
    }

    const size_t size = w * h * sizeof(u32);

    t->data = (char*)malloc(size);
    if (t->data == NULL) {
        texture_release(t);
        return LSRERR_OUT_OF_MEMORY;
    }

    ZeroMemory(t->data, size);

    *outObj = t;

    return LSRERR_OK;
}

int texture_create(const char* name, int w, int h, texture** outObj) {
    if (w < 0 || h < 0 || outObj == NULL) {
        return LSRERR_INVALID_ARGUMENT;
    }

    texture* t = NULL;
    int result = texture_allocate(name, w, h, &t);
    if (result != LSRERR_OK) {
        return result;
    }

    *outObj = t;

    return LSRERR_OK;
}

void texture_release(texture* t) {
    if (t != NULL) {
        if (t->name != NULL) {
            free(t->name);
        }

        if (t->data != NULL) {
            free(t->data);
        }

        free(t);
    }
}

int texture_load_bitmap(texture* t, const bmp* image) {
    if (t == NULL || image == NULL) {
        return LSRERR_INVALID_ARGUMENT;
    }

    if (image->info.biPlanes != 1
        || image->info.biBitCount != 24
        || image->info.biHeight < 0) {
        return LSRERR_NOT_SUPPORTED;
    }

    const int image_width = image->info.biWidth;
    const int image_height = image->info.biHeight;

    // Calculate minimum width and height
    const int w = t->width < image_width ? t->width : image_width;
    const int h = t->height < image_height ? t->height : image_height;

    // Fli the image and convert pixels to 32-bit RGBA
    const char* start = image->data + image_width * (image_height - 1) * sizeof(RGBTRIPLE);
    for (int x = 0; x < h; x++) {
        u32* dst = (u32*)(t->data + w * x * sizeof(u32));
        const RGBTRIPLE* src =
            (RGBTRIPLE*)(start - image_width * x * sizeof(RGBTRIPLE));
        for (int xx = 0; xx < w; xx++) {
            dst[xx] = (0xFF << 24) | (src[xx].rgbtRed << 16)
                | (src[xx].rgbtGreen << 8) | src[xx].rgbtBlue;
        }
    }

    return LSRERR_OK;
}

int texture_set_color(texture* t, const RECT* rect, u32 color) {
    if (t == NULL) {
        return LSRERR_INVALID_ARGUMENT;
    }

    if (rect != NULL) {
        // Check if the rectangle is outside the boundaries
        if (rect->right < 0 || rect->bottom < 0
            || rect->left > t->width || rect->top > t->height) {
            return LSRERR_OK;
        }
    }

    // Calculate actual intersection of a provided rectangle and the texture
    const int min_x = rect == NULL ? 0
        : (0 < rect->left ? rect->left : 0);
    const int max_x = rect == NULL ? t->width
        : (t->width < rect->right ? t->width : rect->right);

    const int min_y = rect == NULL ? 0
        : (0 < rect->top ? rect->top : 0);
    const int max_y = rect == NULL ? t->height
        : (t->height < rect->bottom ? t->height : rect->bottom);

    for (int x = min_y; x < max_y; x++) {
        u32* pixels = (u32*)(t->data + x * t->width * sizeof(u32));
        for (int xx = min_x; xx < max_x; xx++) {
            pixels[xx] = color;
        }
    }

    return LSRERR_OK;
}

int texture_blt(texture* t, const texture* image) {
    if (t == NULL || image == NULL) {
        return LSRERR_INVALID_ARGUMENT;
    }

    const int w = t->width < image->width ? t->width : image->width;
    const int h = t->height < image->height ? t->height : image->height;

    for (int i = 0; i < h; i++) {
        const u32* src = (u32*)(image->data + i * image->width * sizeof(u32));
        u32* dst = (u32*)(t->data + i * t->width * sizeof(u32));
        CopyMemory(dst, src, w * sizeof(u32));
    }

    return LSRERR_OK;
}

int texture_draw_point(texture* t, int x, int y, u32 color) {
    if (t == NULL) {
        return LSRERR_INVALID_ARGUMENT;
    }

    if (x < 0 || t->width <= x || y < 0 || t->height <= y) {
        return LSRERR_INVALID_ARGUMENT;
    }

    u32* pixels = (u32*)(t->data + y * t->width * sizeof(u32));
    pixels[x] = color;

    return LSRERR_OK;
}
