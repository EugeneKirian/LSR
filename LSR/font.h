#pragma once

#include "common.h"
#include "texture.h"

#define FONT_MAX_QUAD_COUNT 4

typedef struct fqd {
    int x, y;
    int width, height;
    int span;
    f32x2 uv[FONT_MAX_QUAD_COUNT];
} fqd;

typedef struct font font;

int font_create(const char* name, int size, int bold, u32 front, u32 back, font** outObj);
void font_release(font* f);

int font_get_texture(const font* f, const texture** t);
int font_get_item(const font* f, u32 c, fqd* quad);
int font_get_height(const font* f, int* height);
