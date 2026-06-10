#include <stdlib.h>

#include "arena.h"

#define ARENA_ALIGNMENT 64
#define ARENA_ALIGN(x) (((x) + (ARENA_ALIGNMENT - 1)) & ~(ARENA_ALIGNMENT - 1))

arena* arena_create(size_t capacity) {
    arena* a = (arena*)malloc(sizeof(arena));

    if (a == NULL) {
        return NULL;
    }

    char* value = (char*)malloc(ARENA_ALIGN(capacity));

    if (value == NULL) {
        free(a);
        return NULL;
    };

    a->size = 0;
    a->capacity = capacity;
    a->value = value;

    return a;
}

void* arena_allocate(arena* a, size_t size) {
    if (a == NULL) {
        return NULL;
    }

    const size_t aligned = ARENA_ALIGN(size);

    if (a->capacity < a->size + aligned) {
        return NULL;
    }

    char* result = a->value + a->size;
    a->size += aligned;

    return result;
}

void arena_clear(arena* a) {
    if (a == NULL) {
        return;
    }

    a->size = 0;
}

void arena_release(arena* a) {
    if (a != NULL) {
        free(a->value);
        free(a);
    }
}
