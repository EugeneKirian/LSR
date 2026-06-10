#pragma once

#include "texture.h"

typedef struct surface {
    HDC                 hdc;
    CRITICAL_SECTION    lock;
    int                 width, height;
    HBITMAP             bitmap;
    char*               data;
} surface;

int surface_create(HDC hdc, int w, int h, surface** outObj);
void surface_release(surface* s);

int surface_load_texture(surface* s, const texture* t);
int surface_resize(surface* s, int w, int h);
int surface_blt(surface* s, const RECT* rect, HDC hdc, const POINT* point);
