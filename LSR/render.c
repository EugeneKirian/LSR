#include "arena.h"
#include "render.h"

#include <float.h>
#include <math.h>
#include <stdlib.h>

#define RENDER_MAX_VERTEX_COUNT 65535

typedef struct rv {
    f32x4 position;
    f32x3 normal;
    f32x4 color;
    f32x2 uv;
} rv;

#define RENDER_MAX_ARENA_SIZE (4 * RENDER_MAX_VERTEX_COUNT * (sizeof(rv) + sizeof(int)))

struct render {
    int active;
    arena* arena;
    struct {
        render_blending blending;
        render_clipping clipping;
        render_culling culling;
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

typedef enum frustrum_plane {
    FRUSTRUM_PLANE_LEFT         = 0,
    FRUSTRUM_PLANE_RIGHT        = 1,
    FRUSTRUM_PLANE_TOP          = 2,
    FRUSTRUM_PLANE_BOTTOM       = 3,
    FRUSTRUM_PLANE_NEAR         = 4,
    FRUSTRUM_PLANE_FAR          = 5,
    FRUSTRUM_PLANE_COUNT        = 6,
    FRUSTRUM_PLANE_FORCE_DWORD  = 0x7FFFFFFF,
} frustrum_plane;

static int viewport_is_valid(const render_viewport* vp);

static int clip_polygon_against_plane(const rv* in_vertices, int in_count,
    frustrum_plane plane, rv* out_vertices);

static int frustrum_is_outside_plane(const rv* v, frustrum_plane plane);
static f32 frustrum_get_plane_distance(const rv* v, frustrum_plane plane);

static int render_get_fog_factor(const render* r, f32 depth, f32* factor);

static int render_points(render* r, const f32m4* wvp,
    const vertex* vertexes, const int* indexes, size_t index_count);
static int render_triangles(render* r, const f32m4* wvp,
    const vertex* vertexes, const int* indexes, size_t index_count);
static int render_rasterize_triangle(render* r, const rv* vertexes);

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

    int result = LSRERR_OK;
    if ((result = f32m4_multiply(&r->matrixes[RENDER_MATRIX_WORLD],
        &r->matrixes[RENDER_MATRIX_VIEW], &wv)) == LSRERR_OK) {
        if ((result = f32m4_multiply(&wv,
            &r->matrixes[RENDER_MATRIX_PROJECTION], &wvp)) == LSRERR_OK) {
            switch (r->settings.mode) {
            case RENDER_DRAW_MODE_POINTS: {
                return render_points(r, &wvp, vertexes, indexes, index_count);
            } break;
            case RENDER_DRAW_MODE_TRIANGLES: {
                return render_triangles(r, &wvp, vertexes, indexes, index_count);
            } break;
            }
        }
    }

    return result;
}

int render_points(render* r, const f32m4* wvp,
    const vertex* vertexes, const int* indexes, size_t index_count) {
    if (r == NULL || vertexes == NULL || indexes == NULL) {
        return LSRERR_INVALID_ARGUMENT;
    }

    if (index_count > RENDER_MAX_VERTEX_COUNT) {
        return LSRERR_INVALID_ARGUMENT;
    }

    const size_t size = sizeof(rv) * index_count;
    rv* converted = (rv*)arena_allocate(r->arena, size);
    if (converted == NULL) {
        return LSRERR_OUT_OF_MEMORY;
    }

    ZeroMemory(converted, size);

    int result = LSRERR_OK;
    size_t converted_count = 0;

    // Transform vertex positions to clip space
    for (size_t i = 0; i < index_count; i++) {
        const vertex* in = &vertexes[indexes[i]];
        rv* out = &converted[converted_count];

        f32x4 v;
        v.x = in->position.x;
        v.y = in->position.y;
        v.z = in->position.z;
        v.w = 1.0f;

        if ((result = f32x4_multiply_f32m4(&v, wvp, &out->position)) != LSRERR_OK) {
            return result;
        }

        if (r->settings.clipping == RENDER_CLIPPING_ENABLED) {
            int outside = FALSE;
            for (size_t p = 0; p < FRUSTRUM_PLANE_COUNT; p++) {
                if (outside = frustrum_is_outside_plane(out, p)) {
                    break;
                }
            }

            if (!outside) {
                converted_count++;
            }
        }
        else {
            converted_count++;
        }

        out->normal = in->normal;
        out->uv = in->uv;

        if ((result == f32x4_set_color(&out->color, in->color)) != LSRERR_OK) {
            return result;
        }
    }

    // Perspective divide: Clip space -> NDC space
    for (size_t i = 0; i < converted_count; i++) {
        rv* v = &converted[i];

        v->position.x /= v->position.w;
        v->position.y /= v->position.w;
        v->position.z /= v->position.w;
    }

    // Transform vertexes NDC coordinates to "screen" coordinates.
    // And adjusting to [0, ...) space, so that there are no pixels outside target texture.
    const texture* c = r->current;
    texture* s = r->surface;
    const int width = s->width - 1;
    const int height = s->height - 1;

    for (size_t i = 0; i < converted_count; i++) {
        rv* v = &converted[i];

        const int x = (int)((f32)width * ((v->position.x + 1.0f) / 2.0f));
        const int y = (int)((f32)height * ((1.0f - v->position.y) / 2.0f));

        if (x >= 0 && x < s->width && y >= 0 && y < s->height) {
            int draw = TRUE;
            if (r->settings.depth != RENDER_DEPTH_BUFFER_NONE) {
                f32 depth = INFINITY;
                if ((result = texture_get_point_depth(r->depth, x, y, &depth)) != LSRERR_OK) {
                    return result;
                }

                draw = r->settings.depth == RENDER_DEPTH_BUFFER_Z
                    ? v->position.z < depth : v->position.w < depth;
            }

            if (draw) {
                f32x4 color;
                CopyMemory(&color, &v->color, sizeof(f32x4));

                // Sample texture to get pixel color
                if (c != NULL) {
                    const f32 uvx = v->uv.x, uvy = v->uv.y;

                    const int xc = (int)((f32)(c->width - 1) * uvx);
                    const int yc = (int)((f32)(c->height - 1) * uvy);

                    u32 sample_color = 0;
                    if ((result = texture_get_point_color(c, xc, yc, &sample_color)) != LSRERR_OK) {
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

                u32 final_color = 0;
                if ((result = f32x4_get_color(&color, &final_color)) != LSRERR_OK) {
                    return result;
                }

                // Draw pixel and update depth buffer if needed
                if ((result = texture_set_point_color(s, x, y, final_color)) == LSRERR_OK) {
                    if (r->settings.depth != RENDER_DEPTH_BUFFER_NONE) {
                        if ((result = texture_set_point_depth(r->depth, x, y,
                            r->settings.depth == RENDER_DEPTH_BUFFER_Z ? v->position.z : v->position.w)) != LSRERR_OK) {
                            return result;
                        }
                    }
                }
            }
        }
    }

    return result;
}

int render_triangles(render* r, const f32m4* wvp,
    const vertex* vertexes, const int* indexes, size_t index_count) {
    if (r == NULL || vertexes == NULL || indexes == NULL) {
        return LSRERR_INVALID_ARGUMENT;
    }

    if (index_count > RENDER_MAX_VERTEX_COUNT || (index_count % 3) != 0) {
        return LSRERR_INVALID_ARGUMENT;
    }

    const size_t size = sizeof(rv) * 2 * index_count;
    rv* converted = (rv*)arena_allocate(r->arena, size);
    if (converted == NULL) {
        return LSRERR_OUT_OF_MEMORY;
    }

    ZeroMemory(converted, size);

    int result = LSRERR_OK;
    size_t converted_count = 0, element_count = 0;

    // Transform vertex positions to clip space
    for (size_t i = 0; i < index_count; i += 3) {
        rv tri[3];

        // 1. Transform Vertices from World into Homogeneous Clip Space via WVP matrix
        for (int v = 0; v < 3; v++) {
            const vertex* src = &vertexes[indexes[i + v]];

            f32x4 pos;
            pos.x = src->position.x;
            pos.y = src->position.y;
            pos.z = src->position.z;
            pos.w = 1.0f;

            if ((result = f32x4_multiply_f32m4(&pos, wvp, &tri[v].position)) != LSRERR_OK) {
                return result;
            }

            tri[v].normal = src->normal;
            tri[v].uv = src->uv;

            if ((result = f32x4_set_color(&tri[v].color, src->color)) != LSRERR_OK) {
                return result;
            }
        }

        // Trivial rejection optimization: If all 3 vertices are out of any single plane, skip!
        int skip_triangle = 0;
        for (int p = 0; p < FRUSTRUM_PLANE_COUNT; p++) {
            if (frustrum_is_outside_plane(&tri[0], p) &&
                frustrum_is_outside_plane(&tri[1], p) &&
                frustrum_is_outside_plane(&tri[2], p)) {
                skip_triangle = 1;
                break;
            }
        }
        if (skip_triangle) continue;

        if (r->settings.clipping == RENDER_CLIPPING_ENABLED) {
            // Buffer spaces to swap lists between plane clipping passes Max vertices generated = 12
            rv buffer_a[12];
            rv buffer_b[12];

            // Initialize input buffer with original triangle vertices
            memcpy(buffer_a, tri, sizeof(rv) * 3);
            int current_vertex_count = 3;

            // Sequentially clip against all 6 frustum planes
            for (int p = 0; p < FRUSTRUM_PLANE_COUNT; p++) {
                if (p % 2 == 0) {
                    current_vertex_count = clip_polygon_against_plane(buffer_a, current_vertex_count, p, buffer_b);
                }
                else {
                    current_vertex_count = clip_polygon_against_plane(buffer_b, current_vertex_count, p, buffer_a);
                }
                if (current_vertex_count < 3) break; // Primitive is completely wiped out
            }

            // Target buffer depends on whether plane loop ended on odd/even step
            const rv* final_polygon = (FRUSTRUM_PLANE_COUNT % 2 == 0) ? buffer_a : buffer_b;

            // Triangulate resulting convex polygon using a clean Triangle Fan arrangement
            rv perspective_triangle[3];
            for (int vert = 1; vert < current_vertex_count - 1; vert++) {
                rv split_triangle[3];
                split_triangle[0] = final_polygon[0];
                split_triangle[1] = final_polygon[vert];
                split_triangle[2] = final_polygon[vert + 1];

                // Perspective Divide (w component must be non-zero)
                for (int v = 0; v < 3; v++) {
                    perspective_triangle[v] = split_triangle[v];

                    perspective_triangle[v].position.x /= split_triangle[v].position.w;
                    perspective_triangle[v].position.y /= split_triangle[v].position.w;
                    perspective_triangle[v].position.z /= split_triangle[v].position.w;
                }

                render_rasterize_triangle(r, perspective_triangle);
            }
        }
        else {
            // If clipping is explicitly turned off, rasterize directly

            // Perspective divide
            for (size_t v = 0; v < 3; v++) {
                tri[v].position.x /= tri[v].position.w;
                tri[v].position.y /= tri[v].position.w;
                tri[v].position.z /= tri[v].position.w;
            }

            render_rasterize_triangle(r, tri);
        }
    }

    return LSRERR_OK;
}

int viewport_is_valid(const render_viewport* vp) {
    return vp->x != 0 || vp->y != 0
        || vp->width != 0 || vp->height != 0;
}

int frustrum_is_outside_plane(const rv* v, frustrum_plane plane) {
    switch (plane) {
    case FRUSTRUM_PLANE_LEFT: return (v->position.x < -v->position.w);
    case FRUSTRUM_PLANE_RIGHT: return (v->position.x > v->position.w);
    case FRUSTRUM_PLANE_TOP: return (v->position.y > v->position.w);
    case FRUSTRUM_PLANE_BOTTOM: return (v->position.y < -v->position.w);
    case FRUSTRUM_PLANE_NEAR: return (v->position.z < 0.0f);
    case FRUSTRUM_PLANE_FAR: return (v->position.z > v->position.w);
    }

    return FALSE;
}

f32 frustrum_get_plane_distance(const rv* v, frustrum_plane plane) {
    switch (plane) {
    case FRUSTRUM_PLANE_LEFT: return v->position.w + v->position.x;
    case FRUSTRUM_PLANE_RIGHT: return v->position.w - v->position.x;
    case FRUSTRUM_PLANE_TOP: return v->position.w - v->position.y;
    case FRUSTRUM_PLANE_BOTTOM: return v->position.w + v->position.y;
    case FRUSTRUM_PLANE_NEAR: return v->position.z;
    case FRUSTRUM_PLANE_FAR: return v->position.w - v->position.z;
    }

    return 0.0f;
}

// Linearly interpolates all standard attributes between two vertices at time t
static rv rv_lerp(const rv* v1, const rv* v2, f32 t) { // TODO
    rv out;
    out.position.x = v1->position.x + t * (v2->position.x - v1->position.x);
    out.position.y = v1->position.y + t * (v2->position.y - v1->position.y);
    out.position.z = v1->position.z + t * (v2->position.z - v1->position.z);
    out.position.w = v1->position.w + t * (v2->position.w - v1->position.w);

    out.normal.x = v1->normal.x + t * (v2->normal.x - v1->normal.x);
    out.normal.y = v1->normal.y + t * (v2->normal.y - v1->normal.y);
    out.normal.z = v1->normal.z + t * (v2->normal.z - v1->normal.z);

    out.uv.x = v1->uv.x + t * (v2->uv.x - v1->uv.x);
    out.uv.y = v1->uv.y + t * (v2->uv.y - v1->uv.y);

    out.color.a = v1->color.a + t * (v2->color.a - v1->color.a);
    out.color.r = v1->color.r + t * (v2->color.r - v1->color.r);
    out.color.g = v1->color.g + t * (v2->color.g - v1->color.g);
    out.color.b = v1->color.b + t * (v2->color.b - v1->color.b);

    return out;
}

static int clip_polygon_against_plane(const rv* in_vertices, int in_count,
    frustrum_plane plane, rv* out_vertices) {
    if (in_count < 3) return 0;

    int out_count = 0;
    const rv* s = &in_vertices[in_count - 1]; // Start with the last vertex

    for (int i = 0; i < in_count; i++) {
        const rv* p = &in_vertices[i];

        const int s_outside = frustrum_is_outside_plane(s, plane);
        const int p_outside = frustrum_is_outside_plane(p, plane);

        if (p_outside) {
            if (!s_outside) {
                // Case 1: Going Inside -> Outside (Output intersection point)
                f32 d1 = frustrum_get_plane_distance(s, plane);
                f32 d2 = frustrum_get_plane_distance(p, plane);
                f32 t = d1 / (d1 - d2);
                out_vertices[out_count++] = rv_lerp(s, p, t);
            }
            // Case 2: Both Outside (Output nothing)
        }
        else {
            if (s_outside) {
                // Case 3: Going Outside -> Inside (Output intersection + current point)
                f32 d1 = frustrum_get_plane_distance(s, plane);
                f32 d2 = frustrum_get_plane_distance(p, plane);
                f32 t = d1 / (d1 - d2);
                out_vertices[out_count++] = rv_lerp(s, p, t);
            }
            // Case 4: Both Inside (Output current point)
            out_vertices[out_count++] = p[0];
        }
        s = p; // Move to next edge
    }
    return out_count;
}

int render_rasterize_triangle(render* r, const rv* vertexes) {
    if (r == NULL || vertexes == NULL) {
        return LSRERR_INVALID_ARGUMENT;
    }

    texture* s = r->surface;
    const int width = s->width - 1;
    const int height = s->height - 1;

    const rv* v0 = &vertexes[0];
    const rv* v1 = &vertexes[1];
    const rv* v2 = &vertexes[2];

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
        if (fabsf(signed_area) <= 1e-5f) {
            return LSRERR_OK;
        }
    }
    else if (r->settings.culling == RENDER_CULLING_CCW) {
        if (signed_area < 1e-5f) {
            return LSRERR_OK;
        }
    }
    else if (r->settings.culling == RENDER_CULLING_CW) {
        if (signed_area > -1e-5f) {
            return LSRERR_OK;
        }
    }

    const f32 area = fabsf(signed_area);
    const f32 inv_area = 1.0f / area;

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

    int result = LSRERR_OK;
    const texture* c = r->current;

    // Loop over every pixel in the bounding box
    for (int y = start_y; y <= end_y; y++) {
        for (int x = start_x; x <= end_x; x++) {
            const f32 px = (float)x + 0.5f; // Pixel center coordinate
            const f32 py = (float)y + 0.5f;

            // Edge functions (Barycentric weights)
            f32 w0 = ((x2 - x1) * (py - y1) - (y2 - y1) * (px - x1)) * inv_area;
            f32 w1 = ((x0 - x2) * (py - y2) - (y0 - y2) * (px - x2)) * inv_area;
            f32 w2 = ((x1 - x0) * (py - y0) - (y1 - y0) * (px - x0)) * inv_area;

            // Flip the signs so containment check works universally
            if (signed_area < 0.0f) {
                w0 = -w0; w1 = -w1; w2 = -w2;
            }

            // If pixel center is inside all three edges
            if (w0 >= 0.0f && w1 >= 0.0f && w2 >= 0.0f) {
                // Interpolate 1/W for depth testing and perspective correction
                const f32 interpolated_inv_w = w0 * w0_inv + w1 * w1_inv + w2 * w2_inv;
                const f32 current_w = 1.0f / interpolated_inv_w; // True linear depth

                int draw = TRUE;
                f32 interpolated_depth = INFINITY;

                // Depth interpolation & depth check
                if (r->settings.depth != RENDER_DEPTH_BUFFER_NONE) {
                    f32 depth = INFINITY;
                    if ((result = texture_get_point_depth(r->depth, x, y, &depth)) != LSRERR_OK) {
                        return result;
                    }

                    interpolated_depth = r->settings.depth == RENDER_DEPTH_BUFFER_Z
                        ? (w0 * v0->position.z + w1 * v1->position.z + w2 * v2->position.z) * current_w
                        : current_w;

                    draw = interpolated_depth < depth;
                }

                // Perspective correct attribute interpolation
                f32x4 color;
                color.a = (w0 * w0_color.a + w1 * w1_color.a + w2 * w2_color.a) * current_w;
                color.r = (w0 * w0_color.r + w1 * w1_color.r + w2 * w2_color.r) * current_w;
                color.g = (w0 * w0_color.g + w1 * w1_color.g + w2 * w2_color.g) * current_w;
                color.b = (w0 * w0_color.b + w1 * w1_color.b + w2 * w2_color.b) * current_w;

                if (draw) {
                    // Sample texture to get pixel color
                    if (c != NULL) {
                        const f32 u = (w0 * u0_w + w1 * u1_w + w2 * u2_w) * current_w;
                        const f32 v = (w0 * v0_w + w1 * v1_w + w2 * v2_w) * current_w;

                        const int xc = (int)((f32)(c->width - 1) * u);
                        const int yc = (int)((f32)(c->height - 1) * v);

                        // Sample texture color
                        u32 sample_color = 0;
                        if ((result = texture_get_point_color(c, xc, yc, &sample_color)) != LSRERR_OK) {
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
                        const f32 linear_depth =
                            (w0 * v0->position.z + w1 * v1->position.z + w2 * v2->position.z) * current_w;

                        f32 factor = 0.0f;
                        if ((result = render_get_fog_factor(r, linear_depth, &factor)) != LSRERR_OK) {
                            return result;
                        }

                        if ((result = f32x4_interpolate(&color, &r->settings.fog.color, factor)) != LSRERR_OK) {
                            return result;
                        }
                    }

                    if ((result == LSRERR_OK)
                        && r->settings.blending == RENDER_BLENDING_ENABLED) {
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
                    if ((result == LSRERR_OK)
                        && (result = f32x4_get_color(&color, &final_color)) != LSRERR_OK) {
                        return result;
                    }

                    // Draw pixel and update depth buffer if needed
                    if ((result = texture_set_point_color(r->surface, x, y, final_color)) == LSRERR_OK) {
                        if (r->settings.depth != RENDER_DEPTH_BUFFER_NONE) {
                            if ((result = texture_set_point_depth(r->depth, x, y, interpolated_depth)) != LSRERR_OK) {
                                return result;
                            }
                        }
                    }
                }
            }
        }
    }

    return result;
}

int render_get_fog_factor(const render* r, f32 depth, f32* factor) {
    if (r == NULL || factor == NULL) {
        return LSRERR_INVALID_ARGUMENT;
    }

    switch (r->settings.fog.mode) {
    case RENDER_FOG_LINEAR: {
        const f32 distance = r->settings.fog.end - r->settings.fog.start;

        if (r->settings.fog.end <= 0.0f || fabsf(distance) < 1e-5f) {
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
