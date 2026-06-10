#pragma once

#include "common.h"

typedef struct bmp {
    char*               path;
    BITMAPINFOHEADER    info;
    char*               data;
} bmp;

int bmp_open(const char* path, bmp** outObj);
void bmp_release(bmp* image);
