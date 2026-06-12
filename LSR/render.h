#pragma once

#include "texture.h"
#include "surface.h"

typedef struct render render;

typedef enum render_fog {
    RENDER_FOG_NONE         = 0,
    RENDER_FOR_LINEAR       = 1,
    RENDER_FOG_COUNT        = 2,
    RENDER_FOG_FORCE_DWORD  = 0x7FFFFFFF
} render_fog;

typedef enum render_draw_mode {
    RENDER_DRAW_MODE_POINTS         = 0,
    RENDER_DRAW_MODE_COUNT          = 1,
    RENDER_DRAW_MODE_FORCE_DWORD    = 0x7FFFFFFF
} render_draw_mode;

typedef enum render_matrix {
    RENDER_MATRIX_WORLD         = 0,
    RENDER_MATRIX_VIEW          = 1,
    RENDER_MATRIX_PROJECTION    = 2,
    RENDER_MATRIX_COUNT         = 3,
    RENDER_MATRIX_FORCE_DWORD   = 0x7FFFFFFF
} render_matrix;

typedef enum render_depth_buffer {
    RENDER_DEPTH_BUFFER_NONE        = 0,
    RENDER_DEPTH_BUFFER_Z           = 1,
    RENDER_DEPTH_BUFFER_W           = 2,
    RENDER_DEPTH_BUFFER_COUNT       = 3,
    RENDER_DEPTH_BUFFER_FORCE_DWORD = 0x7FFFFFFF
} render_depth_buffer;

typedef struct render_viewport {
    int x, y, width, height;
    f32 min, max;
} render_viewport;

int render_create(render** outObj);
void render_release(render* r);

int render_set_draw_mode(render* r, render_draw_mode mode);
int render_set_fog(render* r, render_fog fog);
int render_set_texture(render* r, const texture* t);
int render_set_matrix(render* r, render_matrix type, const f32m4* matrix);
int render_set_viewport(render* r, const render_viewport* vp);

int render_set_render_target(render* r, surface* t);
int render_set_depth_buffer(render* r, render_depth_buffer depth);

int render_start(render* r);
int render_end(render* r);

int render_clear(render* r, u32 color);
int render_draw(render* r, const vertex* vertexes, const int* indexes, size_t index_count);
