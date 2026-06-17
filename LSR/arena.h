#pragma once

#include "common.h"

typedef struct {
    u8*     value;
    size_t  size;
    size_t  capacity;
} arena;

arena* arena_create(size_t capacity);
void* arena_allocate(arena* a, size_t size);
void arena_clear(arena* a);
void arena_release(arena* a);
