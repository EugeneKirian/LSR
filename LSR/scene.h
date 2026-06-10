#pragma once

#include "camera.h"
#include "mesh.h"
#include "texture.h"

typedef struct scene {
    struct {
        texture** textures;
        size_t count;
    } assets;
    struct {
        mg** meshes;
        transform** transforms;
        size_t count;
    } objects;
    camera* camera;
} scene;

int scene_create(const char* path, scene** outObj);
void scene_release(scene* s);
