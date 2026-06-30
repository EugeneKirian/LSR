#pragma once

#include "common.h"

#define _USE_MATH_DEFINES
#include <math.h>

#define FIELD_OF_VIEW   60.0f   /* 60 degrees */

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

int f32x4_get_color(const f32x4* value, u32* color) {
    if (value == NULL || color == NULL) {
        return LSRERR_INVALID_ARGUMENT;
    }

    const u32 a = (u32)clampf(floorf(value->a * 255.0f), 0.0f, 255.0f);
    const u32 r = (u32)clampf(floorf(value->r * 255.0f), 0.0f, 255.0f);
    const u32 g = (u32)clampf(floorf(value->g * 255.0f), 0.0f, 255.0f);
    const u32 b = (u32)clampf(floorf(value->b * 255.0f), 0.0f, 255.0f);

    *color = (a << 24) | (r << 16) | (g << 8) | b;

    return LSRERR_OK;
}

int f32x4_set_color(f32x4* value, u32 color) {
    if (value == NULL) {
        return LSRERR_INVALID_ARGUMENT;
    }

    value->a = ((color >> 24) & 0xFF) / 255.0f;
    value->r = ((color >> 16) & 0xFF) / 255.0f;
    value->g = ((color >> 8) & 0xFF) / 255.0f;
    value->b = ((color >> 0) & 0xFF) / 255.0f;

    return LSRERR_OK;
}

int f32x4_dither(f32x4* color, int x, int y, f32 depth) {
    if (color == NULL) {
        return LSRERR_OUT_OF_MEMORY;
    }

    // Calculate dither intensity based on distance
    f32 intensity = clampf(1.0f - (depth / 5.0f), 0.0f, 5.0f);

    // Quadratic falloff for smoother transition
    intensity = intensity * intensity;

    // Only apply dithering when intensity is significant
    if (intensity > 0.05f) {
        // Generate noise value for this pixel position
        const f32 amount = 0.02f * intensity
            * ((f32)(noise(x, y) % 256) - 128.0f) / 128.0f; // Range: -1.0 to 1.0

        color->r += amount;
        color->g += amount;
        color->b += amount;
    }

    return LSRERR_OK;
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

int f32x4_perspective_divide(f32x4* value) {
    if (value == NULL) {
        return LSRERR_INVALID_ARGUMENT;
    }

    value->x /= value->w;
    value->y /= value->w;
    value->z /= value->w;

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
    const f32 fov = FIELD_OF_VIEW * (f32)(M_PI / 180.0);

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

u32 power_of_two_round_up(u32 value) {
    if (value <= 1) {
        return 1;
    }

    value--;
    value |= value >> 1;
    value |= value >> 2;
    value |= value >> 4;
    value |= value >> 8;
    value |= value >> 16;

    return value + 1;
}

int f32x4_interpolate(f32x4* result, const f32x4* value, f32 t) {
    if (result == NULL || value == NULL) {
        return LSRERR_INVALID_ARGUMENT;
    }

    result->a = result->a * t + value->a * (1.0f - t);
    result->r = result->r * t + value->r * (1.0f - t);
    result->g = result->g * t + value->g * (1.0f - t);
    result->b = result->b * t + value->b * (1.0f - t);

    return LSRERR_OK;
}

int f32x4_blend_color(f32x4* result, const f32x4* back, const f32x4* front) {
    if (result == NULL || back == NULL || front == NULL) {
        return LSRERR_INVALID_ARGUMENT;
    }

    if (front->a == 0.0f) {
        CopyMemory(result, back, sizeof(f32x4));
        return LSRERR_OK;
    }
    else if (back->a == 0.0f || front->a == 1.0f) {
        CopyMemory(result, front, sizeof(f32x4));
        return LSRERR_OK;
    }

    const f32 ba = back->a;
    const f32 fa = front->a;
    const f32 ra = fa + (ba * (1.0f - fa));

    result->a = ra;
    result->r = ((front->r * fa) + (back->r * ba * (1.0f - fa))) / ra;
    result->g = ((front->g * fa) + (back->g * ba * (1.0f - fa))) / ra;
    result->b = ((front->b * fa) + (back->b * ba * (1.0f - fa))) / ra;

    return LSRERR_OK;
}
