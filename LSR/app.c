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
};

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

    render* r = a->render;
    scene* s = a->scene;

    // Move camera
    int result = camera_move(s->camera, a->movement, (f32)time);
    if (result != LSRERR_OK) {
        return result;
    }

    if (a->mouse.button & (MOUSE_BUTTON_LEFT | MOUSE_BUTTON_RIGHT)) {
        const f32 dx = (f32)(a->mouse.previous.x - a->mouse.current.x) * 10.0f * (f32)time;
        const f32 dy = (f32)(a->mouse.previous.y - a->mouse.current.y) * 10.0f * (f32)time;
        if ((result = camera_rotate(s->camera, dx, dy)) != LSRERR_OK) {
            return result;
        }
    }

    EnterCriticalSection(&a->lock);

    // TODO fog
    // TODO mode
    // TODO depth buffer

    f32m4 view;
    if ((result = camera_get_matrix(s->camera, &view)) == LSRERR_OK) {
        if ((result = render_set_matrix(r, RENDER_MATRIX_VIEW, &view)) == LSRERR_OK) {
            if ((result = render_start(r)) == LSRERR_OK) {
                if ((result = render_clear(r, 0xFF636363)) == LSRERR_OK) {
                    const size_t object_count = s->objects.count;
                    for (size_t i = 0; i < object_count; i++) {
                        const mg* m = s->objects.meshes[i];
                        const size_t mesh_count = m->count;
                        for (size_t ii = 0; ii < mesh_count; ii++) {
                            // Texture
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

                            if ((result = render_set_texture(r, tex)) == LSRERR_OK) {

                                // TODO set world matrix

                                // Object
                                result = render_draw(r, m->meshes[ii]->vertexes,
                                    m->meshes[ii]->indexes, m->meshes[ii]->index_count);
                            }
                        }
                    }

                    result = render_end(r);
                }
            }
        }
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

int app_mouse_up(app* a, mouse_button button) {
    if (a == NULL) {
        return LSRERR_INVALID_ARGUMENT;
    }

    if (button < MOUSE_BUTTON_LEFT || button >= MOUSE_BUTTON_COUNT) {
        return LSRERR_INVALID_ARGUMENT;
    }

    a->mouse.button &= (~button);

    return LSRERR_OK;
}

int app_mouse_down(app* a, mouse_button button) {
    if (a == NULL) {
        return LSRERR_INVALID_ARGUMENT;
    }

    if (button < MOUSE_BUTTON_LEFT || button >= MOUSE_BUTTON_COUNT) {
        return LSRERR_INVALID_ARGUMENT;
    }

    a->mouse.button |= button;

    return LSRERR_OK;
}
