#pragma once

#include "common.h"

#define _USE_MATH_DEFINES
#include <math.h>

int f32x3_add_f32x3(f32x3* v, const f32x3* value) {
    if (v == NULL || value == NULL) {
        return LSRERR_INVALID_ARGUMENT;
    }

    v->x += value->x;
    v->y += value->y;
    v->z += value->z;

    return LSRERR_OK;
}

int f32x3_cross_product(const f32x3* v1, const f32x3* v2, f32x3* cp) {
    if (v1 == NULL || v2 == NULL || cp == NULL) {
        return LSRERR_INVALID_ARGUMENT;
    }

    cp->x = v1->y * v2->z - v1->z * v2->y;
    cp->y = v1->z * v2->x - v1->x * v2->z;
    cp->z = v1->x * v2->y - v1->y * v2->x;

    return LSRERR_OK;
}

int f32x3_normalize(const f32x3* v, f32x3* n) {
    if (v == NULL || n == NULL) {
        return LSRERR_INVALID_ARGUMENT;
    }

    const f32 length =
        sqrtf(v->x * v->x + v->y * v->y + v->z * v->z);

    if (length == 0.0f) {
        n->x = v->x;
        n->y = v->y;
        n->z = v->z;

        return LSRERR_OK;
    }

    n->x = v->x / length;
    n->y = v->y / length;
    n->z = v->z / length;

    return LSRERR_OK;
}

int f32x3_triangle_normal(const f32x3* a, const f32x3* b, const f32x3* c, f32x3* n) {
    if (a == NULL || b == NULL || c == NULL || n == NULL) {
        return LSRERR_INVALID_ARGUMENT;
    }

    f32x3 e1, e2, cp;
    ZeroMemory(&cp, sizeof(f32x3));

    // 1. Calculate two edges of the triangle
    e1.x = b->x - a->x; e1.y = b->y - a->y; e1.z = b->z - a->z;
    e2.x = c->x - a->x; e2.y = c->y - a->y; e2.z = c->z - a->z;

    // 2. Compute normal (cross product)
    int result = f32x3_cross_product(&e1, &e2, &cp);
    if (result == LSRERR_OK) {
        return f32x3_normalize(&cp, n);
    }

    return result;
}

int f32x4_multiply_f32m4(const f32x4* v, const f32m4* m, f32x4* result) {
    if (v == NULL || m == NULL || result == NULL) {
        return LSRERR_INVALID_ARGUMENT;
    }

    result->x = v->x * m->m4x4[0][0] + v->y * m->m4x4[1][0] + v->z * m->m4x4[2][0] + v->w * m->m4x4[3][0];
    result->y = v->x * m->m4x4[0][1] + v->y * m->m4x4[1][1] + v->z * m->m4x4[2][1] + v->w * m->m4x4[3][1];
    result->z = v->x * m->m4x4[0][2] + v->y * m->m4x4[1][2] + v->z * m->m4x4[2][2] + v->w * m->m4x4[3][2];
    result->w = v->x * m->m4x4[0][3] + v->y * m->m4x4[1][3] + v->z * m->m4x4[2][3] + v->w * m->m4x4[3][3];

    return LSRERR_OK;
}

int f32m4_identity(f32m4* m) {
    if (m == NULL) {
        return LSRERR_INVALID_ARGUMENT;
    }

    ZeroMemory(m, sizeof(f32m4));

    m->m4x4[0][0] = 1.0f;
    m->m4x4[1][1] = 1.0f;
    m->m4x4[2][2] = 1.0f;
    m->m4x4[3][3] = 1.0f;

    return LSRERR_OK;
}

int f32m4_multiply(f32m4* m1, f32m4* m2, f32m4* result) {
    if (m1 == NULL || m2 == NULL || result == NULL) {
        return LSRERR_INVALID_ARGUMENT;
    }

    for (int r = 0; r < 4; r++) {
        for (int c = 0; c < 4; c++) {
            f32 sum = 0.0f;
            for (int k = 0; k < 4; k++) {
                sum += m1->m16[r * 4 + k] * m2->m16[k * 4 + c];
            }

            result->m16[r * 4 + c] = sum;
        }
    }

    return LSRERR_OK;
}

int f32m4_projection(f32m4* m, int w, int h, f32 min, f32 max) {
    if (m == NULL) {
        return LSRERR_INVALID_ARGUMENT;
    }

    const f32 a = (f32)w / (f32)h;
    const f32 fov = 90.0f * (f32)(M_PI / 180.0); // 90 degrees

    const f32 tan_fov = tanf(fov / 2.0f);

    m->m4x4[0][0] = 1.0f / (a * tan_fov); m->m4x4[0][1] = 0.0f; m->m4x4[0][2] = 0.0f; m->m4x4[0][3] = 0.0f;
    m->m4x4[1][0] = 0.0f; m->m4x4[1][1] = 1.0f / tan_fov; m->m4x4[1][2] = 0.0f; m->m4x4[1][3] = 0.0f;
    m->m4x4[2][0] = 0.0f; m->m4x4[2][1] = 0.0f; m->m4x4[2][2] = max / (max - min); m->m4x4[2][3] = 1.0f;
    m->m4x4[3][0] = 0.0f; m->m4x4[3][1] = 0.0f; m->m4x4[3][2] = -(min * max) / (max - min); m->m4x4[3][3] = 0.0f;

    return LSRERR_OK;
}

int f32m4_orthographic(f32m4* m, int w, int h, f32 min, f32 max) {
    if (m == NULL) {
        return LSRERR_INVALID_ARGUMENT;
    }

    m->m4x4[0][0] = 2.0f / (f32)w; m->m4x4[0][1] = 0.0f; m->m4x4[0][2] = 0.0f; m->m4x4[0][3] = 0.0f;
    m->m4x4[1][0] = 0.0f; m->m4x4[1][1] = -2.0f / (f32)h; m->m4x4[1][2] = 0.0f; m->m4x4[1][3] = 0.0f;
    m->m4x4[2][0] = 0.0f; m->m4x4[2][1] = 0.0f; m->m4x4[2][2] = 1.0f / (max - min); m->m4x4[2][3] = 0.0f;
    m->m4x4[3][0] = -1.0f; m->m4x4[3][1] = 1.0f; m->m4x4[3][2] = -min / (max - min); m->m4x4[3][3] = 1.0f;

    return LSRERR_OK;
}

int transform_f32m4(const transform* t, f32m4* m) {
    if (t == NULL || m == NULL) {
        return LSRERR_INVALID_ARGUMENT;
    }

    // 1. Convert angles from degrees to radians
    const f32 yaw = t->rotation.yaw * (f32)(M_PI / 180.0);
    const f32 pitch = t->rotation.pitch * (f32)(M_PI / 180.0);
    const f32 roll = t->rotation.roll * (f32)(M_PI / 180.0);

    // 2. Precompute sines and cosines
    const f32 sy = sinf(yaw);   const f32 cy = cosf(yaw);
    const f32 sp = sinf(pitch); const f32 cp = cosf(pitch);
    const f32 sr = sinf(roll);  const f32 cr = cosf(roll);

    // 3. Construct Row-Major World Matrix
    // (Scale * Rotation * Translation)

    // Row 0: Right Vector * Scale.X
    m->m4x4[0][0] = (cy * cr + sy * sp * sr) * t->scale.x;
    m->m4x4[0][1] = sr * cp * t->scale.x;
    m->m4x4[0][2] = (-sy * cr + cy * sp * sr) * t->scale.z;
    m->m4x4[0][3] = 0.0f;

    // Row 1: Up Vector * Scale.Y
    m->m4x4[1][0] = (-cy * sr + sy * sp * cr) * t->scale.y;
    m->m4x4[1][1] = cr * cp * t->scale.y;
    m->m4x4[1][2] = (sy * sr + cy * sp * cr) * t->scale.z;
    m->m4x4[1][3] = 0.0f;

    // Row 2: Forward Vector * Scale.Z
    m->m4x4[2][0] = sy * cp * t->scale.x;
    m->m4x4[2][1] = -sp * t->scale.y;
    m->m4x4[2][2] = cy * cp * t->scale.z;
    m->m4x4[2][3] = 0.0f;

    // Row 3: Translation Vector (Position)
    m->m4x4[3][0] = t->position.x;
    m->m4x4[3][1] = t->position.y;
    m->m4x4[3][2] = t->position.z;
    m->m4x4[3][3] = 1.0f;

    return LSRERR_OK;
}
