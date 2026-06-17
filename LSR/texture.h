#pragma once

#include "bmp.h"

typedef struct texture {
    char*   name;
    int     width, height;
    u8*     data;
} texture;

int texture_create(const char* name, int w, int h, texture** outObj);
void texture_release(texture* t);

int texture_load_bitmap(texture* t, const bmp* image);
int texture_set_color(texture* t, const RECT* rect, u32 color);
int texture_blt(texture* t, const texture* image);

int texture_get_point_color(const texture* t, int x, int y, u32* color);
int texture_set_point_color(texture* t, int x, int y, u32 color);

int texture_get_point_depth(const texture* t, int x, int y, f32* depth);
int texture_set_point_depth(texture* t, int x, int y, f32 depth);
