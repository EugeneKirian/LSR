#include "arena.h"
#include "render.h"

#include <stdlib.h>

#define RENDER_MAX_VERTEX_COUNT 65535

typedef struct rv {
    f32x4 position;
    f32x3 normal;
    u32 color;
    f32x2 uv;
} rv;

struct render {
    int active;
    arena* arena;
    struct {
        render_fog fog;
        render_draw_mode mode;
        render_depth_buffer depth;
    } settings;
    f32m4 matrixes[RENDER_MATRIX_COUNT];
    const texture* current;
    surface* target;
    render_viewport viewport;
    texture* surface, *depth;
};

static int is_viewport_valid(const render_viewport* vp, int* valid) {
    if (vp == NULL || valid == NULL) {
        return LSRERR_INVALID_ARGUMENT;
    }

    *valid = vp->x != 0 || vp->y != 0
        || vp->width != 0 || vp->height != 0;

    return LSRERR_OK;
}

static int render_allocate(render** outObj) {
    if (outObj == NULL) {
        return LSRERR_INVALID_ARGUMENT;
    }

    render* r = (render*)malloc(sizeof(render));
    if (r == NULL) {
        return LSRERR_OUT_OF_MEMORY;
    }

    ZeroMemory(r, sizeof(render));

    r->arena = arena_create(2 * RENDER_MAX_VERTEX_COUNT * (sizeof(rv) + sizeof(int)));
    if (r->arena == NULL) {
        free(r);
        return LSRERR_OUT_OF_MEMORY;
    }

    *outObj = r;

    return LSRERR_OK;
}

int render_create(render** outObj) {
    if (outObj == NULL) {
        return LSRERR_INVALID_ARGUMENT;
    }

    render* r = NULL;
    int result = render_allocate(&r);
    if (result != LSRERR_OK) {
        return result;
    }

    r->settings.mode = RENDER_DRAW_MODE_POINTS; // TODO

    for (size_t i = 0; i < RENDER_MATRIX_COUNT; i++) {
        if ((result = f32m4_identity(&r->matrixes[i])) != LSRERR_OK) {
            render_release(r);
            return result;
        }
    }

    *outObj = r;

    return LSRERR_OK;
}

void render_release(render* r) {
    if (r != NULL) {
        if (r->arena != NULL) {
            arena_release(r->arena);
        }

        if (r->surface != NULL) {
            texture_release(r->surface);
        }

        if (r->depth != NULL) {
            texture_release(r->depth);
        }

        free(r);
    }
}

int render_set_draw_mode(render* r, render_draw_mode mode) {
    if (r == NULL) {
        return LSRERR_INVALID_ARGUMENT;
    }

    if (mode < RENDER_DRAW_MODE_POINTS || mode >= RENDER_DRAW_MODE_COUNT) {
        return LSRERR_INVALID_ARGUMENT;
    }

    r->settings.mode = mode;

    return LSRERR_OK;
}

int render_set_fog(render* r, render_fog fog) {
    if (r == NULL) {
        return LSRERR_INVALID_ARGUMENT;
    }

    if (fog < RENDER_FOG_NONE || fog >= RENDER_FOG_COUNT) {
        return LSRERR_INVALID_ARGUMENT;
    }

    r->settings.fog = fog;

    return LSRERR_OK;
}

int render_set_texture(render* r, const texture* t) {
    if (r == NULL) {
        return LSRERR_INVALID_ARGUMENT;
    }

    r->current = t;

    return LSRERR_OK;
}

int render_set_matrix(render* r, render_matrix type, const f32m4* matrix) {
    if (r == NULL || matrix == NULL) {
        return LSRERR_INVALID_ARGUMENT;
    }

    if (type < RENDER_MATRIX_WORLD || type >= RENDER_MATRIX_COUNT) {
        return LSRERR_INVALID_ARGUMENT;
    }

    CopyMemory(&r->matrixes[type], matrix, sizeof(f32m4));

    return LSRERR_OK;
}

int render_set_viewport(render* r, const render_viewport* vp) {
    if (r == NULL || vp == NULL) {
        return LSRERR_INVALID_ARGUMENT;
    }

    int valid = FALSE;
    int result = is_viewport_valid(vp, &valid);
    if (result != LSRERR_OK || !valid) {
        return LSRERR_INVALID_ARGUMENT;
    }

    if (r->active) {
        return LSRERR_INVALID_CALL;
    }

    const int w = vp->width - vp->x;
    const int h = vp->height - vp->y;

    texture* surface = NULL;
    if ((result = texture_create(NULL, w, h, &surface)) != LSRERR_OK) {
        return LSRERR_OUT_OF_MEMORY;
    }

    texture* depth = NULL;
    if ((result = texture_create(NULL, w, h, &depth)) != LSRERR_OK) {
        texture_release(surface);
        return LSRERR_OUT_OF_MEMORY;
    }

    texture_release(InterlockedExchangePointer(&r->surface, surface));
    texture_release(InterlockedExchangePointer(&r->depth, depth));

    CopyMemory(&r->viewport, vp, sizeof(render_viewport));

    return LSRERR_OK;
}

int render_set_render_target(render* r, surface* s) {
    if (r == NULL) {
        return LSRERR_INVALID_ARGUMENT;
    }

    if (r->active) {
        return LSRERR_INVALID_CALL;
    }

    r->target = s;

    return LSRERR_OK;
}

int render_set_depth_buffer(render* r, render_depth_buffer depth) {
    if (r == NULL) {
        return LSRERR_INVALID_ARGUMENT;
    }

    if (depth < RENDER_DEPTH_BUFFER_NONE || depth >= RENDER_DEPTH_BUFFER_COUNT) {
        return LSRERR_INVALID_ARGUMENT;
    }

    r->settings.depth = depth;

    return LSRERR_OK;
}

int render_start(render* r) {
    if (r == NULL) {
        return LSRERR_INVALID_ARGUMENT;
    }

    if (r->active || r->target == NULL) {
        return LSRERR_INVALID_CALL;
    }

    int valid = FALSE;
    const int result = is_viewport_valid(&r->viewport, &valid);
    if (result != LSRERR_OK || !valid) {
        return LSRERR_INVALID_CALL;
    }

    r->active = TRUE;

    texture_set_color(r->surface, NULL, 0x00000000);
    texture_set_color(r->depth, NULL, 0x7F800000); // Positive Infinity

    return LSRERR_OK;
}

int render_end(render* r) {
    if (r == NULL) {
        return LSRERR_INVALID_ARGUMENT;
    }

    if (!r->active) {
        return LSRERR_INVALID_CALL;
    }

    const int result = surface_load_texture(r->target, r->surface);

    r->active = FALSE;

    return result;
}

int render_draw(render* r, const vertex* vertexes, const int* indexes, size_t index_count) {
    if (r == NULL || vertexes == NULL || indexes == NULL) {
        return LSRERR_INVALID_ARGUMENT;
    }

    if (!r->active) {
        return LSRERR_INVALID_CALL;
    }

    // TODO draw...
    texture_blt(r->surface, r->current);

    return LSRERR_OK;
}
