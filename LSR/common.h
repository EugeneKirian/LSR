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
typedef unsigned char u8;
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
    f32x2 uv;
} vertex;

// Direct3D-style row-major matrix
typedef struct f32m4 {
    union {
        f32 m4x4[4][4];
        f32 m16[16];
    };
} f32m4;

typedef struct euler {
    f32 roll, pitch, yaw;
} euler;

typedef struct transform {
    f32x3 scale;
    f32x3 position;
    euler rotation;
} transform;

typedef struct aabb {
    f32x3 min;
    f32x3 max;
} aabb;

#define DIRECTION_COUNT  6

typedef enum direction {
    DIRECTION_NONE          = 0x0,
    DIRECTION_FORWARD       = 0x1,
    DIRECTION_BACKWARD      = 0x2,
    DIRECTION_LEFT          = 0x4,
    DIRECTION_RIGHT         = 0x8,
    DIRECTION_UPWARD        = 0x10,
    DIRECTION_DOWNWARD      = 0x20,
    DIRECTION_FORCE_DWORD   = 0x7FFFFFFF
} direction;

int f32x3_add_f32x3(f32x3* v, const f32x3* value);
int f32x3_cross_product(const f32x3* v1, const f32x3* v2, f32x3* cp);
int f32x3_normalize(const f32x3* v, f32x3* n);
int f32x3_triangle_normal(const f32x3* a, const f32x3* b, const f32x3* c, f32x3* n);

int f32x4_multiply_f32m4(const f32x4* v, const f32m4* m, f32x4* result);

int f32m4_identity(f32m4* m);
int f32m4_multiply(f32m4* m1, f32m4* m2, f32m4* result);
int f32m4_projection(f32m4* m, int w, int h, f32 min, f32 max);
