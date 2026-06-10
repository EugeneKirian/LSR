#pragma once

#include "common.h"

typedef struct camera {
    f32x3 position;
    f32x3 up, right;
} camera;

int camera_create(f32x3* position, camera** outObj);
void camera_release(camera* c);
