#pragma once

#include "scene.h"

typedef struct app app;

int app_create(app** outObj);
void app_release(app* a);

int app_create_surface(app* a, HDC hdc, int w, int h);
int app_load_scene(app* a, const char* path);

int app_resize_surface(app* a, int w, int h);
int app_blt_surface(app* a, const RECT* rect, HDC hdc, const POINT* point);

int app_execute(app* a, f64 time);

int app_key_down(app* a, int key);
int app_key_up(app* a, int key);
