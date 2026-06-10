#include "mesh.h"

#include <float.h>
#include <stdlib.h>

static int obj_get_textures(wfobj* obj, size_t* outCount, char*** outTextures) {
    if (obj == NULL || outCount == NULL || outTextures == NULL) {
        return LSRERR_INVALID_ARGUMENT;
    }

    const size_t face_count = obj->face_count;

    if (face_count == 0) {
        *outCount = 0;
        *outTextures = NULL;
        return LSRERR_OK;
    }

    size_t texture_count = 0;
    char** textures = (char**)malloc(face_count * sizeof(char*));
    if (textures == NULL) {
        *outCount = 0;
        *outTextures = NULL;
        return LSRERR_OUT_OF_MEMORY;
    }

    ZeroMemory(textures, face_count * sizeof(char*));

    for (size_t i = 0; i < face_count; i++) {
        const char* texture = obj->faces[i].material;

        int found = FALSE;
        for (size_t ii = 0; ii < texture_count; ii++) {
            if (textures[ii] == texture /* NULL */
                || strcmp(textures[ii], texture) == 0) {
                found = TRUE;
                break;
            }
        }

        if (!found) {
            if (texture != NULL) {
                textures[texture_count] = (char*)malloc(strlen(texture) + 1);
                if (textures[texture_count] == NULL) {
                    for (size_t iii = 0; iii < texture_count; iii++) {
                        if (textures[iii] != NULL) {
                            free(textures[iii]);
                        }
                    }

                    free(textures);

                    *outTextures = NULL;
                    *outCount = 0;
                    return LSRERR_OUT_OF_MEMORY;
                }

                strcpy(textures[texture_count], texture);
            }

            texture_count++;
        }
    }

    *outCount = texture_count;
    *outTextures = textures;

    return LSRERR_OK;
}

static int obj_get_texture_counts(wfobj* obj, const char* texture, size_t* outVertexCount, size_t* outIndexCount) {
    if (obj == NULL || outVertexCount == NULL || outIndexCount == NULL) {
        return LSRERR_INVALID_ARGUMENT;
    }

    size_t index_count = 0, vertex_count = 0;

    const size_t face_count = obj->face_count;
    for (size_t i = 0; i < face_count; i++) {
        if (obj->faces[i].material == texture /* NULL*/
            || strcmp(obj->faces[i].material, texture) == 0) {
            vertex_count += 3;
            index_count += 3;
        }
    }

    *outVertexCount = vertex_count;
    *outIndexCount = index_count;

    return LSRERR_OK;
}

static int mg_allocate(size_t count, char** textures, mg** outObj) {
    if (outObj == NULL) {
        return LSRERR_INVALID_ARGUMENT;
    }

    mg* m = (mg*)malloc(sizeof(mg));
    if (m == NULL) {
        return LSRERR_OUT_OF_MEMORY;
    }

    ZeroMemory(m, sizeof(mg));

    m->count = count;
    m->textures = textures;
    m->meshes = (mesh**)malloc(count * sizeof(mesh*));
    if (m->meshes == NULL) {
        free(m);
        return LSRERR_OUT_OF_MEMORY;
    }

    ZeroMemory(m->meshes, count * sizeof(mesh*));

    *outObj = m;

    return LSRERR_OK;
}

static int mesh_allocate(size_t vertex_count, size_t index_count, mesh** outObj) {
    if (outObj == NULL) {
        return LSRERR_INVALID_ARGUMENT;
    }

    mesh* m = (mesh*)malloc(sizeof(mesh));
    if (m == NULL) {
        return LSRERR_OUT_OF_MEMORY;
    }

    m->vertex_count = vertex_count;
    m->vertexes = (vertex*)malloc(vertex_count * sizeof(vertex));
    if (m->vertexes == NULL) {
        return LSRERR_OUT_OF_MEMORY;
    }

    ZeroMemory(m->vertexes, vertex_count * sizeof(vertex));

    m->index_count = index_count;
    m->indexes = (int*)malloc(index_count * sizeof(int));
    if (m->indexes == NULL) {
        free(m->vertexes);
        return LSRERR_OUT_OF_MEMORY;
    }

    ZeroMemory(m->indexes, index_count * sizeof(int));

    *outObj = m;

    return LSRERR_OK;
}

static void mesh_release(mesh* m) {
    if (m != NULL) {
        if (m->indexes != NULL) {
            free(m->indexes);
        }

        if (m->vertexes != NULL) {
            free(m->vertexes);
        }
    }
}

static int mesh_initialize(wfobj* obj, const char* texture, mesh** outObj) {
    if (obj == NULL || outObj == NULL) {
        return LSRERR_INVALID_ARGUMENT;
    }

    // Get vertex and index count
    size_t index_count = 0, vertex_count = 0;
    int result = obj_get_texture_counts(obj, texture, &vertex_count, &index_count);
    if (result != LSRERR_OK) {
        return result;
    }

    // Allocate mesh
    mesh* m = NULL;
    if ((result = mesh_allocate(vertex_count, index_count, &m)) != LSRERR_OK) {
        return result;
    }

    m->texture = texture;

    // Populate mesh
    size_t index_index = 0, vertex_index = 0;

    const size_t face_count = obj->face_count;
    for (size_t i = 0; i < face_count; i++) {
        wfface* face = &obj->faces[i];
        if (face->material == texture /* NULL */
            || strcmp(face->material, texture) == 0) {
            // Reminder: indexes in obj are 1-based

            const int vxi[] = { face->vertex[0], face->vertex[1], face->vertex[2] };
            const int vti[] = { face->texture[0], face->texture[1], face->texture[2] };
            const int vni[] = { face->normal[0], face->normal[1], face->normal[2] };

            for (size_t x = 0; x < 3; x++) {
                // Position
                CopyMemory(&m->vertexes[vertex_index + x].position,
                    &obj->vertexes[vxi[x] - 1], sizeof(f32x3));

                // UV
                if (vti[x] != 0) {
                    CopyMemory(&m->vertexes[vertex_index + x].uv,
                        &obj->vertex_uvs[vti[x] - 1], sizeof(f32x2));
                }

                // Color
                m->vertexes[vertex_index + x].color = 0xFFFFFFFF; // White
            }

            // Normal
            f32x3 normal;
            ZeroMemory(&normal, sizeof(f32x3));
            
            f32x3_triangle_normal(&m->vertexes[0].position,
                &m->vertexes[1].position, &m->vertexes[2].position, &normal);

            for (size_t x = 0; x < 3; x++) {
                const f32x3* n = vni[x] == 0 ? &normal : &obj->vertex_normals[vni[x] - 1];
                CopyMemory(&m->vertexes[vertex_index + x].normal, n, sizeof(f32x3));
            }

            m->indexes[index_index + 0] = (int)(index_index + 0);
            m->indexes[index_index + 1] = (int)(index_index + 1);
            m->indexes[index_index + 2] = (int)(index_index + 2);

            index_index += 3;
            vertex_index += 3;
        }
    }

    *outObj = m;

    return LSRERR_OK;
}

int mg_create(wfobj* obj, mg** outObj) {
    if (obj == NULL || outObj == NULL) {
        return LSRERR_INVALID_ARGUMENT;
    }

    size_t count = 0;
    char** textures = NULL;

    // Get unique texture names (or lack thereof)
    // Each sub-mesh is grouped by the texture
    int result = obj_get_textures(obj, &count, &textures);
    if (result != LSRERR_OK) {
        return result;
    }

    // Allocate mesh group
    mg* group = NULL;
    if ((result = mg_allocate(count, textures, &group)) != LSRERR_OK) {
        return result;
    }

    // Create sub-mesh
    for (size_t i = 0; i < count; i++) {
        mesh* m = NULL;
        if ((result = mesh_initialize(obj, textures[i], &m)) != LSRERR_OK) {
            if (textures != NULL) {
                for (size_t x = 0; x < count; x++) {
                    if (textures[x] == NULL) {
                        free(textures[x]);
                    }
                }

                free(textures);
            }

            return result;
        }

        group->meshes[i] = m;
    }

    *outObj = group;

    return LSRERR_OK;
}

void mg_release(mg* group) {
    if (group != NULL) {
        const size_t count = group->count;

        if (group->meshes != NULL) {
            for (size_t i = 0; i < count; i++) {
                if (group->meshes[i] != NULL) {
                    mesh_release(group->meshes[i]);
                }
            }

            free(group->meshes);
        }

        if (group->textures != NULL) {
            for (size_t i = 0; i < count; i++) {
                if (group->textures[i] != NULL) {
                    free(group->textures[i]);
                }
            }

            free(group->textures);
        }

        free(group);
    }
}

int mg_get_aabb(mg* group, aabb* bounds) {
    if (group == NULL || bounds == NULL) {
        return LSRERR_INVALID_ARGUMENT;
    }

    const size_t mesh_count = group->count;
    if (mesh_count == 0) {
        ZeroMemory(bounds, sizeof(aabb));
        return LSRERR_OK;
    }

    bounds->min.x = FLT_MAX;
    bounds->max.x = -FLT_MAX;
    bounds->min.y = FLT_MAX;
    bounds->max.y = -FLT_MAX;
    bounds->min.z = FLT_MAX;
    bounds->max.z = -FLT_MAX;

    for (size_t i = 0; i < mesh_count; ++i) {
        const mesh* m = group->meshes[i];
        const size_t vertex_count = m->vertex_count;

        for (size_t ii = 0; ii < vertex_count; ii++) {
            const f32x3* vertex = &m->vertexes[ii].position;
            if (vertex->x < bounds->min.x) bounds->min.x = vertex->x;
            if (vertex->x > bounds->max.x) bounds->max.x = vertex->x;
            if (vertex->y < bounds->min.y) bounds->min.y = vertex->y;
            if (vertex->y > bounds->max.y) bounds->max.y = vertex->y;
            if (vertex->z < bounds->min.z) bounds->min.z = vertex->z;
            if (vertex->z > bounds->max.z) bounds->max.z = vertex->z;
        }
    }

    return LSRERR_OK;
}
