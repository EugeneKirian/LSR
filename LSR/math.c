#pragma once

#include "common.h"

#include <math.h>

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

    // Calculate two edges of the triangle
    const f32x3 e1 = { b->x - a->x, b->y - a->y, b->z - a->z };
    const f32x3 e2 = { c->x - a->x, c->y - a->y, c->z - a->z };

    f32x3 cp;
    ZeroMemory(&cp, sizeof(f32x3));

    // Compute normal
    int result = f32x3_cross_product(&e1, &e2, &cp);
    if (result == LSRERR_OK) {
        return f32x3_normalize(&cp, n);
    }

    return result;
}


int f32m4_identity(f32m4* m) {
    if (m == NULL) {
        return LSRERR_INVALID_ARGUMENT;
    }

    ZeroMemory(m, sizeof(f32m4));

    m->m4x4[0][0] = 1.0f;
    m->m4x4[1][1] = 1.0f;
    m->m4x4[2][2] = 1.0f;
    m->m4x4[3][4] = 1.0f;

    return LSRERR_OK;
}
