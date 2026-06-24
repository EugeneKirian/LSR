#include "frustrum.h"

int frustrum_is_outside_plane(const f32x4* vertex, frustrum_plane plane, int* outside) {
    if (vertex == NULL || outside == NULL) {
        return LSRERR_INVALID_ARGUMENT;
    }

    switch (plane) {
    case FRUSTRUM_PLANE_LEFT: { *outside = vertex->x < -vertex->w; } break;
    case FRUSTRUM_PLANE_RIGHT: { *outside = vertex->x > vertex->w; } break;
    case FRUSTRUM_PLANE_TOP: { *outside = vertex->y > vertex->w; } break;
    case FRUSTRUM_PLANE_BOTTOM: { *outside = vertex->y < -vertex->w; } break;
    case FRUSTRUM_PLANE_NEAR: { *outside = vertex->z < 0.0f; } break;
    case FRUSTRUM_PLANE_FAR: { *outside = vertex->z > vertex->w; } break;
    default: { *outside = FALSE; } break;
    }

    return LSRERR_OK;
}

int frustrum_get_plane_distance(const f32x4* vertex, frustrum_plane plane, f32* distance) {
    if (vertex == NULL || distance == NULL) {
        return LSRERR_INVALID_ARGUMENT;
    }

    switch (plane) {
    case FRUSTRUM_PLANE_LEFT: { *distance = vertex->w + vertex->x; } break;
    case FRUSTRUM_PLANE_RIGHT: { *distance = vertex->w - vertex->x; } break;
    case FRUSTRUM_PLANE_TOP: { *distance = vertex->w - vertex->y; } break;
    case FRUSTRUM_PLANE_BOTTOM: { *distance = vertex->w + vertex->y; } break;
    case FRUSTRUM_PLANE_NEAR: { *distance = vertex->z; } break;
    case FRUSTRUM_PLANE_FAR: { *distance = vertex->w - vertex->z; } break;
    default: { *distance = 0.0f; } break;
    }

    return LSRERR_OK;
}
