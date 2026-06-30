#pragma once

#include "bmp.h"

typedef enum texture_type {
    TEXTURE_TYPE_SIMPLE         = 0,
    TEXTURE_TYPE_COMPLEX        = 1,
    TEXTURE_TYPE_COUNT          = 2,
    TEXTURE_TYPE_FORCE_DWORD    = 0x7FFFFFFF
} texture_type;

typedef struct texture_level {
    int width, height;
    u8* data;
} texture_level;

typedef struct texture {
    char*   name;
    int     width, height;
    texture_level* levels;
    int     level_count;
} texture;

typedef enum texture_sampling {
    TEXTURE_SAMPLING_NEAREST        = 0,
    TEXTURE_SAMPLING_BILINEAR       = 1,
    TEXTURE_SAMPLING_TRILINEAR      = 2,
    TEXTURE_SAMPLING_COUNT          = 3,
    TEXTURE_SAMPLING_FORCE_DWORD    = 0x7FFFFFFF
} texture_sampling;

int texture_create(const char* name, int w, int h, texture_type type, texture** outObj);
void texture_release(texture* t);

int texture_load_bitmap(texture* t, const bmp* image);
int texture_set_color(texture* t, const RECT* rect, int level, u32 color);
int texture_blt(texture* t, const texture* image, int dst_level, int src_level);

int texture_get_point_color(const texture* t, int level, int x, int y, u32* color);
int texture_set_point_color(texture* t, int level, int x, int y, u32 color);

int texture_get_point_depth(const texture* t, int level, int x, int y, f32* depth);
int texture_set_point_depth(texture* t, int level, int x, int y, f32 depth);

int texture_sample(const texture* t, texture_sampling sampling, f32 u, f32 v, f32 lod, f32x4* color);
int texute_get_line_level_of_detail(const texture* t,
    f32 x0, f32 y0, f32 x1, f32 y1, f32 u0, f32 v0, f32 u1, f32 v1, f32* lod);
int texute_get_triangle_level_of_detail(const texture* t,
    f32 x0, f32 y0, f32 x1, f32 y1, f32 x2, f32 y2,
    f32 u0, f32 v0, f32 u1, f32 v1, f32 u2, f32 v2, f32* lod);
