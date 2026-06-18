#include "font.h"

#include <math.h>
#include <stdlib.h>

#define FONT_MAX_GLYPH_COUNT    256
#define FONT_MAX_PADDING_SIZE   2

typedef struct glyph {
    int x, y;
    int width, height;
    int advanceX, advanceY;
    int bearingX, bearingY;
} glyph;

struct font {
    texture texture;
    TEXTMETRICA metrics;
    glyph glyphs[FONT_MAX_GLYPH_COUNT];
    u32 front, back;
};

static const MAT2 identity = { {0,1}, {0,0}, {0,0}, {0,1} };

static int font_allocate(const char* name, font** outObj) {
    if (name == NULL || outObj == NULL) {
        return LSRERR_INVALID_ARGUMENT;
    }

    font* f = (font*)malloc(sizeof(font));
    if (f == NULL) {
        return LSRERR_OUT_OF_MEMORY;
    }

    ZeroMemory(f, sizeof(font));

    const size_t length = strlen(name) + 1;
    f->texture.name = (char*)malloc(length);
    if (f->texture.name == NULL) {
        font_release(f);
        return LSRERR_OUT_OF_MEMORY;
    }

    strcpy(f->texture.name, name);

    *outObj = f;

    return LSRERR_OK;
}

int font_create(const char* name, int size, int bold, u32 front, u32 back, font** outObj) {
    if (name == NULL || outObj == NULL) {
        return LSRERR_INVALID_ARGUMENT;
    }

    if (size < 1) {
        return LSRERR_INVALID_ARGUMENT;
    }

    HDC hdc = NULL;
    HFONT fnt = NULL;

    font* f = NULL;
    int result = font_allocate(name, &f);
    if (result != LSRERR_OK) {
        return result;
    }

    f->front = front;
    f->back = back;

    // Create HDC
    if ((hdc = CreateCompatibleDC(NULL)) == NULL) {
        result = LSRERR_INTERNAL_ERROR;
        goto error;
    }

    // Create font object
    if ((fnt = CreateFontA(size, 0, 0, 0, bold ? FW_BOLD : FW_NORMAL,
        FALSE, FALSE, FALSE, ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, name)) == NULL) {
        result = LSRERR_INTERNAL_ERROR;
        goto error;
    }

    SelectObject(hdc, fnt);
    GetTextMetricsA(hdc, &f->metrics);

    // Calculate texture atlas size
    u32 max_width = 0, max_height = 0;

    // Calculate maximum width and height of font characters
    for (u32 i = 0; i < FONT_MAX_GLYPH_COUNT; i++) {
        GLYPHMETRICS gm;
        if (GetGlyphOutlineA(hdc, i, GGO_METRICS, &gm, 0, NULL, &identity) != GDI_ERROR) {
            if (gm.gmBlackBoxY > max_height) {
                max_height = gm.gmBlackBoxY;
            }
            if (gm.gmBlackBoxX > max_width) {
                max_width = gm.gmBlackBoxX;
            }
        }
    }

    // Calculate area needed for the atlas
    f->texture.width = f->texture.height = power_of_two_round_up(
        (u32)(sqrtf((f32)((max_width + FONT_MAX_PADDING_SIZE)
            * (max_height + FONT_MAX_PADDING_SIZE) * FONT_MAX_GLYPH_COUNT)) + 1.0f));

    // Allocate font atlas texture
    const size_t atlas_size = f->texture.width * f->texture.height * sizeof(u32);
    f->texture.data = (u8*)malloc(atlas_size);
    if (f->texture.data == NULL) {
        result = LSRERR_OUT_OF_MEMORY;
        goto error;
    }

    // Set background color for the font atlas texture
    for (int y = 0; y < f->texture.height; y++) {
        u32* row = (u32*)(f->texture.data + (y * f->texture.width) * sizeof(u32));
        for (int x = 0; x < f->texture.width; x++) {
            row[x] = back;
        }
    }

    // Place each glyph on the atlas texture
    int current_x = FONT_MAX_PADDING_SIZE, current_y = FONT_MAX_PADDING_SIZE;
    for (u32 i = 0; i < FONT_MAX_GLYPH_COUNT; i++) {
        GLYPHMETRICS gm;
        if (GetGlyphOutlineA(hdc, i, GGO_METRICS, &gm, 0, NULL, &identity) != GDI_ERROR) {
            // Store glyph info
            f->glyphs[i].x = current_x;
            f->glyphs[i].y = current_y;
            f->glyphs[i].width = gm.gmBlackBoxX;
            f->glyphs[i].height = gm.gmBlackBoxY;
            f->glyphs[i].advanceX = gm.gmCellIncX;
            f->glyphs[i].advanceY = gm.gmCellIncY;
            f->glyphs[i].bearingX = gm.gmptGlyphOrigin.x;
            f->glyphs[i].bearingY = gm.gmptGlyphOrigin.y;

            // Update current position
            current_x += max_width + FONT_MAX_PADDING_SIZE;

            // Check if we need to move to next row
            if (current_x + max_width > f->texture.width) {
                current_x = FONT_MAX_PADDING_SIZE;
                current_y += max_height + FONT_MAX_PADDING_SIZE;
            }

            // Check if got outside the atlas boundaries
            if (current_x >= f->texture.width || current_y >= f->texture.height) {
                result = LSRERR_INTERNAL_ERROR;
                goto error;
            }
        }
    }

    // Allocate temporary buffer for one character to be stored in
    const size_t buffer_size = max_width * max_height * sizeof(u8);
    u8* buffer = (u8*)malloc(buffer_size);
    if (buffer == NULL) {
        result = LSRERR_OUT_OF_MEMORY;
        goto error;
    }

    // Rasterize glyphs
    for (u32 i = 0; i < FONT_MAX_GLYPH_COUNT; i++) {
        GLYPHMETRICS gm;
        DWORD curent_buffer_size = 0;
        if ((curent_buffer_size = GetGlyphOutlineA(hdc, i, GGO_GRAY8_BITMAP, &gm, 0, NULL, &identity)) != GDI_ERROR) {
            if (buffer_size < curent_buffer_size) {
                result = LSRERR_INTERNAL_ERROR;
                goto error;
            }

            if (gm.gmBlackBoxX > 0 && gm.gmBlackBoxY > 0) {
                if (GetGlyphOutlineA(hdc, i, GGO_GRAY8_BITMAP, &gm, curent_buffer_size, buffer, &identity) != GDI_ERROR) {
                    u32 pitch = (gm.gmBlackBoxX + 3) & ~3;
                    // Convert gray-scale transparency gradient (0 to 64) to the ARGB colors
                    for (u32 y = 0; y < gm.gmBlackBoxY; y++) {
                        u32* pixels = (u32*)(f->texture.data
                            + (f->glyphs[i].y + y) * f->texture.width * sizeof(u32));

                        for (u32 x = 0; x < gm.gmBlackBoxX; x++) {
                            u32 index = y * gm.gmBlackBoxX + x;
                            if (index < curent_buffer_size) {
                                u32 scale = (u32)buffer[y * pitch + x];
                                u32 alpha = scale == 64 ? 255 : ((255 * scale) / 64);

                                // Assemble final blended color
                                u32 color = color_blend(back,
                                    ((alpha & 0xFF) << 24) | (front & 0x00FFFFFF));

                                pixels[f->glyphs[i].x + x] = color;
                            }
                        }
                    }
                }
            }
        }
    }

    free(buffer);

    DeleteObject(fnt);
    DeleteDC(hdc);

    *outObj = f;

    return LSRERR_OK;

error:
    if (fnt != NULL) {
        DeleteObject(fnt);
    }

    if (hdc != NULL) {
        DeleteDC(hdc);
    }

    font_release(f);

    return result;
}

void font_release(font* f) {
    if (f != NULL) {
        if (f->texture.name != NULL) {
            free(f->texture.name);
        }

        if (f->texture.data != NULL) {
            free(f->texture.data);
        }

        free(f);
    }
}

int font_get_texture(const font* f, const texture** t) {
    if (f == NULL || t == NULL) {
        return LSRERR_INVALID_ARGUMENT;
    }

    *t = &f->texture;

    return LSRERR_OK;
}

int font_get_item(const font* f, u32 c, fqd* quad) {
    if (f == NULL || quad == NULL) {
        return LSRERR_INVALID_ARGUMENT;
    }

    if (FONT_MAX_GLYPH_COUNT <= c) {
        return LSRERR_INVALID_ARGUMENT;
    }

    const glyph* g = &f->glyphs[c];

    const f32 minx = (f32)g->x;
    const f32 maxx = (f32)(g->x + g->width);
    const f32 miny = (f32)g->y;
    const f32 maxy = (f32)(g->y + g->height);

    const f32 tw = (f32)f->texture.width;
    const f32 th = (f32)f->texture.height;

    quad->x = g->bearingX;
    quad->y = g->bearingY;
    quad->width = g->width;
    quad->height = g->height;
    quad->span = g->advanceX;

    quad->uv[0].x = (f32)minx / tw;
    quad->uv[0].y = (f32)miny / th;

    quad->uv[1].x = (f32)minx / tw;
    quad->uv[1].y = (f32)maxy / th;

    quad->uv[2].x = (f32)maxx / tw;
    quad->uv[2].y = (f32)maxy / th;

    quad->uv[3].x = (f32)maxx / tw;
    quad->uv[3].y = (f32)miny / th;

    return LSRERR_OK;
}

int font_get_height(const font* f, int* height) {
    if (f == NULL || height == NULL) {
        return LSRERR_INVALID_ARGUMENT;
    }

    *height = f->metrics.tmHeight + f->metrics.tmExternalLeading;

    return LSRERR_OK;
}
