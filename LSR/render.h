#pragma once

#include "texture.h"
#include "surface.h"

typedef struct render render;

typedef enum render_clipping {
    RENDER_CLIPPING_DISABLED    = 0,
    RENDER_CLIPPING_ENABLED     = 1,
    RENDER_CLIPPING_COUNT       = 2,
    RENDER_CLIPPING_FORCE_DWORD = 0x7FFFFFFF
} render_clipping;

typedef enum render_culling {
    RENDER_CULLING_NONE         = 0,
    RENDER_CULLING_CCW          = 1,
    RENDER_CULLING_CW           = 2,
    RENDER_CULLING_COUNT        = 3,
    RENDER_CULLING_FORCE_DWORD  = 0x7FFFFFFF
} render_culling;

typedef enum render_fog {
    RENDER_FOG_NONE                 = 0,
    RENDER_FOG_LINEAR               = 1,
    RENDER_FOG_EXPONENTIAL          = 2,
    RENDER_FOG_EXPONENTIAL_SQUARED  = 3,
    RENDER_FOG_COUNT                = 4,
    RENDER_FOG_FORCE_DWORD          = 0x7FFFFFFF
} render_fog;

typedef enum render_draw_mode {
    RENDER_DRAW_MODE_POINTS         = 0,
    RENDER_DRAW_MODE_TRIANGLES      = 1,
    RENDER_DRAW_MODE_COUNT          = 2,
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

typedef enum render_blending {
    RENDER_BLENDING_DISABLED        = 0,
    RENDER_BLENDING_ENABLED         = 1,
    RENDER_BLENDING_COUNT           = 2,
    RENDER_BLENDING_FORCE_DWORD     = 0x7FFFFFFF
} render_blending;

typedef struct render_viewport {
    int x, y, width, height;
    f32 min, max;
} render_viewport;

int render_create(render** outObj);
void render_release(render* r);

int render_set_draw_mode(render* r, render_draw_mode mode);
int render_set_blending(render* r, render_blending blending);
int render_set_clipping(render* r, render_clipping clipping);
int render_set_culling(render* r, render_culling culling);
int render_set_fog(render* r, render_fog fog);
int render_set_fog_color(render* r, u32 color);
int render_set_fog_density(render* r, f32 density);
int render_set_fog_range(render* r, f32 start, f32 end);
int render_set_texture(render* r, const texture* t);
int render_set_matrix(render* r, render_matrix type, const f32m4* matrix);
int render_set_viewport(render* r, const render_viewport* vp);

int render_set_render_target(render* r, surface* t);
int render_set_depth_buffer(render* r, render_depth_buffer depth);

int render_start(render* r);
int render_end(render* r);

int render_clear(render* r, u32 color);
int render_draw(render* r, const vertex* vertexes, const int* indexes, size_t index_count);
