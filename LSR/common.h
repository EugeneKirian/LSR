#pragma once

#include <stddef.h>

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#define LSRERR_OK                0
#define LSRERR_INVALID_ARGUMENT (-1)
#define LSRERR_OUT_OF_MEMORY    (-2)
#define LSRERR_FILE_NOT_FOUND   (-3)
#define LSRERR_FILE_READ_ERROR  (-4)
#define LSRERR_INVALID_FILE     (-5)
#define LSRERR_NOT_SUPPORTED    (-6)
#define LSRERR_INVALID_CALL     (-7)

typedef float f32;
typedef double f64;
typedef unsigned int u32;

typedef struct f32x2 {
    f32 x, y;
} f32x2;

typedef struct f32x3 {
    f32 x, y, z;
} f32x3;

typedef struct f32x4 {
    f32 x, y, z, w;
} f32x4;

typedef struct vertex {
    f32x3 position;
    f32x3 normal;
    u32 color;
    f32x3 uv;
} vertex;

// Direct3D-style row-major matrix
typedef struct f32m4 {
    union {
        f32 m4x4[4][4];
        f32 m16[16];
    };
} f32m4;

typedef struct transform {
    f32x3 scale;
    f32x3 position;
    f32x3 rotation; // Euler Angles
} transform;

typedef struct aabb {
    f32x3 min;
    f32x3 max;
} aabb;

int f32x3_cross_product(const f32x3* v1, const f32x3* v2, f32x3* cp);
int f32x3_normalize(const f32x3* v, f32x3* n);
int f32x3_triangle_normal(const f32x3* a, const f32x3* b, const f32x3* c, f32x3* n);

int f32m4_identity(f32m4* m);
