#include "camera.h"

#define _USE_MATH_DEFINES
#include <math.h>
#include <stdlib.h>

static int camera_get_forward(const camera* c, f32x3* forward) {
    if (c == NULL || forward == NULL) {
        return LSRERR_INVALID_ARGUMENT;
    }

    const f32 pitch = c->rotation.pitch * (f32)(M_PI / 180.0);
    const f32 yaw = c->rotation.yaw * (f32)(M_PI / 180.0);

    const f32 cos_pitch = cosf(pitch);
    const f32 sin_pitch = sinf(pitch);

    const f32 cos_yaw = cosf(yaw);
    const f32 sin_yaw = sinf(yaw);

    forward->x = sin_yaw * cos_pitch;
    forward->y = sin_pitch;
    forward->z = cos_yaw * cos_pitch;

    return LSRERR_OK;
}

static int camera_get_right(const camera* c, f32x3* right) {
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

    const f32 cos_yaw = cosf(yaw);
    const f32 sin_yaw = sinf(yaw);

    right->x = cos_roll * cos_yaw + sin_roll * sin_pitch * sin_yaw;
    right->y = sin_roll * cos_pitch;
    right->z = -cos_roll * sin_yaw + sin_roll * sin_pitch * cos_yaw;

    return LSRERR_OK;
}

static int camera_get_up(const camera* c, f32x3* up) {
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

    const f32 cos_yaw = cosf(yaw);
    const f32 sin_yaw = sinf(yaw);

    up->x = -sin_roll * cos_yaw + cos_roll * sin_pitch * sin_yaw;
    up->y = cos_roll * cos_pitch;
    up->z = sin_roll * sin_yaw + cos_roll * sin_pitch * cos_yaw;

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

    const f32 yaw = c->rotation.yaw * (f32)(M_PI / 180.0);

    c->position.x += sinf(yaw) * value;
    c->position.z += cosf(yaw) * value;

    return LSRERR_OK;
}

int camera_move_up(camera* c, f32 value) {
    if (c == NULL) {
        return LSRERR_INVALID_ARGUMENT;
    }

    c->position.y += value;

    return LSRERR_OK;
}

int camera_move_right(camera* c, f32 value) {
    if (c == NULL) {
        return LSRERR_INVALID_ARGUMENT;
    }

    const f32 yaw = c->rotation.yaw * (f32)(M_PI / 180.0);

    c->position.x += cosf(yaw) * value;
    c->position.z += -sinf(yaw) * value;

    return LSRERR_OK;
}

int camera_look_at(camera* c, const f32x3* target) {
    if (c == NULL || target == NULL) {
        return LSRERR_INVALID_ARGUMENT;
    }

    // 1. Calculate the forward direction vector (Target - Position)
    const f32 dx = target->x - c->position.x;
    const f32 dy = target->y - c->position.y;
    const f32 dz = target->z - c->position.z;

    // 2. Compute the horizontal ground distance (XZ plane projection)
    const f32 xz_dist = sqrtf(dx * dx + dz * dz);

    // 3. Handle gimbal lock edge case (Looking perfectly straight Up or Down)
    if (xz_dist < 0.00001f) {
        // Yaw remains unchanged to prevent sudden spinning/snapping
        c->rotation.pitch = dy > 0.0f
            ? 90.0f  /* Perfectly Up */ : -90.0f; /* Perfectly Down */

        // Roll is conventionally zeroed or preserved during an absolute LookAt
        c->rotation.roll = 0.0f;

        return LSRERR_OK;
    }

    // 4. Calculate Yaw (Rotation around Y-axis)
    // In D3D LH, Yaw = 0 is +Z, Yaw = 90 deg is +X.
    // Therefore, atan2f(x, z) is used.
    f32 yaw_rad = atan2f(dx, dz);

    // 5. Calculate Pitch (Rotation around X-axis)
    // Pitch is the angle between the ground plane (xz_dist) and the vertical target (dy)
    f32 pitch_rad = atan2f(dy, xz_dist);

    // 6. Convert Radians back to Degrees and update the camera asset
    c->rotation.yaw = yaw_rad * (f32)(180.0 / M_PI);
    c->rotation.pitch = pitch_rad * (f32)(180.0 / M_PI);
    c->rotation.roll = 0.0f; // Reset roll to align with global horizon

    // Normalize Yaw to a friendly [0, 360.0) range
    while (c->rotation.yaw < 0.0f) {
        c->rotation.yaw += 360.0f;
    }

    return LSRERR_OK;
}

int camera_rotate(camera* c, f32 x, f32 y) {
    if (c == NULL) {
        return LSRERR_INVALID_ARGUMENT;
    }

    // 1. Update yaw (horizontal look)
    c->rotation.yaw += x;

    // 2. Keep yaw within [0.0, 360.0) range
    // to avoid large numbers breaking precision
    while (c->rotation.yaw >= 360.0f) {
        c->rotation.yaw -= 360.0f;
    }
    
    while (c->rotation.yaw < 0.0f) {
        c->rotation.yaw += 360.0f;
    }

    // 3. Update pitch (vertical look)
    c->rotation.pitch += y;

    // 4. Clamp pitch to prevent the camera
    // from flipping over the poles (+/- 90 degrees)
    c->rotation.pitch = clampf(c->rotation.pitch, -89.9f, 89.9f);

    // 5. Explicitly lock roll to 0
    // to keep the player baseline perfectly horizontal
    c->rotation.roll = 0.0f;

    return LSRERR_OK;
}

int camera_get_matrix(const camera* c, f32m4* m) {
    if (c == NULL || m == NULL) {
        return LSRERR_INVALID_ARGUMENT;
    }

    int result = LSRERR_OK;
    f32x3 forward, right, up;

    // 1. Calculate and normalize forward vector
    if ((result = camera_get_forward(c, &forward)) != LSRERR_OK) {
        return result;
    }

    // 2. Normalize the forward vector
    if ((result = f32x3_normalize(&forward, &forward)) != LSRERR_OK) {
        return result;
    }

    // 3. Derive right vector using cross product with global up (0, 1, 0)
    // Left-Handed: Right = Up_Global x Forward
    f32x3 global_up;
    global_up.x = 0.0f; global_up.y = 1.0f; global_up.z = 0.0f;
    if ((result = f32x3_cross_product(&global_up, &forward, &right)) != LSRERR_OK) {
        return result;
    }

    // 4. Normalize right vector
    f32 r_len = sqrtf(right.x * right.x + right.y * right.y + right.z * right.z);
    if (r_len > 0.0f) {
        right.x /= r_len; right.y /= r_len; right.z /= r_len;
    }
    else {
        // Fallback for looking perfectly up or down
        right.x = 1.0f; right.y = 0.0f; right.z = 0.0f;
    }

    // 5. Derive local up vector using cross product
    // Left-Handed: Up = Forward x Right
    if ((result = f32x3_cross_product(&forward, &right, &up)) != LSRERR_OK) {
        return result;
    }

    // 6. Handle Roll
    if (c->rotation.roll != 0.0f) {
        const f32 roll = c->rotation.roll * (f32)(M_PI / 180.0);
        const f32 cos_r = cosf(roll);
        const f32 sin_r = sinf(roll);

        const f32x3 r_orig = right;
        const f32x3 u_orig = up;

        right.x = r_orig.x * cos_r - u_orig.x * sin_r;
        right.y = r_orig.y * cos_r - u_orig.y * sin_r;
        right.z = r_orig.z * cos_r - u_orig.z * sin_r;

        up.x = r_orig.x * sin_r + u_orig.x * cos_r;
        up.y = r_orig.y * sin_r + u_orig.y * cos_r;
        up.z = r_orig.z * sin_r + u_orig.z * cos_r;
    }

    // 7. Left-Handed View Translation Dot Products
    f32x3 translation;
    translation.x = -(c->position.x * right.x + c->position.y * right.y + c->position.z * right.z);
    translation.y = -(c->position.x * up.x + c->position.y * up.y + c->position.z * up.z);
    translation.z = -(c->position.x * forward.x + c->position.y * forward.y + c->position.z * forward.z);

    // 8. Output to Row-Major Matrix (Corrected indexing layout)
    m->m4x4[0][0] = right.x; m->m4x4[0][1] = up.x; m->m4x4[0][2] = forward.x; m->m4x4[0][3] = 0.0f;
    m->m4x4[1][0] = right.y; m->m4x4[1][1] = up.y; m->m4x4[1][2] = forward.y; m->m4x4[1][3] = 0.0f;
    m->m4x4[2][0] = right.z; m->m4x4[2][1] = up.z; m->m4x4[2][2] = forward.z; m->m4x4[2][3] = 0.0f;
    m->m4x4[3][0] = translation.x; m->m4x4[3][1] = translation.y; m->m4x4[3][2] = translation.z; m->m4x4[3][3] = 1.0f;

    return LSRERR_OK;
}
