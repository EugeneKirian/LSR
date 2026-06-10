#include "arena.h"
#include "wavefront.h"

#include <stdio.h>

#define WF_MAX_ARENA_SIZE           (16*1024*1024)
#define WF_MAX_ERROR_MESSAGE_SIZE   512

struct wavefront {
    arena* arena;
    char* path;
    int error;
    char* message;
    wfobj* object;
};

typedef struct wfobj_info {
    size_t line_count;
    size_t library_count;
    size_t material_count;
    size_t vertex_count;
    size_t vertex_normal_count;
    size_t vertex_uv_count;
    size_t group_count;
    size_t face_count;
} wfobj_info;

static int make_path(arena* a, const char* path, const char* name, char** outPath) {
    if (a == NULL || path == NULL || name == NULL || outPath == NULL) {
        return LSRERR_INVALID_ARGUMENT;
    }

    const size_t length = strlen(path) + strlen(name);
    char* result = (char*)arena_allocate(a, length + 1);
    if (result == NULL) {
        return LSRERR_OUT_OF_MEMORY;
    }

    strcpy(result, path);

    char* last = strrchr(result, '\\');
    if (last == NULL) {
        last = strrchr(result, '/');
    }

    if (last != NULL) {
        *(last + 1) = NULL;
    }

    strcat(result, name);

    *outPath = result;

    return LSRERR_OK;
}

static int read_file(arena* a, const char* path, char** outContent) {
    if (a == NULL || path == NULL || outContent == NULL) {
        return LSRERR_INVALID_ARGUMENT;
    }

    HANDLE file = CreateFileA(path, GENERIC_READ,
        FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE) {
        return LSRERR_FILE_NOT_FOUND;
    }

    const DWORD size = GetFileSize(file, NULL);
    if (size == INVALID_FILE_SIZE) {
        CloseHandle(file);
        return LSRERR_INVALID_FILE;
    }

    char* content = (char*)arena_allocate(a, size + 1);
    if (content == NULL) {
        CloseHandle(file);
        return LSRERR_OUT_OF_MEMORY;
    }

    DWORD read = 0;
    if (!ReadFile(file, content, size, &read, NULL) || size != read) {
        CloseHandle(file);
        return LSRERR_FILE_READ_ERROR;
    }

    CloseHandle(file);
    content[size] = NULL;

    *outContent = content;

    return LSRERR_OK;
}

static const char* next_line(const char* str, size_t* lines) {
    if (str == NULL) {
        return NULL;
    }

    if (lines != NULL) {
        *lines = 0;
    }

    const char* ptr = str;

    // Skip all normal characters
    while (*ptr != NULL && iscntrl(*ptr) == 0) {
        ptr++;
    }

    // Skip all control characters
    while (*ptr != NULL && iscntrl(*ptr) != 0) {
        if (*ptr == '\n') {
            if (lines != NULL) {
                (*lines)++;
            }
        }

        ptr++;
    }

    return ptr;
}

static size_t get_line_length(const char* str) {
    if (str == NULL) {
        return 0;
    }

    const char* ptr = str;

    while (*ptr != NULL) {
        if (iscntrl(*ptr) != 0) {
            return (size_t)(ptr - str);
        }

        ptr++;
    }

    return (size_t)(ptr - str);
}

static int wavefront_load_material(wavefront* wf, const char* content) {
    if (wf == NULL || content == NULL) {
        return LSRERR_INVALID_ARGUMENT;
    }

    wfobj* obj = wf->object;
    const char* ptr = content;

    char* material = NULL;

    // Parse one line at a time until the end of the content
    while (ptr != NULL && *ptr != NULL) {
        if (iscntrl(*ptr) == 0) {
            switch (*ptr) {
            case 'm': {
                if (material != NULL) {
                    wfmtl* mtl = NULL;

                    for (size_t i = 0; i < obj->material_count; i++) {
                        if (strcmp(material, obj->materials[i].name) == 0) {
                            mtl = &obj->materials[i];
                            break;
                        }
                    }

                    if (mtl != NULL && mtl->texture != NULL) {
                        if (strncmp(ptr, "map_Kd ", 7) == 0) {
                            const size_t l = get_line_length(ptr);
                            char* name = (char*)arena_allocate(wf->arena, l + 1);
                            if (name == NULL) {
                                return LSRERR_OUT_OF_MEMORY;
                            }

                            if (sscanf(ptr, "map_Kd %s", name) != 1) {
                                return LSRERR_INVALID_FILE;
                            }

                            char* path = NULL;
                            if (make_path(wf->arena, wf->path, name, &path) == LSRERR_OK) {
                                mtl->texture = path;
                            }
                        }
                    }
                }
            } break;
            case 'n': {
                if (strncmp(ptr, "newmtl ", 7) == 0) {
                    const size_t l = get_line_length(ptr);
                    char* name = (char*)arena_allocate(wf->arena, l + 1);
                    if (name == NULL) {
                        return LSRERR_OUT_OF_MEMORY;
                    }

                    if (sscanf(ptr, "newmtl %s", name) != 1) {
                        return LSRERR_INVALID_FILE;
                    }

                    if (material == NULL || strcmp(material, name) != 0) {
                        material = name;
                    }
                }
            } break;
            }
        }

        ptr = next_line(ptr, NULL);
    }

    return LSRERR_OK;
}

static int wavefront_load_materials(wavefront* wf) {
    if (wf == NULL) {
        return LSRERR_INVALID_ARGUMENT;
    }

    for (size_t i = 0; i < wf->object->library_count; i++) {
        char* path = NULL;
        if (make_path(wf->arena, wf->path, wf->object->libraries[i], &path) == LSRERR_OK) {
            char* content = NULL;
            if (read_file(wf->arena, path, &content) == LSRERR_OK) {
                wavefront_load_material(wf, content);
            }
        }
    }

    return LSRERR_OK;
}

static int wavefront_allocate(arena* a, const char* path, wavefront** outObj) {
    if (a == NULL || path == NULL || outObj == NULL) {
        return LSRERR_INVALID_ARGUMENT;
    }

    wavefront* wf = (wavefront*)arena_allocate(a, sizeof(wavefront));
    if (wf == NULL) {
        return LSRERR_OUT_OF_MEMORY;
    }

    ZeroMemory(wf, sizeof(wavefront));

    wf->arena = a;
    wf->error = LSRERR_OK;

    wf->path = (char*)arena_allocate(a, strlen(path) + 1);
    if (wf->path == NULL) {
        wf->error = LSRERR_OUT_OF_MEMORY;
        return wf->error;
    }

    strcpy(wf->path, path);

    wf->message = (char*)arena_allocate(a, WF_MAX_ERROR_MESSAGE_SIZE);
    if (wf->message == NULL) {
        wf->error = LSRERR_OUT_OF_MEMORY;
        return wf->error;
    }

    wf->object = (wfobj*)arena_allocate(a, sizeof(wfobj));
    if (wf->object == NULL) {
        wf->error = LSRERR_OUT_OF_MEMORY;
        return wf->error;
    }

    ZeroMemory(wf->message, WF_MAX_ERROR_MESSAGE_SIZE);
    ZeroMemory(wf->object, sizeof(wfobj));

    *outObj = wf;

    return LSRERR_OK;
}

static int wavefront_get_object_info(wavefront* wf, const char* content, wfobj_info* info) {
    if (wf == NULL || content == NULL || info == NULL) {
        return LSRERR_INVALID_ARGUMENT;
    }

    ZeroMemory(info, sizeof(wfobj_info));

    const char* ptr = content;

    // Parse one line at a time until the end of the content
    while (ptr != NULL && *ptr != NULL) {
        if (iscntrl(*ptr) == 0) {
            switch (*ptr) {
            case 'f': {
                info->face_count++;
            } break;
            case 'g': {
                info->group_count++;
            } break;
            case 'm': {
                if (strncmp(ptr, "mtllib ", 7) == 0) {
                    info->library_count++;
                }
            } break;
            case 'u': {
                if (strncmp(ptr, "usemtl ", 7) == 0) {
                    info->material_count++;
                }
            } break;
            case 'v': {
                const char c = *(ptr + 1);

                switch (c) {
                case ' ': {
                    info->vertex_count++;
                } break;
                case 't': {
                    info->vertex_uv_count++;
                } break;
                case 'n': {
                    info->vertex_normal_count++;
                } break;
                }
            } break;
            }
        }

        ptr = next_line(ptr, NULL);
    }

    return LSRERR_OK;
}

static int wavefront_allocate_object(wavefront* wf, const wfobj_info* info) {
    if (wf == NULL || info == NULL) {
        return LSRERR_INVALID_ARGUMENT;
    }

    wf->object->libraries = (char**)arena_allocate(wf->arena, sizeof(char*) * info->library_count);
    if (wf->object->libraries == NULL) {
        return LSRERR_OUT_OF_MEMORY;
    }

    wf->object->materials = (wfmtl*)arena_allocate(wf->arena, sizeof(wfmtl) * info->material_count);
    if (wf->object->materials == NULL) {
        return LSRERR_OUT_OF_MEMORY;
    }

    wf->object->vertexes = (f32x3*)arena_allocate(wf->arena, sizeof(f32x3) * info->vertex_count);
    if (wf->object->vertexes == NULL) {
        return LSRERR_OUT_OF_MEMORY;
    }

    wf->object->vertex_normals = (f32x3*)arena_allocate(wf->arena, sizeof(f32x3) * info->vertex_normal_count);
    if (wf->object->vertex_normals == NULL) {
        return LSRERR_OUT_OF_MEMORY;
    }

    wf->object->vertex_uvs = (f32x2*)arena_allocate(wf->arena, sizeof(f32x2) * info->vertex_uv_count);
    if (wf->object->vertex_uvs == NULL) {
        return LSRERR_OUT_OF_MEMORY;
    }

    wf->object->groups = (char**)arena_allocate(wf->arena, sizeof(char*) * info->group_count);
    if (wf->object->groups == NULL) {
        return LSRERR_OUT_OF_MEMORY;
    }

    wf->object->faces = (wfface*)arena_allocate(wf->arena, sizeof(wfface) * info->face_count);
    if (wf->object->faces == NULL) {
        return LSRERR_OUT_OF_MEMORY;
    }

    return LSRERR_OK;
}

static int wavefront_parse_object(wavefront* wf, const wfobj_info* info, const char* content) {
    if (wf == NULL || info == NULL || content == NULL) {
        return LSRERR_INVALID_ARGUMENT;
    }

    int result = wavefront_allocate_object(wf, info);
    if (result != LSRERR_OK) {
        return result;
    }

    wfobj* obj = wf->object;
    const char* ptr = content;

    size_t line = 1;
    char* group = NULL;
    char* material = NULL;

    // Parse one line at a time until the end of the content
    while (ptr != NULL && *ptr != NULL) {
        if (iscntrl(*ptr) == 0) {
            switch (*ptr) {
            case 'f': {
                const char c = *(ptr + 1);

                if (c != ' ') {
                    wf->error = LSRERR_INVALID_FILE;
                    sprintf(wf->message, "Expected space after 'f' on line %zu", line);
                    return wf->error;
                }

                // Only triangulated faces are supported
                // each face vertex definition has 3 indexes: vertex, texture, and normal
                int indexes[WF_MAX_FACE_VERTEX_COUNT * 3];
                ZeroMemory(indexes, sizeof(int) * WF_MAX_FACE_VERTEX_COUNT * 3);

                int count = 0, slash = 0;
                const char* next = ptr + 2;
                while (iscntrl(*next) == 0) {
                    if (isspace(*next) != 0) {
                        slash = 0;
                        count++;
                    }

                    if (count >= WF_MAX_FACE_VERTEX_COUNT) {
                        wf->error = LSRERR_NOT_SUPPORTED;
                        sprintf(wf->message, "Face contains more than 3 vertexes on line %zu", line);
                        return wf->error;
                    }

                    if (isdigit(*next) != 0) {
                        if (indexes[count * WF_MAX_FACE_VERTEX_COUNT + slash] == 0) {
                            if (sscanf(next, "%d", &indexes[count * WF_MAX_FACE_VERTEX_COUNT + slash]) != 1) {
                                wf->error = LSRERR_INVALID_FILE;
                                sprintf(wf->message, "Failed to parse face vertex index on line %zu", line);
                                return wf->error;
                            }
                        }
                    }

                    if (*next == '/') {
                        slash++;
                    }

                    next++;
                }

                obj->faces[obj->face_count].group = group;
                obj->faces[obj->face_count].material = material;

                obj->faces[obj->face_count].vertex[0] = indexes[0 * WF_MAX_FACE_VERTEX_COUNT + 0];
                obj->faces[obj->face_count].vertex[1] = indexes[1 * WF_MAX_FACE_VERTEX_COUNT + 0];
                obj->faces[obj->face_count].vertex[2] = indexes[2 * WF_MAX_FACE_VERTEX_COUNT + 0];

                obj->faces[obj->face_count].texture[0] = indexes[0 * WF_MAX_FACE_VERTEX_COUNT + 1];
                obj->faces[obj->face_count].texture[1] = indexes[1 * WF_MAX_FACE_VERTEX_COUNT + 1];
                obj->faces[obj->face_count].texture[2] = indexes[2 * WF_MAX_FACE_VERTEX_COUNT + 1];

                obj->faces[obj->face_count].normal[0] = indexes[0 * WF_MAX_FACE_VERTEX_COUNT + 2];
                obj->faces[obj->face_count].normal[1] = indexes[1 * WF_MAX_FACE_VERTEX_COUNT + 2];
                obj->faces[obj->face_count].normal[2] = indexes[2 * WF_MAX_FACE_VERTEX_COUNT + 2];

                obj->face_count++;
            } break;
            case 'g': {
                const char c = *(ptr + 1);

                if (c != ' ') {
                    wf->error = LSRERR_INVALID_FILE;
                    sprintf(wf->message, "Expected space after 'g' on line %zu", line);
                    return wf->error;
                }

                const size_t l = get_line_length(ptr);
                char* name = (char*)arena_allocate(wf->arena, l + 1);
                if (name == NULL) {
                    wf->error = LSRERR_OUT_OF_MEMORY;
                    return wf->error;
                }

                if (sscanf(ptr, "g %s", name) != 1) {
                    wf->error = LSRERR_INVALID_FILE;
                    sprintf(wf->message, "Failed to parse group name on line %zu", line);
                    return wf->error;
                }

                if (group == NULL || strcmp(group, name) != 0) {
                    group = name;

                    int found = FALSE;
                    for (size_t i = 0; i < obj->group_count; i++) {
                        if (strcmp(obj->groups[i], name) == 0) {
                            found = TRUE;
                            break;
                        }
                    }

                    if (!found) {
                        obj->groups[obj->group_count++] = name;
                    }
                }
            } break;
            case 'm': {
                if (strncmp(ptr, "mtllib ", 7) == 0) {
                    const size_t l = get_line_length(ptr);
                    char* name = (char*)arena_allocate(wf->arena, l + 1);
                    if (name == NULL) {
                        wf->error = LSRERR_OUT_OF_MEMORY;
                        return wf->error;
                    }

                    if (sscanf(ptr, "mtllib %s", name) != 1) {
                        wf->error = LSRERR_INVALID_FILE;
                        sprintf(wf->message, "Failed to parse material library name on line %zu", line);
                        return wf->error;
                    }

                    int found = FALSE;

                    for (size_t i = 0; i < obj->library_count; i++) {
                        if (strcmp(obj->libraries[i], name) == 0) {
                            found = TRUE;
                            break;
                        }
                    }

                    if (!found) {
                        obj->libraries[obj->library_count++] = name;
                    }
                }
                else {
                    wf->error = LSRERR_INVALID_FILE;
                    sprintf(wf->message, "Unknown token on line % zu", line);
                    return wf->error;
                }
            } break;
            case 'u': {
                if (strncmp(ptr, "usemtl ", 7) == 0) {
                    const size_t l = get_line_length(ptr);
                    char* name = (char*)arena_allocate(wf->arena, l + 1);
                    if (name == NULL) {
                        wf->error = LSRERR_OUT_OF_MEMORY;
                        return wf->error;
                    }

                    if (sscanf(ptr, "usemtl %s", name) != 1) {
                        wf->error = LSRERR_INVALID_FILE;
                        sprintf(wf->message, "Failed to parse material name on line %zu", line);
                        return wf->error;
                    }

                    if (material == NULL || strcmp(material, name) != 0) {
                        material = name;

                        int found = FALSE;
                        for (size_t i = 0; i < obj->material_count; i++) {
                            if (strcmp(obj->materials[i].name, name) == 0) {
                                found = TRUE;
                                break;
                            }
                        }

                        if (!found) {
                            obj->materials[obj->material_count++].name = name;
                        }
                    }
                }
                else {
                    wf->error = LSRERR_INVALID_FILE;
                    sprintf(wf->message, "Unknown token on line % zu", line);
                    return wf->error;
                }
            } break;
            case 'v': {
                const char c = *(ptr + 1);

                switch (c) {
                case ' ': {
                    float x = 0.0f, y = 0.0f, z = 0.0f;

                    if (sscanf(ptr, "v %f %f %f", &x, &y, &z) != 3) {
                        wf->error = LSRERR_INVALID_FILE;
                        sprintf(wf->message, "Failed to parse vertex on line %zu", line);
                        return wf->error;
                    }

                    obj->vertexes[obj->vertex_count].x = x;
                    obj->vertexes[obj->vertex_count].y = y;
                    obj->vertexes[obj->vertex_count].z = z;
                    obj->vertex_count++;
                } break;
                case 't': {
                    const char cc = *(ptr + 2);

                    if (cc != ' ') {
                        wf->error = LSRERR_INVALID_FILE;
                        sprintf(wf->message, "Unknown token on line % zu", line);
                        return wf->error;
                    }

                    float x = 0.0f, y = 0.0f;

                    if (sscanf(ptr, "vt %f %f", &x, &y) != 2) {
                        wf->error = LSRERR_INVALID_FILE;
                        sprintf(wf->message, "Failed to parse vertex texture coordinates on line %zu", line);
                        return wf->error;
                    }

                    obj->vertex_uvs[obj->vertex_uv_count].x = x;
                    obj->vertex_uvs[obj->vertex_uv_count].y = y;
                    obj->vertex_uv_count++;
                } break;
                case 'n': {
                    const char cc = *(ptr + 2);

                    if (cc != ' ') {
                        wf->error = LSRERR_INVALID_FILE;
                        sprintf(wf->message, "Unknown token on line % zu", line);
                        return wf->error;
                    }

                    float x = 0.0f, y = 0.0f, z = 0.0f;

                    if (sscanf(ptr, "vn %f %f %f", &x, &y, &z) != 3) {
                        wf->error = LSRERR_INVALID_FILE;
                        sprintf(wf->message, "Failed to parse vertex normal on line %zu", line);
                        return wf->error;
                    }

                    obj->vertex_normals[obj->vertex_normal_count].x = x;
                    obj->vertex_normals[obj->vertex_normal_count].y = y;
                    obj->vertex_normals[obj->vertex_normal_count].z = z;
                    obj->vertex_normal_count++;
                } break;
                default: {
                    wf->error = LSRERR_INVALID_FILE;
                    sprintf(wf->message, "Unknown token on line % zu", line);
                    return wf->error;
                } break;
                }
            } break;
            }
        }

        size_t l = 0;
        ptr = next_line(ptr, &l);
        line += l;
    }

    if (obj->library_count != 0) {
        wf->error = wavefront_load_materials(wf);
    }

    return wf->error;
}

int wavefront_open(const char* path, wavefront** outObj) {
    if (path == NULL || outObj == NULL) {
        return LSRERR_INVALID_ARGUMENT;
    }

    arena* a = arena_create(WF_MAX_ARENA_SIZE);
    if (a == NULL) {
        return LSRERR_OUT_OF_MEMORY;
    }

    char* content = NULL;
    int result = read_file(a, path, &content);
    if (result != LSRERR_OK) {
        arena_release(a);
        return result;
    }

    wavefront* wf = NULL;
    if ((result = wavefront_allocate(a, path, &wf)) != LSRERR_OK) {
        arena_release(a);
        return result;
    }

    wfobj_info info;
    ZeroMemory(&info, sizeof(wfobj_info));

    if ((wf->error = wavefront_get_object_info(wf, content, &info)) == LSRERR_OK) {
        wf->error = wavefront_parse_object(wf, &info, content);
    }

    *outObj = wf;

    return wf->error;
}

void wavefront_release(wavefront* wf) {
    if (wf != NULL) {
        arena_release(wf->arena);
    }
}

int wavefront_get_error(wavefront* wf, const char** outMessage) {
    if (wf == NULL || outMessage == NULL) {
        return LSRERR_INVALID_ARGUMENT;
    }

    *outMessage = wf->message;

    return LSRERR_OK;
}

int wavefront_get_object(wavefront* wf, wfobj** outObj) {
    if (wf == NULL || outObj == NULL) {
        return LSRERR_INVALID_ARGUMENT;
    }

    if (wf->error == LSRERR_OK) {
        *outObj = wf->object;
    }

    return wf->error;
}
