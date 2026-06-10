#pragma once

#include "wavefront.h"

typedef struct mesh {
    const char* texture;
    vertex* vertexes;
    size_t vertex_count;
    int* indexes;
    size_t index_count;
} mesh;

typedef struct mg {
    size_t count;
    char** textures;
    mesh** meshes;
} mg;

int mg_create(wfobj* obj, mg** outObj);
void mg_release(mg* group);

int mg_get_aabb(mg* group, aabb* bounds);
