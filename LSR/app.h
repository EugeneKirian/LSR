#pragma once

#include "scene.h"

typedef struct app app;

typedef enum mouse_button {
    MOUSE_BUTTON_LEFT           = 0,
    MOUSE_BUTTON_RIGHT          = 1,
    MOUSE_BUTTON_COUNT          = 2,
    MOUSE_BUTTON_FORCE_DWORD    = 0x7FFFFFFF
} mouse_button;

int app_create(app** outObj);
void app_release(app* a);

int app_create_surface(app* a, HDC hdc, int w, int h);
int app_load_scene(app* a, const char* path);

int app_resize_surface(app* a, int w, int h);
int app_blt_surface(app* a, const RECT* rect, HDC hdc, const POINT* point);

int app_execute(app* a, f64 time);

int app_key_down(app* a, int key);
int app_key_up(app* a, int key);

int app_mouse_move(app* a, const POINT* point);
int app_mouse_up(app* a, const POINT* point, mouse_button button);
int app_mouse_down(app* a, const POINT* point, mouse_button button);
