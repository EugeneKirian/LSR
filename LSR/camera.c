#include "camera.h"

#include <stdlib.h>

int camera_create(f32x3* position, camera** outObj) {
    if (position == NULL || outObj == NULL) {
        return LSRERR_INVALID_ARGUMENT;
    }

    camera* c = (camera*)malloc(sizeof(camera));
    if (c == NULL) {
        return LSRERR_OUT_OF_MEMORY;
    }

    ZeroMemory(c, sizeof(camera));
    
    CopyMemory(&c->position, position, sizeof(f32x3));

    c->up.y = 1.0f;
    c->right.x = 1.0f;

    *outObj = c;

    return LSRERR_OK;
}

void camera_release(camera* c) {
    if (c != NULL) {
        free(c);
    }
}
