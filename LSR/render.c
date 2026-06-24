#include "arena.h"
#include "frustrum.h"
#include "render.h"

#include <float.h>
#include <math.h>
#include <stdlib.h>

#define EPSILON                         (1e-5f)

#define PRIMITIVE_LINE_VERTEX_COUNT     2
#define PRIMITIVE_TRIANGLE_VERTEX_COUNT 3

#define RENDER_MAX_VERTEX_COUNT         65535

typedef struct v {
    f32x4 position;
    f32x3 normal;
    f32x4 color;
    f32x2 uv;
} v;

typedef int (*vertex_shader)(render* r, const f32m4* wvp,
    const vertex* in_vertexes, const int* in_indexes, size_t index_count,
    v** out_vertexes, int** out_indexes, size_t* out_index_count);

typedef int (*geometry_shader)(render* r,
    const v* in_vertexes, const int* in_indexes, size_t in_index_count,
    v** out_vertexes, size_t* out_vertex_count);

typedef int(*fragment_shader)(render* r,
    const v* in_vertexes, size_t in_vertex_count);

typedef struct pipeline {
    vertex_shader   vertex;
    geometry_shader geometry;
    fragment_shader fragment;
} pipeline;

#define RENDER_MAX_ARENA_SIZE (6 * RENDER_MAX_VERTEX_COUNT * (sizeof(v) + sizeof(int)))

struct render {
    int active;
    arena* arena;
    struct {
        render_blending blending;
        render_clipping clipping;
        render_culling culling;
        render_fill fill;
        struct {
            render_fog mode;
            f32 density;
            f32 start, end;
            f32x4 color;
        } fog;
        render_draw_mode mode;
        render_depth_buffer depth;
    } settings;
    f32m4 matrixes[RENDER_MATRIX_COUNT];
    const texture* current;
    surface* target;
    render_viewport viewport;
    texture* surface, *depth;
};

static int clip_line_against_plane(v* v1, v* v2, frustrum_plane plane); // TODO
static int clip_polygon_against_plane(const v* in_vertexes, int in_vertex_count,
    frustrum_plane plane, v* out_vertexes, int* out_vertex_count);

static int vertex_interpolate(v* result, const v* v1, const v* v2, f32 t);

static inline int viewport_is_valid(const render_viewport* vp) {
    return vp->x != 0 || vp->y != 0
        || vp->width != 0 || vp->height != 0;
}

static int render_get_fog_factor(const render* r, f32 depth, f32* factor);
static int rasterize_pixel(render* r, int x, int y, int tx, int ty, f32 fog, f32 depth, const f32x4* pixel);

static int vertex_shader_stage(render* r, const f32m4* wvp,
    const vertex* in_vertexes, const int* in_indexes, size_t index_count,
    v** out_vertexes, int** out_indexes, size_t* out_index_count);

static int geometry_shader_point_stage(render* r,
    const v* in_vertexes, const int* in_indexes, size_t in_index_count,
    v** out_vertexes, size_t* out_vertex_count);
static int fragment_shader_point_stage(render* r,
    const v* in_vertexes, size_t in_vertex_count);

static int geometry_shader_line_stage(render* r,
    const v* in_vertexes, const int* in_indexes, size_t in_index_count,
    v** out_vertexes, size_t* out_vertex_count);
static int fragment_shader_line_stage(render* r,
    const v* in_vertexes, size_t in_vertex_count);

static int geometry_shader_triangle_stage(render* r,
    const v* in_vertexes, const int* in_indexes, size_t in_index_count,
    v** out_vertexes, size_t* out_vertex_count);
static int fragment_shader_triangle_stage(render* r,
    const v* in_vertexes, size_t in_vertex_count);

static int render_allocate(render** outObj) {
    if (outObj == NULL) {
        return LSRERR_INVALID_ARGUMENT;
    }

    render* r = (render*)malloc(sizeof(render));
    if (r == NULL) {
        return LSRERR_OUT_OF_MEMORY;
    }

    ZeroMemory(r, sizeof(render));

    r->arena = arena_create(RENDER_MAX_ARENA_SIZE);
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

    for (size_t i = 0; i < RENDER_MATRIX_COUNT; i++) {
        if ((result = f32m4_identity(&r->matrixes[i])) != LSRERR_OK) {
            render_release(r);
            return result;
        }
    }

    r->settings.clipping = RENDER_CLIPPING_ENABLED;
    r->settings.culling = RENDER_CULLING_CCW;
    r->settings.depth = RENDER_DEPTH_BUFFER_Z;
    r->settings.fill = RENDER_FILL_SOLID;
    r->settings.mode = RENDER_DRAW_MODE_TRIANGLES;

    r->settings.fog.color.a = 1.0f;
    r->settings.fog.color.r = 1.0f;
    r->settings.fog.color.g = 1.0f;
    r->settings.fog.color.b = 1.0f;
    r->settings.fog.mode = RENDER_FOG_NONE;
    r->settings.fog.density = 0.33f;
    r->settings.fog.start = 1.0f;
    r->settings.fog.end = 100.0f;

    *outObj = r;

    return result;
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

int render_set_blending(render* r, render_blending blending) {
    if (r == NULL) {
        return LSRERR_INVALID_ARGUMENT;
    }

    if (blending < RENDER_BLENDING_DISABLED || blending >= RENDER_BLENDING_COUNT) {
        return LSRERR_INVALID_ARGUMENT;
    }

    r->settings.blending = blending;

    return LSRERR_OK;
}

int render_set_clipping(render* r, render_clipping clipping) {
    if (r == NULL) {
        return LSRERR_INVALID_ARGUMENT;
    }

    if (clipping < RENDER_CLIPPING_DISABLED || clipping >= RENDER_CLIPPING_COUNT) {
        return LSRERR_INVALID_ARGUMENT;
    }

    r->settings.clipping = clipping;

    return LSRERR_OK;
}

int render_set_culling(render* r, render_culling culling) {
    if (r == NULL) {
        return LSRERR_INVALID_ARGUMENT;
    }

    if (culling < RENDER_CULLING_NONE || culling >= RENDER_CULLING_COUNT) {
        return LSRERR_INVALID_ARGUMENT;
    }

    r->settings.culling = culling;

    return LSRERR_OK;
}

int render_set_fill(render* r, render_fill fill) {
    if (r == NULL) {
        return LSRERR_INVALID_ARGUMENT;
    }

    if (fill < RENDER_FILL_POINT || fill >= RENDER_FILL_COUNT) {
        return LSRERR_INVALID_ARGUMENT;
    }

    r->settings.fill = fill;

    return LSRERR_OK;
}

int render_set_fog(render* r, render_fog fog) {
    if (r == NULL) {
        return LSRERR_INVALID_ARGUMENT;
    }

    if (fog < RENDER_FOG_NONE || fog >= RENDER_FOG_COUNT) {
        return LSRERR_INVALID_ARGUMENT;
    }

    r->settings.fog.mode = fog;

    return LSRERR_OK;
}

int render_set_fog_color(render* r, u32 color) {
    if (r == NULL) {
        return LSRERR_INVALID_ARGUMENT;
    }

    return f32x4_set_color(&r->settings.fog.color, color);
}

int render_set_fog_density(render* r, f32 density) {
    if (r == NULL) {
        return LSRERR_INVALID_ARGUMENT;
    }

    r->settings.fog.density = density;

    return LSRERR_OK;
}

int render_set_fog_range(render* r, f32 start, f32 end) {
    if (r == NULL) {
        return LSRERR_INVALID_ARGUMENT;
    }

    r->settings.fog.start = start;
    r->settings.fog.end = end;

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

    if (!viewport_is_valid(vp)) {
        return LSRERR_INVALID_ARGUMENT;
    }

    if (r->active) {
        return LSRERR_INVALID_CALL;
    }

    int result = LSRERR_OK;
    const int w = vp->width - vp->x;
    const int h = vp->height - vp->y;

    texture* surface = NULL;
    if ((result = texture_create(NULL, w, h, &surface)) != LSRERR_OK) {
        return result;
    }

    texture* depth = NULL;
    if ((result = texture_create(NULL, w, h, &depth)) != LSRERR_OK) {
        texture_release(surface);
        return result;
    }

    texture_release(InterlockedExchangePointer(&r->surface, surface));
    texture_release(InterlockedExchangePointer(&r->depth, depth));

    CopyMemory(&r->viewport, vp, sizeof(render_viewport));

    return f32m4_projection(&r->matrixes[RENDER_MATRIX_PROJECTION], w, h, vp->min, vp->max);
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

    if (r->active || r->target == NULL
        || !viewport_is_valid(&r->viewport)) {
        return LSRERR_INVALID_CALL;
    }

    r->active = TRUE;

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

int render_clear(render* r, u32 color) {
    if (r == NULL) {
        return LSRERR_INVALID_ARGUMENT;
    }

    if (!r->active) {
        return LSRERR_INVALID_CALL;
    }

    texture_set_color(r->surface, NULL, color);
    texture_set_color(r->depth, NULL, 0x7F800000); // Positive Infinity

    return LSRERR_OK;
}

int render_draw(render* r, const vertex* vertexes, const int* indexes, size_t index_count) {
    if (r == NULL || vertexes == NULL || indexes == NULL) {
        return LSRERR_INVALID_ARGUMENT;
    }

    if (!r->active) {
        return LSRERR_INVALID_CALL;
    }

    arena_clear(r->arena);

    f32m4 wv, wvp;
    ZeroMemory(&wv, sizeof(f32m4));
    ZeroMemory(&wvp, sizeof(f32m4));

    pipeline steps;

    int result = LSRERR_OK;
    if ((result = f32m4_multiply(&r->matrixes[RENDER_MATRIX_WORLD],
        &r->matrixes[RENDER_MATRIX_VIEW], &wv)) == LSRERR_OK) {
        if ((result = f32m4_multiply(&wv,
            &r->matrixes[RENDER_MATRIX_PROJECTION], &wvp)) == LSRERR_OK) {
            // Construct the shader pipeline to rasterize various geometry (primitives).
            steps.vertex = vertex_shader_stage;
            switch (r->settings.mode) {
            case RENDER_DRAW_MODE_POINTS: {
                steps.geometry = geometry_shader_point_stage;
                steps.fragment = fragment_shader_point_stage;
            } break;
            case RENDER_DRAW_MODE_LINES: {
                steps.geometry = geometry_shader_line_stage;
                steps.fragment = r->settings.fill == RENDER_FILL_POINT
                    ? fragment_shader_point_stage : fragment_shader_line_stage;
            } break;
            case RENDER_DRAW_MODE_TRIANGLES: {
                steps.geometry = geometry_shader_triangle_stage;
                switch (r->settings.fill) {
                case RENDER_FILL_POINT: { steps.fragment = fragment_shader_point_stage; } break;
                case RENDER_FILL_WIRE: { steps.fragment = fragment_shader_line_stage; } break;
                case RENDER_FILL_SOLID: { steps.fragment = fragment_shader_triangle_stage; } break;
                default: { return LSRERR_INVALID_CALL; } break;
                }
            } break;
            default: { return LSRERR_INVALID_CALL; } break;
            }

            v* vs = NULL;
            int* indx = NULL;
            size_t count = 0;
            // Execute fixed shader pipeline rasterization.
            if ((result = steps.vertex(r, &wvp,
                vertexes, indexes, index_count, &vs, &indx, &count)) == LSRERR_OK) {
                if ((result = steps.geometry(r, vs, indx, count, &vs, &count)) == LSRERR_OK) {
                    result = steps.fragment(r, vs, count);
                }
            }
        }
    }

    return result;
}

int vertex_interpolate(v* result, const v* v1, const v* v2, f32 t) {
    if (result == NULL || v1 == NULL || v2 == NULL) {
        return LSRERR_INVALID_ARGUMENT;
    }

    result->position.x = v1->position.x + t * (v2->position.x - v1->position.x);
    result->position.y = v1->position.y + t * (v2->position.y - v1->position.y);
    result->position.z = v1->position.z + t * (v2->position.z - v1->position.z);
    result->position.w = v1->position.w + t * (v2->position.w - v1->position.w);

    result->normal.x = v1->normal.x + t * (v2->normal.x - v1->normal.x);
    result->normal.y = v1->normal.y + t * (v2->normal.y - v1->normal.y);
    result->normal.z = v1->normal.z + t * (v2->normal.z - v1->normal.z);

    result->uv.x = v1->uv.x + t * (v2->uv.x - v1->uv.x);
    result->uv.y = v1->uv.y + t * (v2->uv.y - v1->uv.y);

    result->color.a = v1->color.a + t * (v2->color.a - v1->color.a);
    result->color.r = v1->color.r + t * (v2->color.r - v1->color.r);
    result->color.g = v1->color.g + t * (v2->color.g - v1->color.g);
    result->color.b = v1->color.b + t * (v2->color.b - v1->color.b);

    return LSRERR_OK;;
}

int clip_polygon_against_plane(const v* in_vertexes,
    int in_vertex_count, frustrum_plane plane, v* out_vertexes, int* out_vertex_count) {
    if (in_vertexes == NULL || out_vertexes == NULL || out_vertex_count == NULL) {
        return LSRERR_INVALID_ARGUMENT;
    }

    if (in_vertex_count < PRIMITIVE_TRIANGLE_VERTEX_COUNT) {
        *out_vertex_count = 0;
        return LSRERR_OK;
    }

    int count = 0;
    int result = LSRERR_OK;
    const v* s = &in_vertexes[in_vertex_count - 1]; // Start with the last vertex

    for (int i = 0; i < in_vertex_count; i++) {
        const v* p = &in_vertexes[i];

        int s_outside = FALSE, p_outside = FALSE;
        if ((result = frustrum_is_outside_plane(&s->position, plane, &s_outside)) != LSRERR_OK) {
            return result;
        }

        if ((result = frustrum_is_outside_plane(&p->position, plane, &p_outside)) != LSRERR_OK) {
            return result;
        }

        if (p_outside) {
            if (!s_outside) {
                // Case 1: Going Inside -> Outside (output intersection point)
                f32 d1 = 0.0f, d2 = 0.0f;
                if ((result = frustrum_get_plane_distance(&s->position, plane, &d1)) != LSRERR_OK) {
                    return result;
                }

                if ((result = frustrum_get_plane_distance(&p->position, plane, &d2)) != LSRERR_OK) {
                    return result;
                }

                if ((result = vertex_interpolate(&out_vertexes[count++], s, p, d1 / (d1 - d2))) != LSRERR_OK) {
                    return result;
                }
            }
            // Case 2: Both Outside (output nothing)
        }
        else {
            if (s_outside) {
                // Case 3: Going Outside -> Inside (output intersection + current point)
                f32 d1 = 0.0f, d2 = 0.0f;
                if ((result = frustrum_get_plane_distance(&s->position, plane, &d1)) != LSRERR_OK) {
                    return result;
                }

                if ((result = frustrum_get_plane_distance(&p->position, plane, &d2)) != LSRERR_OK) {
                    return result;
                }

                if ((result = vertex_interpolate(&out_vertexes[count++], s, p, d1 / (d1 - d2))) != LSRERR_OK) {
                    return result;
                }
            }

            // Case 4: Both Inside (output current point)
            CopyMemory(&out_vertexes[count++], &p[0], sizeof(v));
        }

        s = p; // Move to next edge
    }

    *out_vertex_count = count;

    return result;
}

int clip_line_against_plane(v* v0, v* v1, frustrum_plane plane) { // TODO API standardization

    int result = LSRERR_OK;

    f32 d0 = 0.0f, d1 = 0.0f; // TODO Loop
    if ((result = frustrum_get_plane_distance(&v0->position, plane, &d0)) != LSRERR_OK) {
        return result;
    }

    if ((result = frustrum_get_plane_distance(&v1->position, plane, &d1)) != LSRERR_OK) {
        return result;
    }

    // Both points are strictly outside the plane -> Trivial Rejection
    if (d0 < 0.0f && d1 < 0.0f) return FALSE;

    // Both points are inside the plane -> Entirely saved, no clipping needed
    if (d0 >= 0.0f && d1 >= 0.0f) return TRUE;

    // One point is inside, one is outside. Calculate the interpolation factor 't'.
    // Because the signs are flipped, the ratio formula changes slightly to handle 
    // the span correctly without resulting in a negative 't'.
    const f32 t = d0 / (d0 - d1);

    // Create an interpolated intersection vertex
    v intersected;
    ZeroMemory(&intersected, sizeof(v));

    if ((result = vertex_interpolate(&intersected, v0, v1, t)) != LSRERR_OK) {
        return result;
    }

    // Replace the endpoint that was outside (negative distance) with intersection point
    if (d0 < 0.0f) {
        *v0 = intersected; // v0 was outside, clip it
    }
    else {
        *v1 = intersected; // v1 was outside, clip it
    }

    return TRUE;
}

int render_get_fog_factor(const render* r, f32 depth, f32* factor) {
    if (r == NULL || factor == NULL) {
        return LSRERR_INVALID_ARGUMENT;
    }

    switch (r->settings.fog.mode) {
    case RENDER_FOG_LINEAR: {
        const f32 distance = r->settings.fog.end - r->settings.fog.start;

        if (r->settings.fog.end <= 0.0f || fabsf(distance) < EPSILON) {
            *factor = 0.0f;
            return LSRERR_OK;
        }

        *factor = clampf((r->settings.fog.end - depth) / distance, 0.0f, 1.0f);
    } break;
    case RENDER_FOG_EXPONENTIAL: {
        *factor = clampf(expf(-r->settings.fog.density * depth), 0.0f, 1.0f);
    } break;
    case RENDER_FOG_EXPONENTIAL_SQUARED: {
        const f32 dd = r->settings.fog.density * depth;
        *factor = clampf(expf(-(dd * dd)), 0.0f, 1.0f);
    } break;
    default: {
        *factor = 0.0f;
    } break;
    }

    return LSRERR_OK;
}

int vertex_shader_stage(render* r, const f32m4* wvp,
    const vertex* in_vertexes, const int* in_indexes, size_t in_index_count,
    v** out_vertexes, int** out_indexes, size_t* out_index_count) {
    if (r == NULL || in_vertexes == NULL || in_indexes == NULL
        || out_vertexes == NULL || out_indexes == NULL || out_index_count == NULL) {
        return LSRERR_INVALID_ARGUMENT;
    }

    if (in_index_count > RENDER_MAX_VERTEX_COUNT) {
        return LSRERR_INVALID_ARGUMENT;
    }

    // Allocate memory
    v* vertexes = (v*)arena_allocate(r->arena, in_index_count * sizeof(v));
    if (vertexes == NULL) {
        return LSRERR_OUT_OF_MEMORY;
    }

    int* indexes = (int*)arena_allocate(r->arena, in_index_count * sizeof(int));
    if (indexes == NULL) {
        return LSRERR_OUT_OF_MEMORY;
    }

    CopyMemory(indexes, in_indexes, in_index_count * sizeof(int));

    int result = LSRERR_OK;
    size_t count = 0;

    // Transform vertex positions to clip space
    for (size_t i = 0; i < in_index_count; i++) {
        const vertex* in = &in_vertexes[in_indexes[i]];
        v* vertex = &vertexes[count++];

        f32x4 position;
        position.x = in->position.x;
        position.y = in->position.y;
        position.z = in->position.z;
        position.w = 1.0f;

        if ((result = f32x4_multiply_f32m4(&position, wvp, &vertex->position)) != LSRERR_OK) {
            return result;
        }

        CopyMemory(&vertex->normal, &in->normal, sizeof(f32x3));
        CopyMemory(&vertex->uv, &in->uv, sizeof(f32x2));

        if ((result = f32x4_set_color(&vertex->color, in->color)) != LSRERR_OK) {
            return result;
        }
    }

    *out_vertexes = vertexes;
    *out_indexes = indexes;
    *out_index_count = count;

    return result;
}

int geometry_shader_point_stage(render* r,
    const v* in_vertexes, const int* in_indexes, size_t in_index_count,
    v** out_vertexes, size_t* out_vertex_count) {
    if (r == NULL || in_vertexes == NULL || in_indexes == NULL
        || out_vertexes == NULL || out_vertex_count == NULL) {
        return LSRERR_INVALID_ARGUMENT;
    }

    // Allocate memory
    v* vertexes = (v*)arena_allocate(r->arena, in_index_count * sizeof(v));
    if (vertexes == NULL) {
        return LSRERR_OUT_OF_MEMORY;
    }

    int result = LSRERR_OK;
    size_t count = 0;

    for (size_t i = 0; i < in_index_count; i++) {
        const v* in = &in_vertexes[in_indexes[i]];

        int discard = FALSE;
        if (r->settings.clipping == RENDER_CLIPPING_ENABLED) {
            for (frustrum_plane plane = 0; plane < FRUSTRUM_PLANE_COUNT; plane++) {
                if ((result = frustrum_is_outside_plane(&in->position, plane, &discard)) != LSRERR_OK) {
                    return result;
                }

                if (discard) {
                    break;
                }
            }
        }

        if (!discard) {
            v* vertex = &vertexes[count++];
            CopyMemory(vertex, in, sizeof(v));

            if ((result = f32x4_perspective_divide(&vertex->position)) != LSRERR_OK) {
                return result;
            }
        }
    }

    *out_vertexes = vertexes;
    *out_vertex_count = count;

    return LSRERR_OK;
}

int fragment_shader_point_stage(render* r,
    const v* in_vertexes, size_t in_vertex_count) {
    if (r == NULL || in_vertexes == NULL) {
        return LSRERR_INVALID_ARGUMENT;
    }

    int result = LSRERR_OK;

    // Transform vertexes NDC coordinates to "screen" coordinates.
    // And adjusting to [0, ...) space, so that there are no pixels outside target texture.
    texture* s = r->surface;
    const int width = s->width - 1;
    const int height = s->height - 1;

    for (size_t i = 0; i < in_vertex_count; i++) {
        const v* vertex = &in_vertexes[i];

        const int x = (int)((f32)width * ((vertex->position.x + 1.0f) / 2.0f));
        const int y = (int)((f32)height * ((1.0f - vertex->position.y) / 2.0f));

        if (x >= 0 && x < s->width && y >= 0 && y < s->height) {
            int draw = TRUE;
            const f32 interpolated_depth =
                r->settings.depth == RENDER_DEPTH_BUFFER_Z
                ? vertex->position.z : vertex->position.w;

            if (r->settings.depth != RENDER_DEPTH_BUFFER_NONE) {
                f32 depth = INFINITY;
                if ((result = texture_get_point_depth(r->depth, x, y, &depth)) != LSRERR_OK) {
                    return result;
                }

                draw = interpolated_depth < depth;
            }

            if (draw) {
                f32x4 color;
                CopyMemory(&color, &vertex->color, sizeof(f32x4));

                int xc = 0, yc = 0;

                // Sample texture to get pixel color
                if (r->current != NULL) {
                    xc = (int)((f32)(r->current->width - 1) * vertex->uv.x);
                    yc = (int)((f32)(r->current->height - 1) * vertex->uv.y);
                }

                if ((result = rasterize_pixel(r, x, y, xc, yc,
                    vertex->position.z, interpolated_depth, &color)) != LSRERR_OK) {
                    return result;
                }
            }
        }
    }

    return result;
}

int rasterize_pixel(render* r, int x, int y, int tx, int ty, f32 fog, f32 depth, const f32x4* pixel) {
    if (r == NULL || pixel == NULL) {
        return LSRERR_INVALID_ARGUMENT;
    }

    f32x4 color;
    CopyMemory(&color, pixel, sizeof(f32x4));

    int result = LSRERR_OK;

    if (r->current != NULL) {
        u32 sample_color = 0;
        if ((result = texture_get_point_color(r->current, tx, ty, &sample_color)) != LSRERR_OK) {
            return result;
        }

        f32x4 texture_color;
        if ((result = f32x4_set_color(&texture_color, sample_color)) != LSRERR_OK) {
            return result;
        }

        // Blend vertex and texture colors
        color.a *= texture_color.a;
        color.r *= texture_color.r;
        color.g *= texture_color.g;
        color.b *= texture_color.b;
    }

    if (r->settings.fog.mode != RENDER_FOG_NONE) {
        f32 factor = 0.0f;
        if ((result = render_get_fog_factor(r, fog, &factor)) != LSRERR_OK) {
            return result;
        }

        if ((result = f32x4_interpolate(&color, &r->settings.fog.color, factor)) != LSRERR_OK) {
            return result;
        }
    }

    if (r->settings.blending == RENDER_BLENDING_ENABLED) {
        u32 sample_color = 0;
        if ((result = texture_get_point_color(r->surface, x, y, &sample_color)) != LSRERR_OK) {
            return result;
        }

        f32x4 texture_color;
        if ((result = f32x4_set_color(&texture_color, sample_color)) != LSRERR_OK) {
            return result;
        }

        if ((result = f32x4_blend_color(&color, &texture_color, &color)) != LSRERR_OK) {
            return result;
        }
    }

    u32 final_color = 0;
    if ((result = f32x4_get_color(&color, &final_color)) != LSRERR_OK) {
        return result;
    }

    // Draw pixel and update depth buffer if needed
    if ((result = texture_set_point_color(r->surface, x, y, final_color)) == LSRERR_OK) {
        if (r->settings.depth != RENDER_DEPTH_BUFFER_NONE) {
            if ((result = texture_set_point_depth(r->depth, x, y, depth)) != LSRERR_OK) {
                return result;
            }
        }
    }

    return LSRERR_OK;
}

int geometry_shader_line_stage(render* r,
    const v* in_vertexes, const int* in_indexes, size_t in_index_count,
    v** out_vertexes, size_t* out_vertex_count) {
    if (r == NULL || in_vertexes == NULL || in_indexes == NULL
        || out_vertexes == NULL || out_vertex_count == NULL) {
        return LSRERR_INVALID_ARGUMENT;
    }

    if (in_index_count > RENDER_MAX_VERTEX_COUNT
        || (in_index_count % PRIMITIVE_LINE_VERTEX_COUNT) != 0) {
        return LSRERR_INVALID_ARGUMENT;
    }

    v* vertexes = (v*)arena_allocate(r->arena, in_index_count * sizeof(v));
    if (vertexes == NULL) {
        return LSRERR_OUT_OF_MEMORY;
    }

    int result = LSRERR_OK;
    size_t count = 0;
    v line[PRIMITIVE_LINE_VERTEX_COUNT];

    for (size_t i = 0; i < in_index_count; i += PRIMITIVE_LINE_VERTEX_COUNT) {
        for (size_t x = 0; x < PRIMITIVE_LINE_VERTEX_COUNT; x++) {
            CopyMemory(&line[x], &in_vertexes[in_indexes[i + x]], sizeof(v));
        }

        int discard = FALSE;

        // Trivial rejection optimization:
        // Skip the line if both endpoints are out of any single plane.
        for (frustrum_plane plane = 0; plane < FRUSTRUM_PLANE_COUNT; plane++) {
            int all = TRUE, outside = FALSE;
            for (size_t x = 0; x < PRIMITIVE_LINE_VERTEX_COUNT; x++) {
                if ((result = frustrum_is_outside_plane(&line[x].position, plane, &outside)) != LSRERR_OK) {
                    return result;
                }

                all = all && outside;
            }

            if (all) {
                discard = TRUE;
                break;
            }
        }

        if (!discard) {
            if (r->settings.clipping == RENDER_CLIPPING_ENABLED) {
                // Sequentially clip the line segment against all 6 frustum planes.
                for (frustrum_plane plane = 0; plane < FRUSTRUM_PLANE_COUNT; plane++) {
                    if (!clip_line_against_plane(&line[0], &line[1], plane)) {
                        discard = TRUE;
                        break;
                    }
                }
            }

            if (!discard) {
                for (int x = 0; x < PRIMITIVE_LINE_VERTEX_COUNT; x++) {
                    v* vertex = &vertexes[count++];
                    CopyMemory(vertex, &line[x], sizeof(v));

                    if ((result = f32x4_perspective_divide(&vertex->position)) != LSRERR_OK) {
                        return result;
                    }
                }
            }
        }
    }

    *out_vertexes = vertexes;
    *out_vertex_count = count;

    return result;
}

int fragment_shader_line_stage(render* r,
    const v* in_vertexes, size_t in_vertex_count) {
    if (r == NULL || in_vertexes == NULL) {
        return LSRERR_INVALID_ARGUMENT;
    }

    texture* s = r->surface;
    const int width = s->width - 1;
    const int height = s->height - 1;

    int result = LSRERR_OK;

    for (size_t k = 0; k < in_vertex_count; k += PRIMITIVE_LINE_VERTEX_COUNT) {
        const v* v0 = &in_vertexes[k + 0];
        const v* v1 = &in_vertexes[k + 1];

        // Map vertexes to target (screen) coordinates
        const f32 x0_f = ((f32)width * ((v0->position.x + 1.0f) / 2.0f));
        const f32 y0_f = ((f32)height * ((1.0f - v0->position.y) / 2.0f));
        const f32 x1_f = ((f32)width * ((v1->position.x + 1.0f) / 2.0f));
        const f32 y1_f = ((f32)height * ((1.0f - v1->position.y) / 2.0f));

        // Convert endpoints to exact integer pixel coordinates
        const int x0 = (int)floorf(x0_f + 0.5f);
        const int y0 = (int)floorf(y0_f + 0.5f);
        const int x1 = (int)floorf(x1_f + 0.5f);
        const int y1 = (int)floorf(y1_f + 0.5f);

        // Calculate absolute delta steps
        const int dx = abs(x1 - x0);
        const int dy = abs(y1 - y0);

        // Determine the total step count based on the dominant major axis
        const int steps = (dx > dy) ? dx : dy;
        if (steps == 0) {
            // Line coordinates occupy the exact same discrete pixel
            continue;
        }

        // Perspective-Correct Interpolation Setup
        const f32 w0_inv = 1.0f / v0->position.w;
        const f32 w1_inv = 1.0f / v1->position.w;

        // Attribute Pre-division by W
        const f32x4 w0_color = { v0->color.a * w0_inv, v0->color.r * w0_inv, v0->color.g * w0_inv, v0->color.b * w0_inv };
        const f32x4 w1_color = { v1->color.a * w1_inv, v1->color.r * w1_inv, v1->color.g * w1_inv, v1->color.b * w1_inv };
        const f32 u0_w = v0->uv.x * w0_inv, v0_w = v0->uv.y * w0_inv;
        const f32 u1_w = v1->uv.x * w1_inv, v1_w = v1->uv.y * w1_inv;

        // Direct Digital Differential Analyzer (DDA) 1D Step Traversal
        for (int i = 0; i <= steps; i++) {
            // Compute current linear interpolation fraction factor 't' along the sequence
            const f32 t = (f32)i / (f32)steps;

            // Linearly interpolate discrete pixel coordinates matching your precision rules
            const int x = (int)floorf(x0_f + t * (x1_f - x0_f) + 0.5f);
            const int y = (int)floorf(y0_f + t * (y1_f - y0_f) + 0.5f);

            // Hardware Scissor Guard / Bounds Clip Check
            if (x < 0 || x > width || y < 0 || y > height) {
                continue;
            }

            // Interpolate 1/W for perspective attributes
            const f32 interpolated_inv_w = w0_inv + t * (w1_inv - w0_inv);
            const f32 current_w = 1.0f / interpolated_inv_w;
            const f32 current_z = (v0->position.z + t * (v1->position.z - v0->position.z)); // TODO double check

            int draw = TRUE;
            f32 interpolated_depth = INFINITY;

            // Depth interpolation & depth check
            if (r->settings.depth != RENDER_DEPTH_BUFFER_NONE) {
                f32 depth = INFINITY;
                if ((result = texture_get_point_depth(r->depth, x, y, &depth)) != LSRERR_OK) {
                    return result;
                }

                interpolated_depth =
                    r->settings.depth == RENDER_DEPTH_BUFFER_Z ? current_z : current_w;

                draw = interpolated_depth < depth;
            }

            if (draw) {
                // Perspective correct attribute interpolation
                f32x4 color;
                color.a = (w0_color.a + t * (w1_color.a - w0_color.a)) * current_w;
                color.r = (w0_color.r + t * (w1_color.r - w0_color.r)) * current_w;
                color.g = (w0_color.g + t * (w1_color.g - w0_color.g)) * current_w;
                color.b = (w0_color.b + t * (w1_color.b - w0_color.b)) * current_w;

                int xc = 0, yc = 0;

                // Sample texture to get pixel color
                if (r->current != NULL) {
                    const f32 u = (u0_w + t * (u1_w - u0_w)) * current_w;
                    const f32 v = (v0_w + t * (v1_w - v0_w)) * current_w;

                    xc = (int)((f32)(r->current->width - 1) * u);
                    yc = (int)((f32)(r->current->height - 1) * v);
                }

                if ((result = rasterize_pixel(r, x, y, xc, yc, current_z, interpolated_depth, &color)) != LSRERR_OK) {
                    return result;
                }
            }
        }
    }

    return result;
}

int geometry_shader_triangle_stage(render* r,
    const v* in_vertexes, const int* in_indexes, size_t in_index_count,
    v** out_vertexes, size_t* out_vertex_count) {
    if (r == NULL || in_vertexes == NULL || in_indexes == NULL
        || out_vertexes == NULL || out_vertex_count == NULL) {
        return LSRERR_INVALID_ARGUMENT;
    }

    if (in_index_count > RENDER_MAX_VERTEX_COUNT
        || (in_index_count % PRIMITIVE_TRIANGLE_VERTEX_COUNT) != 0) {
        return LSRERR_INVALID_ARGUMENT;
    }

    v* vertexes = (v*)arena_allocate(r->arena, 6 * in_index_count * sizeof(v));
    if (vertexes == NULL) {
        return LSRERR_OUT_OF_MEMORY;
    }

    int result = LSRERR_OK;
    size_t count = 0;
    v triangle[PRIMITIVE_TRIANGLE_VERTEX_COUNT];

    for (size_t i = 0; i < in_index_count; i += PRIMITIVE_TRIANGLE_VERTEX_COUNT) {
        for (size_t x = 0; x < PRIMITIVE_TRIANGLE_VERTEX_COUNT; x++) {
            CopyMemory(&triangle[x], &in_vertexes[in_indexes[i + x]], sizeof(v));
        }

        int discard = FALSE;

        // Trivial rejection optimization:
        // Skip the triangle if all 3 vertexes are out of any single plane.
        for (frustrum_plane plane = 0; plane < FRUSTRUM_PLANE_COUNT; plane++) {
            int all = TRUE, outside = FALSE;
            for (size_t x = 0; x < PRIMITIVE_TRIANGLE_VERTEX_COUNT; x++) {
                if ((result = frustrum_is_outside_plane(&triangle[x].position, plane, &outside)) != LSRERR_OK) {
                    return result;
                }

                all = all && outside;
            }

            if (all) {
                discard = TRUE;
                break;
            }
        }

        if (!discard) {
            if (r->settings.clipping == RENDER_CLIPPING_ENABLED) {
                // Buffer spaces to swap lists between plane clipping passes
                // Maximum vertexes generated = 12
                v buffer_a[12];
                v buffer_b[12];

                // Initialize input buffer with original triangle vertexes
                CopyMemory(buffer_a, triangle, PRIMITIVE_TRIANGLE_VERTEX_COUNT * sizeof(v));
                int current_vertex_count = PRIMITIVE_TRIANGLE_VERTEX_COUNT;

                // Sequentially clip against all 6 frustum planes
                for (frustrum_plane plane = 0; plane < FRUSTRUM_PLANE_COUNT; plane++) {
                    if (plane % 2 == 0) {
                        if ((result = clip_polygon_against_plane(buffer_a,
                            current_vertex_count, plane, buffer_b, &current_vertex_count)) != LSRERR_OK) {
                            return result;
                        }
                    }
                    else {
                        if ((result = clip_polygon_against_plane(buffer_b,
                            current_vertex_count, plane, buffer_a, &current_vertex_count)) != LSRERR_OK) {
                            return result;
                        }
                    }

                    // Check if the primitive is completely wiped out
                    if (current_vertex_count < PRIMITIVE_TRIANGLE_VERTEX_COUNT) {
                        break;
                    }
                }

                // Target buffer depends on whether plane loop ended on odd/even step
                const v* final_polygon =
                    (FRUSTRUM_PLANE_COUNT % PRIMITIVE_LINE_VERTEX_COUNT == 0) ? buffer_a : buffer_b;

                // Triangulate resulting convex polygon using a clean Triangle Fan arrangement
                for (int vert = 1; vert < current_vertex_count - 1; vert++) {
                    v split_triangle[PRIMITIVE_TRIANGLE_VERTEX_COUNT];
                    split_triangle[0] = final_polygon[0];
                    split_triangle[1] = final_polygon[vert];
                    split_triangle[2] = final_polygon[vert + 1];

                    for (size_t x = 0; x < PRIMITIVE_TRIANGLE_VERTEX_COUNT; x++) {
                        if ((result = f32x4_perspective_divide(&split_triangle[x].position)) != LSRERR_OK) {
                            return result;
                        }
                    }

                    switch (r->settings.fill) {
                    case RENDER_FILL_POINT:
                    case RENDER_FILL_SOLID: {
                        for (size_t x = 0; x < PRIMITIVE_TRIANGLE_VERTEX_COUNT; x++) {
                            CopyMemory(&vertexes[count++], &split_triangle[x], sizeof(v));
                        }

                    } break;
                    case RENDER_FILL_WIRE: {
                        CopyMemory(&vertexes[count++], &split_triangle[0], sizeof(v));
                        CopyMemory(&vertexes[count++], &split_triangle[1], sizeof(v));
                        CopyMemory(&vertexes[count++], &split_triangle[1], sizeof(v));
                        CopyMemory(&vertexes[count++], &split_triangle[2], sizeof(v));
                        CopyMemory(&vertexes[count++], &split_triangle[2], sizeof(v));
                        CopyMemory(&vertexes[count++], &split_triangle[0], sizeof(v));
                    } break;
                    }
                }
            }
            else {
                for (size_t x = 0; x < PRIMITIVE_TRIANGLE_VERTEX_COUNT; x++) {
                    if ((result = f32x4_perspective_divide(&triangle[x].position)) != LSRERR_OK) {
                        return result;
                    }
                }

                switch (r->settings.fill) {
                case RENDER_FILL_POINT:
                case RENDER_FILL_SOLID: {
                    for (size_t x = 0; x < PRIMITIVE_TRIANGLE_VERTEX_COUNT; x++) {
                        CopyMemory(&vertexes[count++], &triangle[x], sizeof(v));
                    }

                } break;
                case RENDER_FILL_WIRE: {
                    CopyMemory(&vertexes[count++], &triangle[0], sizeof(v));
                    CopyMemory(&vertexes[count++], &triangle[1], sizeof(v));
                    CopyMemory(&vertexes[count++], &triangle[1], sizeof(v));
                    CopyMemory(&vertexes[count++], &triangle[2], sizeof(v));
                    CopyMemory(&vertexes[count++], &triangle[2], sizeof(v));
                    CopyMemory(&vertexes[count++], &triangle[0], sizeof(v));
                } break;
                }
            }
        }
    }

    *out_vertexes = vertexes;
    *out_vertex_count = count;

    return result;
}

int fragment_shader_triangle_stage(render* r,
    const v* in_vertexes, size_t in_vertex_count) {
    if (r == NULL || in_vertexes == NULL) {
        return LSRERR_INVALID_ARGUMENT;
    }

    texture* s = r->surface;
    const int width = s->width - 1;
    const int height = s->height - 1;

    int result = LSRERR_OK;

    for (size_t k = 0; k < in_vertex_count; k += PRIMITIVE_TRIANGLE_VERTEX_COUNT) {
        const v* v0 = &in_vertexes[k + 0];
        const v* v1 = &in_vertexes[k + 1];
        const v* v2 = &in_vertexes[k + 2];

        // Map vertexes to target (screen) coordinates
        const f32 x0 = ((f32)width * ((v0->position.x + 1.0f) / 2.0f));
        const f32 y0 = ((f32)height * ((1.0f - v0->position.y) / 2.0f));
        const f32 x1 = ((f32)width * ((v1->position.x + 1.0f) / 2.0f));
        const f32 y1 = ((f32)height * ((1.0f - v1->position.y) / 2.0f));
        const f32 x2 = ((f32)width * ((v2->position.x + 1.0f) / 2.0f));
        const f32 y2 = ((f32)height * ((1.0f - v2->position.y) / 2.0f));

        // Compute area of the triangle
        const f32 signed_area = (x1 - x0) * (y2 - y0) - (y1 - y0) * (x2 - x0);

        if (r->settings.culling == RENDER_CULLING_NONE) {
            if (fabsf(signed_area) <= EPSILON) {
                continue;
            }
        }
        else if (r->settings.culling == RENDER_CULLING_CCW) {
            if (signed_area < EPSILON) {
                continue;
            }
        }
        else if (r->settings.culling == RENDER_CULLING_CW) {
            if (signed_area > -EPSILON) {
                continue;
            }
        }

        const f32 inv_area = 1.0f / fabsf(signed_area);

        // Compute triangle bounding box
        const f32 min_x = min(x0, min(x1, x2));
        const f32 max_x = max(x0, max(x1, x2));
        const f32 min_y = min(y0, min(y1, y2));
        const f32 max_y = max(y0, max(y1, y2));

        // Clamp to the target (screen) boundaries
        const int start_x = (int)max(0, floorf(min_x));
        const int end_x = (int)min(width - 1, ceilf(max_x));
        const int start_y = (int)max(0, floorf(min_y));
        const int end_y = (int)min(height - 1, ceilf(max_y));

        // Perspective-Correct Interpolation
        const f32 w0_inv = 1.0f / v0->position.w;
        const f32 w1_inv = 1.0f / v1->position.w;
        const f32 w2_inv = 1.0f / v2->position.w;

        // Color interpolation
        const f32x4 w0_color = { v0->color.a * w0_inv, v0->color.r * w0_inv, v0->color.g * w0_inv, v0->color.b * w0_inv };
        const f32x4 w1_color = { v1->color.a * w1_inv, v1->color.r * w1_inv, v1->color.g * w1_inv, v1->color.b * w1_inv };
        const f32x4 w2_color = { v2->color.a * w2_inv, v2->color.r * w2_inv, v2->color.g * w2_inv, v2->color.b * w2_inv };

        // Texture coordinate interpolation
        const f32 u0_w = v0->uv.x * w0_inv, v0_w = v0->uv.y * w0_inv;
        const f32 u1_w = v1->uv.x * w1_inv, v1_w = v1->uv.y * w1_inv;
        const f32 u2_w = v2->uv.x * w2_inv, v2_w = v2->uv.y * w2_inv;

        // Depth interpolation
        const f32 z0_w = v0->position.z * w0_inv;
        const f32 z1_w = v1->position.z * w1_inv;
        const f32 z2_w = v2->position.z * w2_inv;

        // Loop over every pixel in the bounding box
        for (int y = start_y; y <= end_y; y++) {
            for (int x = start_x; x <= end_x; x++) {
                // Pixel center coordinate
                const f32x2 p = { (f32)x + 0.5f, (f32)y + 0.5f };

                // Edge functions (Barycentric weights)
                f32 w0 = ((x2 - x1) * (p.y - y1) - (y2 - y1) * (p.x - x1)) * inv_area; // Alternative name: alpha
                f32 w1 = ((x0 - x2) * (p.y - y2) - (y0 - y2) * (p.x - x2)) * inv_area; // Alternative name: beta
                f32 w2 = ((x1 - x0) * (p.y - y0) - (y1 - y0) * (p.x - x0)) * inv_area; // Alternative name: gamma

                // Flip the signs so containment check works universally
                if (signed_area < 0.0f) {
                    w0 = -w0; w1 = -w1; w2 = -w2;
                }

                // If pixel center is inside all three edges
                int fill = w0 >= 0.0f && w1 >= 0.0f && w2 >= 0.0f;
                switch (r->settings.fill) {
                case RENDER_FILL_POINT: {
                    fill = (floorf(x0) == (f32)x && floorf(y0) == (f32)y)
                        || (floorf(x1) == (f32)x && floorf(y1) == (f32)y)
                        || (floorf(x2) == (f32)x && floorf(y2) == (f32)y);
                } break;
                case RENDER_FILL_WIRE: {
                    if (fill) {
                        const f32 thickness = 0.02f;
                        fill = w0 < thickness || w1 < thickness || w2 < thickness;
                    }
                } break;
                }

                // Compute and draw pixel
                if (fill) {
                    // Interpolate 1/W for depth testing and perspective correction
                    const f32 interpolated_inv_w = w0 * w0_inv + w1 * w1_inv + w2 * w2_inv;
                    const f32 current_w = 1.0f / interpolated_inv_w; // True linear depth
                    const f32 current_z = (w0 * v0->position.z + w1 * v1->position.z + w2 * v2->position.z) * current_w;

                    int draw = TRUE;
                    f32 interpolated_depth = INFINITY;

                    // Depth interpolation & depth check
                    if (r->settings.depth != RENDER_DEPTH_BUFFER_NONE) {
                        f32 depth = INFINITY;
                        if ((result = texture_get_point_depth(r->depth, x, y, &depth)) != LSRERR_OK) {
                            return result;
                        }

                        interpolated_depth =
                            r->settings.depth == RENDER_DEPTH_BUFFER_Z ? current_z : current_w;

                        draw = interpolated_depth < depth;
                    }

                    if (draw) {
                        // Perspective correct attribute interpolation
                        f32x4 color;
                        color.a = (w0 * w0_color.a + w1 * w1_color.a + w2 * w2_color.a) * current_w;
                        color.r = (w0 * w0_color.r + w1 * w1_color.r + w2 * w2_color.r) * current_w;
                        color.g = (w0 * w0_color.g + w1 * w1_color.g + w2 * w2_color.g) * current_w;
                        color.b = (w0 * w0_color.b + w1 * w1_color.b + w2 * w2_color.b) * current_w;

                        int xc = 0, yc = 0;

                        // Sample texture to get pixel color
                        if (r->current != NULL) {
                            const f32 u = (w0 * u0_w + w1 * u1_w + w2 * u2_w) * current_w;
                            const f32 v = (w0 * v0_w + w1 * v1_w + w2 * v2_w) * current_w;

                            xc = (int)((f32)(r->current->width - 1) * u);
                            yc = (int)((f32)(r->current->height - 1) * v);
                        }

                        if ((result = rasterize_pixel(r, x, y, xc, yc, current_z, interpolated_depth, &color)) != LSRERR_OK) {
                            return result;
                        }
                    }
                }
            }
        }
    }

    return result;
}
