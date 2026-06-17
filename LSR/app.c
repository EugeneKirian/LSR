#include "app.h"
#include "render.h"
#include "scene.h"
#include "surface.h"

#include <stdlib.h>

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
    render_draw_mode    mode;
};

static int app_render_scene(app* a, f64 time);
static int app_render_ui(app* a, f64 time);

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

    InitializeCriticalSection(&a->lock);

    a->mode = RENDER_DRAW_MODE_TRIANGLES;

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
        vp.min = 0.0f;
        vp.max = 1.0f;
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
    case 'M': {
        a->mode = (a->mode + 1) % RENDER_DRAW_MODE_COUNT;
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
    if ((result = f32m4_projection(&matrix, a->surface->width, a->surface->height, 0.0f, 1.0f)) == LSRERR_OK) {
        if ((result = render_set_matrix(r, RENDER_MATRIX_PROJECTION, &matrix)) == LSRERR_OK) {
            if ((result = camera_get_matrix(s->camera, &matrix)) == LSRERR_OK) {
                result = render_set_matrix(r, RENDER_MATRIX_VIEW, &matrix);
            }
        }
    }

    // 2. Set configuration
    if ((result == LSRERR_OK)
        && (result = render_set_clipping(r, RENDER_CLIPPING_ENABLED)) == LSRERR_OK) {
        if ((result = render_set_culling(r, RENDER_CULLING_CCW)) == LSRERR_OK) {
            if ((result = render_set_depth_buffer(r, RENDER_DEPTH_BUFFER_Z)) == LSRERR_OK) {
                result = render_set_draw_mode(r, a->mode);
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
    if ((result = f32m4_orthographic(&matrix, a->surface->width, a->surface->height, 0.0f, 1.0f)) == LSRERR_OK) {
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
        && (result = render_set_clipping(r, RENDER_CLIPPING_ENABLED)) == LSRERR_OK) {
        if ((result = render_set_culling(r, RENDER_CULLING_NONE)) == LSRERR_OK) {
            if ((result = render_set_depth_buffer(r, RENDER_DEPTH_BUFFER_NONE)) == LSRERR_OK) {
                result = render_set_draw_mode(r, RENDER_DRAW_MODE_TRIANGLES);
            }
        }
    }

    // 3. Render the UI
    if ((result == LSRERR_OK) && (result = render_start(r)) == LSRERR_OK) {

        //// TODO
        //vertex v[4];
        //ZeroMemory(v, sizeof(v));

        //v[0].position.x = 100.0f;
        //v[0].position.y = 100.0f;
        //v[0].position.z = 0.0f;
        //v[0].color = 0xFF000000;

        //v[1].position.x = 100.0f;
        //v[1].position.y = 200.0f;
        //v[1].position.z = 0.0f;
        //v[1].color = 0xFFFF0000;

        //v[2].position.x = 200.0f;
        //v[2].position.y = 200.0f;
        //v[2].position.z = 0.0f;
        //v[2].color = 0xFF00FF00;

        //v[3].position.x = 200.0f;
        //v[3].position.y = 100.0f;
        //v[3].position.z = 0.0f;
        //v[3].color = 0xFF0000FF;

        //int idx[6] = { 0, 1, 2, 3, 0, 2 };

        //// Set texture & render the object
        //if ((result = render_set_texture(r, NULL)) == LSRERR_OK) {
        //    if ((result = render_draw(r, v, idx, 6)) != LSRERR_OK) {
        //        goto stop;
        //    }
        //}

    stop:
        if ((result = render_end(r)) != LSRERR_OK) {
            return result;
        }
    }

    return LSRERR_OK;
}
