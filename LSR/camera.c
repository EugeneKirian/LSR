#include "camera.h"

#define _USE_MATH_DEFINES
#include <math.h>
#include <stdlib.h>

static int camera_get_forward(camera* c, f32x3* forward) {
    if (c == NULL || forward == NULL) {
        return LSRERR_INVALID_ARGUMENT;
    }

    const f32 pitch = c->rotation.pitch * (f32)(M_PI / 180.0);
    const f32 yaw = c->rotation.yaw * (f32)(M_PI / 180.0);

    const f32 cos_pitch = cosf(pitch);
    const f32 sin_pitch = sinf(pitch);

    const f32 cosn_yaw = cosf(yaw);
    const f32 sin_yaw = sinf(yaw);

    forward->x = sin_yaw * cos_pitch;
    forward->y = -sin_pitch;
    forward->z = cosn_yaw * cos_pitch;

    return LSRERR_OK;
}

static int camera_get_right(camera* c, f32x3* right) {
    if (c == NULL || right == NULL) {
        return LSRERR_INVALID_ARGUMENT;
    }

    const f32 pitch = c->rotation.pitch * (f32)(M_PI / 180.0);
    const f32 yaw = c->rotation.yaw * (f32)(M_PI / 180.0);
    const f32 roll = c->rotation.roll * (f32)(M_PI / 180.0);

    const f32 cos_pitch = cosf(pitch);
    const f32 sin_pitch = sinf(pitch);

    const f32 cos_roll = cosf(roll);
    const f32 sin_roll = sinf(roll);

    const f32 cosn_yaw = cosf(yaw);
    const f32 sin_yaw = sinf(yaw);

    right->x = cosn_yaw * cos_pitch + sin_yaw * sin_pitch * sin_roll;
    right->y = cos_pitch * sin_roll;
    right->z = -sin_yaw * cos_pitch + cosn_yaw * sin_pitch * sin_roll;

    return LSRERR_OK;
}

static int camera_get_up(camera* c, f32x3* up) {
    if (c == NULL || up == NULL) {
        return LSRERR_INVALID_ARGUMENT;
    }

    const f32 pitch = c->rotation.pitch * (f32)(M_PI / 180.0);
    const f32 yaw = c->rotation.yaw * (f32)(M_PI / 180.0);
    const f32 roll = c->rotation.roll * (f32)(M_PI / 180.0);

    const f32 cos_pitch = cosf(pitch);
    const f32 sin_pitch = sinf(pitch);

    const f32 cos_roll = cosf(roll);
    const f32 sin_roll = sinf(roll);

    const f32 cosn_yaw = cosf(yaw);
    const f32 sin_yaw = sinf(yaw);

    up->x = -cosn_yaw * sin_roll + sin_yaw * sin_pitch * cos_roll;
    up->y = cos_pitch * cos_roll;
    up->z = sin_yaw * sin_roll + cosn_yaw * sin_pitch * cos_roll;

    return LSRERR_OK;
}

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

    *outObj = c;

    return LSRERR_OK;
}

void camera_release(camera* c) {
    if (c != NULL) {
        free(c);
    }
}

int camera_move(camera* c, direction d, f32 value) {
    if (c == NULL) {
        return LSRERR_INVALID_ARGUMENT;
    }

    if ((d & DIRECTION_FORWARD) || (d & DIRECTION_BACKWARD)) {
        if ((d & DIRECTION_FORWARD) && !(d & DIRECTION_BACKWARD)) {
            camera_move_forward(c, value);
        }
        else if (!(d & DIRECTION_FORWARD) && (d & DIRECTION_BACKWARD)) {
            camera_move_forward(c, -value);
        }
    }

    if ((d & DIRECTION_LEFT) || (d & DIRECTION_RIGHT)) {
        if ((d & DIRECTION_LEFT) && !(d & DIRECTION_RIGHT)) {
            camera_move_right(c, -value);
        }
        else if (!(d & DIRECTION_LEFT) && (d & DIRECTION_RIGHT)) {
            camera_move_right(c, value);
        }
    }

    if ((d & DIRECTION_UPWARD) || (d & DIRECTION_DOWNWARD)) {
        if ((d & DIRECTION_UPWARD) && !(d & DIRECTION_DOWNWARD)) {
            camera_move_up(c, value);
        }
        else if (!(d & DIRECTION_UPWARD) && (d & DIRECTION_DOWNWARD)) {
            camera_move_up(c, -value);
        }
    }

    return LSRERR_OK;
}

int camera_move_forward(camera* c, f32 value) {
    if (c == NULL) {
        return LSRERR_INVALID_ARGUMENT;
    }

    f32x3 forward;
    int result = camera_get_forward(c, &forward);
    if (result == LSRERR_OK) {
        c->position.x += forward.x * value;
        c->position.y += forward.y * value;
        c->position.z += forward.z * value;
    }

    return result;
}

int camera_move_up(camera* c, f32 value) {
    if (c == NULL) {
        return LSRERR_INVALID_ARGUMENT;
    }

    f32x3 up;
    int result = camera_get_up(c, &up);
    if (result == LSRERR_OK) {
        c->position.x += up.x * value;
        c->position.y += up.y * value;
        c->position.z += up.z * value;
    }

    return result;
}

int camera_move_right(camera* c, f32 value) {
    if (c == NULL) {
        return LSRERR_INVALID_ARGUMENT;
    }

    f32x3 right;
    int result = camera_get_right(c, &right);
    if (result == LSRERR_OK) {
        c->position.x += right.x * value;
        c->position.y += right.y * value;
        c->position.z += right.z * value;
    }

    return result;
}

int camera_rotate(camera* c, f32 x, f32 y) {
    if (c == NULL) {
        return LSRERR_INVALID_ARGUMENT;
    }

    c->rotation.yaw -= x;
    c->rotation.pitch += y;

    return LSRERR_OK;
}

int camera_get_matrix(camera* c, f32m4* m) {
    if (c == NULL || m == NULL) {
        return LSRERR_INVALID_ARGUMENT;
    }

    // 1. Calculate a perfect Forward vector mathematically
    const f32 pitch = c->rotation.pitch * (f32)(M_PI / 180.0);
    const f32 yaw = c->rotation.yaw * (f32)(M_PI / 180.0);

    f32x3 forward;
    forward.x = sinf(yaw) * cosf(pitch);
    forward.y = -sinf(pitch);
    forward.z = cosf(yaw) * cosf(pitch);

    // Normalize forward vector just to guarantee precision
    f32 f_len = sqrtf(forward.x * forward.x + forward.y * forward.y + forward.z * forward.z);
    forward.x /= f_len; forward.y /= f_len; forward.z /= f_len;

    // 2. Derive Right vector using Cross Product with a global Up vector (0, 1, 0)
    // Left-Handed rule: Right = Up_Global x Forward
    f32x3 global_up = { 0.0f, 1.0f, 0.0f };
    f32x3 right;
    right.x = global_up.y * forward.z - global_up.z * forward.y;
    right.y = global_up.z * forward.x - global_up.x * forward.z;
    right.z = global_up.x * forward.y - global_up.y * forward.x;

    // Normalize Right vector
    f32 r_len = sqrtf(right.x * right.x + right.y * right.y + right.z * right.z);
    if (r_len > 0.0f) {
        right.x /= r_len; right.y /= r_len; right.z /= r_len;
    }
    else {
        // Fallback case if looking straight down/up
        right.x = 1.0f; right.y = 0.0f; right.z = 0.0f;
    }

    // 3. Derive Local Up vector using Cross Product
    // Left-Handed rule: Up = Forward x Right
    f32x3 up;
    up.x = forward.y * right.z - forward.z * right.y;
    up.y = forward.z * right.x - forward.x * right.z;
    up.z = forward.x * right.y - forward.y * right.x;

    // 4. Handle Roll if you explicitly need it (Optional)
    if (c->rotation.roll != 0.0f) {
        const f32 roll = c->rotation.roll * (f32)(M_PI / 180.0);
        const f32 cos_r = cosf(roll);
        const f32 sin_r = sinf(roll);

        // Rotate Right and Up vectors around the Forward axis
        f32x3 r_orig = right;
        f32x3 u_orig = up;

        right.x = r_orig.x * cos_r + u_orig.x * sin_r;
        right.y = r_orig.y * cos_r + u_orig.y * sin_r;
        right.z = r_orig.z * cos_r + u_orig.z * sin_r;

        up.x = -r_orig.x * sin_r + u_orig.x * cos_r;
        up.y = -r_orig.y * sin_r + u_orig.y * cos_r;
        up.z = -r_orig.z * sin_r + u_orig.z * cos_r;
    }

    // 5. Left-Handed View Translation Dot Products
    f32x3 translation;
    translation.x = -(c->position.x * right.x + c->position.y * right.y + c->position.z * right.z);
    translation.y = -(c->position.x * up.x + c->position.y * up.y + c->position.z * up.z);
    translation.z = -(c->position.x * forward.x + c->position.y * forward.y + c->position.z * forward.z);

    // 6. Output to Row-Major Matrix
    m->m4x4[0][0] = right.x;       m->m4x4[0][1] = up.x;       m->m4x4[0][2] = forward.x;       m->m4x4[0][3] = 0.0f;
    m->m4x4[1][0] = right.y;       m->m4x4[1][1] = up.y;       m->m4x4[1][2] = forward.y;       m->m4x4[1][3] = 0.0f;
    m->m4x4[2][0] = right.z;       m->m4x4[2][1] = up.z;       m->m4x4[2][2] = forward.z;       m->m4x4[2][3] = 0.0f;
    m->m4x4[3][0] = translation.x; m->m4x4[3][1] = translation.y; m->m4x4[3][2] = translation.z; m->m4x4[3][3] = 1.0f;

    return LSRERR_OK;
}
