#pragma once

#include "common.h"

typedef struct camera {
    f32x3 position;
    euler rotation;
} camera;

int camera_create(f32x3* position, camera** outObj);
void camera_release(camera* c);

int camera_move(camera* c, direction d, f32 value);
int camera_move_forward(camera* c, f32 value);
int camera_move_up(camera* c, f32 value);
int camera_move_right(camera* c, f32 value);

int camera_get_matrix(camera* c, f32m4* m);
