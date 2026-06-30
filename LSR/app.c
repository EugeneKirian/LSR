#include "app.h"
#include "font.h"
#include "render.h"
#include "scene.h"
#include "surface.h"

#include <stdio.h>
#include <stdlib.h>

#define APP_FONT_NAME           "Arial"
#define APP_FONT_SIZE           18
#define APP_FONT_BACKGROUND     0x00000000
#define APP_FONT_FOREGROUND     0xFFFFFFFF

#define QUAD_INDEX_COUNT        6
#define QUAD_VERTEX_COUNT       4

#define Z_NEAR                  (0.0f)
#define Z_FAR                   (1.0f)

struct app {
    scene*              scene;
    render*             render;
    surface*            surface;
    CRITICAL_SECTION    lock;
    direction           movement;
    struct {
        mouse_button    button;
        POINT           current;
        POINT           previous;
    } mouse;
    render_clipping     clip;
    render_culling      cull;
    render_depth_buffer depth;
    render_dither       dither;
    render_draw_mode    mode;
    render_fill         fill;
    render_fog          fog;
    texture_sampling    sampling;
    font*               font;
};

static int app_render_scene(app* a, f64 time);
static int app_render_ui(app* a, f64 time);
static int app_render_text(app* a, int x, int y, const char* text);

static int app_allocate(app** outObj) {
    if (outObj == NULL) {
        return LSRERR_INVALID_ARGUMENT;
    }

    app* a = (app*)malloc(sizeof(app));
    if (a == NULL) {
        return LSRERR_OUT_OF_MEMORY;
    }

    ZeroMemory(a, sizeof(app));

    *outObj = a;

    return LSRERR_OK;
}

int app_create(app** outObj) {
    if (outObj == NULL) {
        return LSRERR_INVALID_ARGUMENT;
    }

    app* a = NULL;
    int result = app_allocate(&a);
    if (result != LSRERR_OK) {
        return result;
    }

    if ((result = render_create(&a->render)) != LSRERR_OK) {
        app_release(a);
        return result;
    }

    if ((result = font_create(APP_FONT_NAME, APP_FONT_SIZE,
        TRUE, APP_FONT_FOREGROUND, APP_FONT_BACKGROUND, &a->font)) != LSRERR_OK) {
        app_release(a);
        return result;
    }

    InitializeCriticalSection(&a->lock);

    a->clip = RENDER_CLIPPING_ENABLED;
    a->cull = RENDER_CULLING_CCW;
    a->depth = RENDER_DEPTH_BUFFER_Z;
    a->dither = RENDER_DITHER_ENABLED;
    a->fill = RENDER_FILL_SOLID;
    a->fog = RENDER_FOG_LINEAR;
    a->mode = RENDER_DRAW_MODE_TRIANGLES;
    a->sampling = TEXTURE_SAMPLING_NEAREST;

    *outObj = a;

    return LSRERR_OK;
}

void app_release(app* a) {
    if (a != NULL) {
        DeleteCriticalSection(&a->lock);

        if (a->scene != NULL) {
            scene_release(a->scene);
        }

        if (a->render != NULL) {
            render_release(a->render);
        }

        if (a->surface != NULL) {
            surface_release(a->surface);
        }

        if (a->font != NULL) {
            font_release(a->font);
        }
    }
}

int app_create_surface(app* a, HDC hdc, int w, int h) {
    if (a == NULL || hdc == NULL) {
        return LSRERR_INVALID_ARGUMENT;
    }

    if (w < 0 || h < 0) {
        return LSRERR_INVALID_ARGUMENT;
    }

    surface* s = NULL;
    int result = surface_create(hdc, w, h, &s);
    if (result != LSRERR_OK) {
        return result;
    }

    surface_release(InterlockedExchangePointer(&a->surface, s));

    return LSRERR_OK;
}

int app_load_scene(app* a, const char* path) {
    if (a == NULL || path == NULL) {
        return LSRERR_INVALID_ARGUMENT;
    }

    scene* s = NULL;
    int result = scene_create(path, &s);
    if (result != LSRERR_OK) {
        return result;
    }

    scene_release(InterlockedExchangePointer(&a->scene, s));

    return LSRERR_OK;
}

int app_resize_surface(app* a, int w, int h) {
    if (a == NULL) {
        return LSRERR_INVALID_ARGUMENT;
    }

    if (w < 0 || h < 0) {
        return LSRERR_INVALID_ARGUMENT;
    }

    if (a->surface == NULL) {
        return LSRERR_INVALID_CALL;
    }

    EnterCriticalSection(&a->lock);

    int result = surface_resize(a->surface, w, h);

    if (result == LSRERR_OK) {
        render_viewport vp;
        vp.x = 0;
        vp.y = 0;
        vp.width = w;
        vp.height = h;
        vp.min = Z_NEAR;
        vp.max = Z_FAR;

        if ((result = render_set_viewport(a->render, &vp)) == LSRERR_OK){
            result = render_set_render_target(a->render, a->surface);
        }
    }

    LeaveCriticalSection(&a->lock);

    return result;
}

int app_blt_surface(app* a, const RECT* rect, HDC hdc, const POINT* point) {
    if (a == NULL || hdc == NULL) {
        return LSRERR_INVALID_ARGUMENT;
    }

    if (a->surface == NULL) {
        return LSRERR_INVALID_CALL;
    }
    
    EnterCriticalSection(&a->lock);

    const int result = surface_blt(a->surface, rect, hdc, point);

    LeaveCriticalSection(&a->lock);

    return result;
}

int app_execute(app* a, f64 time) {
    if (a == NULL) {
        return LSRERR_INVALID_ARGUMENT;
    }

    if (a->scene == NULL || a->surface == NULL) {
        return LSRERR_INVALID_CALL;
    }

    camera* c = a->scene->camera;

    // Move & rotate camera
    int result = camera_move(c, a->movement, (f32)time);
    if (result == LSRERR_OK) {
        if (a->mouse.button & (MOUSE_BUTTON_LEFT | MOUSE_BUTTON_RIGHT)) {
            const f32 dx = -(f32)(a->mouse.previous.x - a->mouse.current.x) * 10.0f * (f32)time;
            const f32 dy = (f32)(a->mouse.previous.y - a->mouse.current.y) * 10.0f * (f32)time;
            if ((result = camera_rotate(c, dx, dy)) != LSRERR_OK) {
                return result;
            }
        }
    }

    EnterCriticalSection(&a->lock);

    // Render the scene and the UI
    if ((result = app_render_scene(a, time)) == LSRERR_OK) {
        result = app_render_ui(a, time);
    }

    LeaveCriticalSection(&a->lock);

    return result;
}

int app_key_down(app* a, int key) {
    if (a == NULL) {
        return LSRERR_INVALID_ARGUMENT;
    }

    switch (key) {
    case 'W':
    case VK_UP: {
        a->movement |= DIRECTION_FORWARD;
    } break;
    case 'S':
    case VK_DOWN: {
        a->movement |= DIRECTION_BACKWARD;
    } break;
    case 'D':
    case VK_RIGHT: {
        a->movement |= DIRECTION_RIGHT;
    } break;
    case 'A':
    case VK_LEFT: {
        a->movement |= DIRECTION_LEFT;
    } break;
    case 'Q': {
        a->movement |= DIRECTION_UPWARD;
    } break;
    case 'E': {
        a->movement |= DIRECTION_DOWNWARD;
    } break;
    case 'B': {
        a->depth = (a->depth + 1) % RENDER_DEPTH_BUFFER_COUNT;
    } break;
    case 'C': {
        a->clip = (a->clip + 1) % RENDER_CLIPPING_COUNT;
    } break;
    case 'I': {
        a->fill = (a->fill + 1) % RENDER_FILL_COUNT;
    } break;
    case 'F': {
        a->fog = (a->fog + 1) % RENDER_FOG_COUNT;
    } break;
    case 'M': {
        a->mode = (a->mode + 1) % RENDER_DRAW_MODE_COUNT;
    } break;
    case 'X': {
        a->cull = (a->cull + 1) % RENDER_CULLING_COUNT;
    } break;
    case 'H': {
        a->dither = (a->dither + 1) % RENDER_DITHER_COUNT;
    } break;
    case 'T': {
        a->sampling = (a->sampling + 1) % TEXTURE_SAMPLING_COUNT;
    } break;
    }

    return LSRERR_OK;
}

int app_key_up(app* a, int key) {
    if (a == NULL) {
        return LSRERR_INVALID_ARGUMENT;
    }

    switch (key) {
    case 'W':
    case VK_UP: {
        a->movement &= (~DIRECTION_FORWARD);
    } break;
    case 'S':
    case VK_DOWN: {
        a->movement &= (~DIRECTION_BACKWARD);
    } break;
    case 'D':
    case VK_RIGHT: {
        a->movement &= (~DIRECTION_RIGHT);
    } break;
    case 'A':
    case VK_LEFT: {
        a->movement &= (~DIRECTION_LEFT);
    } break;
    case 'Q': {
        a->movement &= (~DIRECTION_UPWARD);
    } break;
    case 'E': {
        a->movement &= (~DIRECTION_DOWNWARD);
    } break;
    }

    return LSRERR_OK;
}

int app_mouse_move(app* a, const POINT* point) {
    if (a == NULL || point == NULL) {
        return LSRERR_INVALID_ARGUMENT;
    }

    CopyMemory(&a->mouse.previous, &a->mouse.current, sizeof(POINT));
    CopyMemory(&a->mouse.current, point, sizeof(POINT));

    return LSRERR_OK;
}

int app_mouse_up(app* a, const POINT* point, mouse_button button) {
    if (a == NULL || point == NULL) {
        return LSRERR_INVALID_ARGUMENT;
    }

    if (button < MOUSE_BUTTON_LEFT || button >= MOUSE_BUTTON_COUNT) {
        return LSRERR_INVALID_ARGUMENT;
    }

    CopyMemory(&a->mouse.current, point, sizeof(POINT));
    CopyMemory(&a->mouse.previous, point, sizeof(POINT));

    a->mouse.button &= (~button);

    return LSRERR_OK;
}

int app_mouse_down(app* a, const POINT* point, mouse_button button) {
    if (a == NULL || point == NULL) {
        return LSRERR_INVALID_ARGUMENT;
    }

    if (button < MOUSE_BUTTON_LEFT || button >= MOUSE_BUTTON_COUNT) {
        return LSRERR_INVALID_ARGUMENT;
    }

    CopyMemory(&a->mouse.current, point, sizeof(POINT));
    CopyMemory(&a->mouse.previous, point, sizeof(POINT));

    a->mouse.button |= button;

    return LSRERR_OK;
}

int app_render_scene(app* a, f64 time) {
    if (a == NULL) {
        return LSRERR_INVALID_ARGUMENT;
    }

    int result = LSRERR_OK;

    scene* s = a->scene;
    render* r = a->render;

    // 1. Set projection and view matrixes
    f32m4 matrix;
    if ((result = f32m4_projection(&matrix, a->surface->width, a->surface->height, Z_NEAR, Z_FAR)) == LSRERR_OK) {
        if ((result = render_set_matrix(r, RENDER_MATRIX_PROJECTION, &matrix)) == LSRERR_OK) {
            if ((result = camera_get_matrix(s->camera, &matrix)) == LSRERR_OK) {
                result = render_set_matrix(r, RENDER_MATRIX_VIEW, &matrix);
            }
        }
    }

    // 2. Set configuration
    if ((result == LSRERR_OK)
        && (result = render_set_blending(r, RENDER_BLENDING_DISABLED)) == LSRERR_OK) {
        if ((result = render_set_clipping(r, a->clip)) == LSRERR_OK) {
            if ((result = render_set_culling(r, a->cull)) == LSRERR_OK) {
                if ((result = render_set_dither(r, a->dither)) == LSRERR_OK) {
                    if ((result = render_set_fill(r, a->fill)) == LSRERR_OK) {
                        if ((result = render_set_fog(r, a->fog)) == LSRERR_OK) {
                            if ((result = render_set_fog_color(r, 0xFFCCCCCC)) == LSRERR_OK) {
                                if ((result = render_set_fog_range(r, 0.2f, 10.0f)) == LSRERR_OK) {
                                    if ((result = render_set_texture_sampling(r, a->sampling)) == LSRERR_OK) {
                                        if ((result = render_set_depth_buffer(r, a->depth)) == LSRERR_OK) {
                                            result = render_set_draw_mode(r, a->mode);
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    // 3. Render the scene
    if ((result == LSRERR_OK) && (result = render_start(r)) == LSRERR_OK) {
        if ((result = render_clear(r, 0xFF636363)) == LSRERR_OK) {
            const size_t object_count = s->objects.count;
            for (size_t i = 0; i < object_count; i++) {
                if ((result = transform_f32m4(s->objects.transforms[i], &matrix)) == LSRERR_OK) {
                    if ((result = render_set_matrix(r, RENDER_MATRIX_WORLD, &matrix)) == LSRERR_OK) {
                        const mg* m = s->objects.meshes[i];
                        const size_t mesh_count = m->count;
                        for (size_t ii = 0; ii < mesh_count; ii++) {
                            // Find texture
                            const texture* tex = NULL;
                            const char* texture_name = m->meshes[ii]->texture;
                            if (texture_name != NULL) {
                                const size_t texture_count = s->assets.count;
                                for (size_t iii = 0; iii < texture_count; iii++) {
                                    if (strcmp(texture_name, s->assets.textures[iii]->name) == 0) {
                                        tex = s->assets.textures[iii];
                                        break;
                                    }
                                }
                            }

                            // Set texture & render the object
                            if ((result = render_set_texture(r, tex)) == LSRERR_OK) {
                                if ((result = render_draw(r, m->meshes[ii]->vertexes,
                                    m->meshes[ii]->indexes, m->meshes[ii]->index_count)) != LSRERR_OK) {
                                    goto stop;
                                }
                            }
                        }
                    }
                }
            }
        }

    stop:
        if ((result = render_end(r)) != LSRERR_OK) {
            return result;
        }
    }

    return result;
}

int app_render_ui(app* a, f64 time) {
    if (a == NULL) {
        return LSRERR_INVALID_ARGUMENT;
    }

    int result = LSRERR_OK;

    scene* s = a->scene;
    render* r = a->render;

    // 1. Set projection, view, and world matrixes
    f32m4 matrix;
    if ((result = f32m4_orthographic(&matrix, a->surface->width, a->surface->height, Z_NEAR, Z_FAR)) == LSRERR_OK) {
        if ((result = render_set_matrix(r, RENDER_MATRIX_PROJECTION, &matrix)) == LSRERR_OK) {
            if ((result = f32m4_identity(&matrix)) == LSRERR_OK) {
                if ((result = render_set_matrix(r, RENDER_MATRIX_VIEW, &matrix)) == LSRERR_OK) {
                    result = render_set_matrix(r, RENDER_MATRIX_WORLD, &matrix);
                }
            }
        }
    }

    // 2. Set configuration
    if ((result == LSRERR_OK)
        && (result = render_set_blending(r, RENDER_BLENDING_ENABLED)) == LSRERR_OK) {
        if ((result = render_set_clipping(r, RENDER_CLIPPING_ENABLED)) == LSRERR_OK) {
            if ((result = render_set_culling(r, RENDER_CULLING_NONE)) == LSRERR_OK) {
                if ((result = render_set_dither(r, RENDER_DITHER_DISABLED)) == LSRERR_OK) {
                    if ((result = render_set_fill(r, RENDER_FILL_SOLID)) == LSRERR_OK) {
                        if ((result = render_set_fog(r, RENDER_FOG_NONE)) == LSRERR_OK) {
                            if ((result = render_set_texture_sampling(r, TEXTURE_SAMPLING_NEAREST)) == LSRERR_OK) {
                                if ((result = render_set_depth_buffer(r, RENDER_DEPTH_BUFFER_NONE)) == LSRERR_OK) {
                                    result = render_set_draw_mode(r, RENDER_DRAW_MODE_TRIANGLES);
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    // 3. Render the UI
    if ((result == LSRERR_OK) && (result = render_start(r)) == LSRERR_OK) {
        char message[256];

        const char* cull_mode = "None";
        switch (a->cull) {
        case RENDER_CULLING_CW: { cull_mode = "CW"; } break;
        case RENDER_CULLING_CCW: { cull_mode = "CCW"; } break;
        }

        const char* depth_mode = "None";
        switch (a->depth) {
        case RENDER_DEPTH_BUFFER_Z: { depth_mode = "Z"; } break;
        case RENDER_DEPTH_BUFFER_W: { depth_mode = "W"; } break;
        }

        const char* draw_mode = "Points";
        switch (a->mode) {
        case RENDER_DRAW_MODE_LINES: { draw_mode = "Lines"; } break;
        case RENDER_DRAW_MODE_TRIANGLES: { draw_mode = "Triangles"; } break;
        }

        const char* fog_mode = "None";
        switch (a->fog) {
        case RENDER_FOG_LINEAR: { fog_mode = "Linear"; } break;
        case RENDER_FOG_EXPONENTIAL: { fog_mode = "Exp"; } break;
        case RENDER_FOG_EXPONENTIAL_SQUARED: { fog_mode = "Exp^2"; } break;
        }

        const char* fill_mode = "Points";
        switch (a->fill) {
        case RENDER_FILL_SOLID: { fill_mode = "Solid"; } break;
        case RENDER_FILL_WIRE: { fill_mode = "Wire"; } break;
        }

        const char* sampling_mode = "Nearest";
        switch (a->sampling) {
        case TEXTURE_SAMPLING_BILINEAR: { sampling_mode = "Bilinear"; } break;
        case TEXTURE_SAMPLING_TRILINEAR: { sampling_mode = "Trilinear"; } break;
        }

        sprintf(message,
            "FPS: %5d\r\n(C) Clip: %s\r\n(X) Cull: %s\r\n(B) Depth: %s\r\n"
            "(H) Dither: %s\r\n(M) Mode: %s\r\n(I) Fill: %s\r\n(F) Fog: %s\r\n"
            "(T) Sampling: %s\r\nCamera: %.2f %.2f %.2f",
            (int)(1.0 / time), a->clip == RENDER_CLIPPING_ENABLED ? "On" : "Off",
            cull_mode, depth_mode, a->dither == RENDER_DITHER_ENABLED ? "On" : "Off",
            draw_mode, fill_mode, fog_mode, sampling_mode,
            a->scene->camera->position.x, a->scene->camera->position.y, a->scene->camera->position.z);

        if ((result = app_render_text(a, 2, 0, message)) != LSRERR_OK) {
            goto stop;
        }

    stop:
        if ((result = render_end(r)) != LSRERR_OK) {
            return result;
        }
    }

    return LSRERR_OK;
}

int app_render_text(app* a, int x, int y, const char* text) {
    if (a == NULL || text == NULL) {
        return LSRERR_INVALID_ARGUMENT;
    }

    int cx = x, height = 0;
    const texture* tex = NULL;
    vertex vertexes[QUAD_VERTEX_COUNT];
    ZeroMemory(vertexes, QUAD_VERTEX_COUNT * sizeof(vertex));
    const int idx[QUAD_INDEX_COUNT] = { 0, 1, 2, 3, 0, 2 };
    fqd quad;
    ZeroMemory(&quad, sizeof(fqd));

    for (int i = 0; i < QUAD_VERTEX_COUNT; i++) {
        vertexes[i].color = 0xFFFFFFFF;
    }

    int result = LSRERR_OK;
    if ((result = font_get_height(a->font, &height)) == LSRERR_OK) {
        if ((result = font_get_texture(a->font, &tex)) == LSRERR_OK) {
            if ((result = render_set_texture(a->render, tex)) == LSRERR_OK) {
                for (int i = 0; text[i] != NULL; i++) {
                    u32 repeat = 1;
                    u32 character = (u32)text[i];

                    if (character == '\r') {
                        continue;
                    }
                    else if (character == '\n') {
                        cx = x;
                        y += height;
                        continue;
                    }
                    else if (character == '\t') {
                        character = ' ';
                        repeat = 4;
                    }

                    if (font_get_item(a->font, character, &quad) == LSRERR_OK) {
                        for (int q = 0; q < QUAD_VERTEX_COUNT; q++) {
                            CopyMemory(&vertexes[q].uv, &quad.uv[q], sizeof(f32x2));
                        }

                        for (u32 r = 0; r < repeat; r++) {
                            vertexes[0].position.x = (f32)(cx);
                            vertexes[0].position.y = (f32)(y + height - quad.y);
                            vertexes[1].position.x = (f32)(cx);
                            vertexes[1].position.y = (f32)(y + height + quad.height - quad.y);
                            vertexes[2].position.x = (f32)(cx + quad.width);
                            vertexes[2].position.y = (f32)(y + height + quad.height - quad.y);
                            vertexes[3].position.x = (f32)(cx + quad.width);
                            vertexes[3].position.y = (f32)(y + height - quad.y);

                            if ((result = render_draw(a->render, vertexes, idx, QUAD_INDEX_COUNT)) != LSRERR_OK) {
                                return result;
                            }

                            cx +=  quad.span;
                        }
                    }
                }
            }
        }
    }

    return LSRERR_OK;
}
