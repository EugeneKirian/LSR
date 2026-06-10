#pragma once

#include "common.h"

typedef struct wavefront wavefront;

typedef struct wfmtl {
    char* name;
    char* texture;
} wfmtl;

#define WF_MAX_FACE_VERTEX_COUNT 3

typedef struct wfface {
    char* group;
    char* material;
    int vertex[WF_MAX_FACE_VERTEX_COUNT];
    int texture[WF_MAX_FACE_VERTEX_COUNT];
    int normal[WF_MAX_FACE_VERTEX_COUNT];
} wfface;

typedef struct wfobj {
    char** libraries;
    size_t library_count;

    wfmtl* materials;
    size_t material_count;

    f32x3* vertexes;
    size_t vertex_count;
    f32x3* vertex_normals;
    size_t vertex_normal_count;
    f32x2* vertex_uvs;
    size_t vertex_uv_count;

    char** groups;
    size_t group_count;

    wfface* faces;
    size_t face_count;
} wfobj;

int wavefront_open(const char* path, wavefront** outObj);
void wavefront_release(wavefront* wf);

int wavefront_get_error(wavefront* wf, const char** outMessage);
int wavefront_get_object(wavefront* wf, wfobj** outObj);
