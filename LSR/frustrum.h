#pragma once
#include "common.h"

typedef enum frustrum_plane {
    FRUSTRUM_PLANE_LEFT         = 0,
    FRUSTRUM_PLANE_RIGHT        = 1,
    FRUSTRUM_PLANE_TOP          = 2,
    FRUSTRUM_PLANE_BOTTOM       = 3,
    FRUSTRUM_PLANE_NEAR         = 4,
    FRUSTRUM_PLANE_FAR          = 5,
    FRUSTRUM_PLANE_COUNT        = 6,
    FRUSTRUM_PLANE_FORCE_DWORD  = 0x7FFFFFFF,
} frustrum_plane;

int frustrum_is_outside_plane(const f32x4* vertex, frustrum_plane plane, int* outside);
int frustrum_get_plane_distance(const f32x4* vertex, frustrum_plane plane, f32* distance);
