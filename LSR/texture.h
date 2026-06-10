#pragma once

#include "bmp.h"

typedef struct texture {
    char*   name;
    int     width, height;
    char*   data;
} texture;

int texture_create(const char* name, int w, int h, texture** outObj);
void texture_release(texture* t);

int texture_load_bitmap(texture* t, const bmp* image);
int texture_set_color(texture* t, const RECT* rect, u32 color);
int texture_blt(texture* t, const texture* image);
