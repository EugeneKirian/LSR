#include "bmp.h"

#include <stdlib.h>

static int bmp_allocate(const char* path, int w, int h, bmp** outObj) {
    const size_t length = strlen(path);
    char* bitmap = (char*)malloc(length + 1);
    if (bitmap == NULL) {
        return LSRERR_OUT_OF_MEMORY;
    }

    strcpy(bitmap, path);

    const size_t size = w * (h >= 0 ? h : -h) * sizeof(RGBTRIPLE);
    void* pointer = malloc(size);
    if (pointer == NULL) {
        free(bitmap);
        return LSRERR_OUT_OF_MEMORY;
    }

    ZeroMemory(pointer, size);

    bmp* result = (bmp*)malloc(sizeof(bmp));
    if (result == NULL) {
        free(bitmap);
        free(pointer);
        return LSRERR_OUT_OF_MEMORY;
    }

    ZeroMemory(result, sizeof(bmp));

    result->info.biSize = sizeof(BITMAPINFOHEADER);
    result->info.biWidth = w;
    result->info.biHeight = h;
    result->info.biPlanes = 1;
    result->info.biBitCount = 24;
    result->info.biCompression = BI_RGB;

    result->path = bitmap;
    result->data = pointer;

    *outObj = result;

    return LSRERR_OK;
}

int bmp_open(const char* path, bmp** outObj) {
    if (path == NULL || outObj == NULL) {
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

    BITMAPFILEHEADER header;
    ZeroMemory(&header, sizeof(BITMAPFILEHEADER));

    DWORD read = 0;
    if (!ReadFile(file, &header, sizeof(BITMAPFILEHEADER), &read, NULL)
        || read != sizeof(BITMAPFILEHEADER)) {
        CloseHandle(file);
        return LSRERR_FILE_READ_ERROR;
    }

    // Validate the file header
    if (header.bfType != 0x4D42) {
        CloseHandle(file);
        return LSRERR_INVALID_FILE;
    }

    // Validate file size
    if (header.bfSize != size) {
        CloseHandle(file);
        return LSRERR_INVALID_FILE;
    }

    BITMAPINFOHEADER info;
    ZeroMemory(&info, sizeof(BITMAPINFOHEADER));
    if (!ReadFile(file, &info, sizeof(BITMAPINFOHEADER), &read, NULL)
        || read != sizeof(BITMAPINFOHEADER)) {
        CloseHandle(file);
        return LSRERR_FILE_READ_ERROR;
    }

    // Validate header size
    if (info.biSize != sizeof(BITMAPINFOHEADER)) {
        CloseHandle(file);
        return LSRERR_INVALID_FILE;
    }

    // Validate image properties
    if (info.biPlanes != 1 || info.biCompression != BI_RGB || info.biBitCount != 24) {
        CloseHandle(file);
        return LSRERR_NOT_SUPPORTED;
    }

    // Allocate object
    bmp* obj = NULL;
    int result = bmp_allocate(path, info.biWidth, info.biHeight, &obj);
    if (result != LSRERR_OK) {
        CloseHandle(file);
        return result;
    }

    // Allocate pixel data
    const DWORD length = GDI_DIBSIZE(info);

    // Read the pixel data
    if (!ReadFile(file, obj->data, length, &read, NULL) || read != length) {
        CloseHandle(file);
        bmp_release(obj);
        return LSRERR_FILE_READ_ERROR;
    }

    CloseHandle(file);

    *outObj = obj;
    
    return LSRERR_OK;
}

void bmp_release(bmp* image) {
    if (image != NULL) {
        free(image->path);
        free(image->data);
        free(image);
    }
}
