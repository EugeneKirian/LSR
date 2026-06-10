#include "scene.h"

#include <stdio.h>
#include <stdlib.h>

#define SCENE_DEFAULT_TEXTURE_SIZE      128

static int scene_allocate(scene** outObj) {
    if (outObj == NULL) {
        return LSRERR_INVALID_ARGUMENT;
    }

    scene* s = (scene*)malloc(sizeof(scene));
    if (s == NULL) {
        return LSRERR_OUT_OF_MEMORY;
    }

    ZeroMemory(s, sizeof(scene));

    *outObj = s;

    return LSRERR_OK;
}

static int scene_allocate_objects(scene* s, size_t count) {
    if (s == NULL) {
        return LSRERR_INVALID_ARGUMENT;
    }

    const size_t mesh_size = count * sizeof(mg*);
    s->objects.meshes = (mg**)malloc(mesh_size);
    if (s->objects.meshes == NULL) {
        return LSRERR_OUT_OF_MEMORY;
    }

    ZeroMemory(s->objects.meshes, mesh_size);

    const size_t transform_size = count * sizeof(transform*);
    s->objects.transforms = (transform**)malloc(transform_size);
    if (s->objects.transforms == NULL) {
        return LSRERR_OUT_OF_MEMORY;
    }

    ZeroMemory(s->objects.transforms, transform_size);

    s->objects.count = count;

    return LSRERR_OK;
}

static int scene_allocate_textures(scene* s, size_t count) {
    if (s == NULL) {
        return LSRERR_INVALID_ARGUMENT;
    }

    const size_t size = count * sizeof(scene*);
    s->assets.textures = (texture*)malloc(size);
    if (s->assets.textures == NULL) {
        return LSRERR_OUT_OF_MEMORY;
    }

    ZeroMemory(s->assets.textures, size);

    return LSRERR_OK;
}

int scene_create(const char* path, scene** outObj) {
    if (path == NULL || outObj == NULL) {
        return LSRERR_INVALID_ARGUMENT;
    }

    char complete[MAX_PATH];
    if (GetFullPathNameA(path, MAX_PATH, complete, NULL) == 0) {
        return LSRERR_FILE_NOT_FOUND;
    }

    scene* s = NULL;
    int result = scene_allocate(&s);
    if (result != LSRERR_OK) {
        return result;
    }

    wavefront* wf = NULL;
    if ((result = wavefront_open(complete, &wf)) != LSRERR_OK) {
        if (wf != NULL) {
            char* message = NULL;
            wavefront_get_error(wf, &message);
            printf("Error: %s\n", message);
            wavefront_release(wf);
        }

        scene_release(s);

        return result;
    }

    wfobj* obj = NULL;
    if ((result = wavefront_get_object(wf, &obj)) != LSRERR_OK) {
        wavefront_release(wf);
        scene_release(s);
        return result;
    }

    mg* mesh = NULL;
    if ((result = mg_create(obj, &mesh)) != LSRERR_OK) {
        wavefront_release(wf);
        scene_release(s);
        return result;
    }

    if ((result = scene_allocate_objects(s, 1)) != LSRERR_OK) {
        wavefront_release(wf);
        scene_release(s);
        return result;
    }

    // Objects
    s->objects.meshes[0] = mesh;

    s->objects.transforms[0] = (transform*)malloc(sizeof(transform));
    if (s->objects.transforms[0] == NULL) {
        wavefront_release(wf);
        scene_release(s);
        return LSRERR_OUT_OF_MEMORY;
    }

    ZeroMemory(s->objects.transforms[0], sizeof(transform));

    s->objects.transforms[0]->scale.x = 1.0f;
    s->objects.transforms[0]->scale.y = 1.0f;
    s->objects.transforms[0]->scale.z = 1.0f;

    // Allocate textures
    if ((result = scene_allocate_textures(s, mesh->count)) != LSRERR_OK) {
        wavefront_release(wf);
        scene_release(s);
        return result;
    }

    // Load mesh textures
    for (size_t i = 0; i < mesh->count; i++) {
        const char* file = NULL;
        const char* name = NULL;
        const size_t material_count = obj->material_count;

        for (size_t x = 0; x < material_count; x++) {
            if (obj->materials[x].name != NULL
                && strcmp(obj->materials[x].name, mesh->textures[i]) == 0) {
                name = obj->materials[x].name;
                file = obj->materials[x].texture;
                break;
            }
        }

        if (file != NULL) {
            bmp* image = NULL;
            if ((result = bmp_open(file, &image)) == LSRERR_OK) {
                if ((result = texture_create(name, image->info.biWidth,
                    image->info.biHeight, &s->assets.textures[s->assets.count])) == LSRERR_OK) {
                    if ((result = texture_load_bitmap(s->assets.textures[s->assets.count], image)) == LSRERR_OK) {
                        s->assets.count++;
                    }
                }

                bmp_release(image);
            }
        }
    }

    wavefront_release(wf);

    // Camera
    {
        aabb bounds;
        ZeroMemory(&bounds, sizeof(aabb));
        if ((result = mg_get_aabb(s->objects.meshes[0], &bounds)) != LSRERR_OK) {
            scene_release(s);
            return result;
        }

        f32x3 t = s->objects.transforms[0]->position;

        f32x3 center;
        center.x = (bounds.min.x + t.x + bounds.max.x + t.x) / 2.0f;
        center.y = (bounds.min.y + t.y + bounds.max.x + t.y) / 2.0f;

        const f32 zz = (bounds.min.z + t.z + bounds.max.z + t.z);
        center.z = zz / 2.0f - zz;

        // Camera
        if ((result = camera_create(&center, &s->camera)) != LSRERR_OK) {
            scene_release(s);
            return result;
        }
    }

    *outObj = s;

    return LSRERR_OK;
}

void scene_release(scene* s) {
    if (s != NULL) {
        // Assets
        if (s->assets.textures != NULL) {
            const size_t count = s->assets.count;
            for (size_t i = 0; i < count; i++) {
                if (s->assets.textures[i] != NULL) {
                    free(s->assets.textures[i]);
                }
            }

            free(s->assets.textures);
        }

        // Objects
        if (s->objects.meshes != NULL) {
            const size_t count = s->objects.count;
            for (size_t i = 0; i < count; i++) {
                if (s->objects.meshes[i] != NULL) {
                    mg_release(s->objects.meshes[i]);
                }
            }

            free(s->objects.meshes);
        }

        if (s->objects.transforms != NULL) {
            const size_t count = s->objects.count;
            for (size_t i = 0; i < count; i++) {
                if (s->objects.transforms[i] != NULL) {
                    free(s->objects.transforms[i]);
                }
            }

            free(s->objects.transforms);
        }

        // Camera
        if (s->camera != NULL) {
            camera_release(s->camera);
        }

        free(s);
    }
}
