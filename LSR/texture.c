#include "texture.h"

#include <math.h>
#include <stdlib.h>

static int texture_sample_nearest(const texture* t, f32 level, f32 u, f32 v, f32x4* color);
static int texture_sample_bilinear(const texture* t, f32 level, f32 u, f32 v, f32x4* color);
static int texture_sample_trilinear(const texture* t, f32 level, f32 u, f32 v, f32x4* color);

static inline int texture_calculate_mip_levels(int w, int h) {
    int levels = 1;

    while (w > 1 || h > 1) {
        if (w > 1) w /= 2;
        if (h > 1) h /= 2;
        levels++;
    }

    return levels;
}

static inline int texture_get_mip_level_size(int value, int level) {
    if (level == 0) {
        return value;
    }

    int divisor = 1;
    for (int i = 0; i < level; i++) {
        divisor *= 2;
    }

    return value / divisor;
}

static int texture_generate_mip_level(texture_level* dst, const texture_level* src) {
    if (dst == NULL || src == NULL) {
        return LSRERR_INVALID_ARGUMENT;
    }

    const u32* src_pixels = (u32*)src->data;
    u32* dst_pixels = (u32*)dst->data;

    for (int y = 0; y < dst->height; y++) {
        for (int x = 0; x < dst->width; x++) {
            // Map destination pixel back to source coordinates
            const int src_x0 = x * 2;
            const int src_y0 = y * 2;

            // Handle NPOT odd edges by clamping the second sample row/col
            const int src_x1 = (src_x0 + 1 < src->width) ? (src_x0 + 1) : src_x0;
            const int src_y1 = (src_y0 + 1 < src->height) ? (src_y0 + 1) : src_y0;

            // Fetch 4 neighboring pixels from the source level
            const u32 p00 = src_pixels[src_y0 * src->width + src_x0];
            const u32 p10 = src_pixels[src_y0 * src->width + src_x1];
            const u32 p01 = src_pixels[src_y1 * src->width + src_x0];
            const u32 p11 = src_pixels[src_y1 * src->width + src_x1];

            // Extract color channels and average them (Box Filter)
            // Note: Shifting by 2 divides by 4.
            const u32 a = (((p00 >> 24) & 0xFF) + ((p10 >> 24) & 0xFF) + ((p01 >> 24) & 0xFF) + ((p11 >> 24) & 0xFF)) >> 2;
            const u32 r = (((p00 >> 16) & 0xFF) + ((p10 >> 16) & 0xFF) + ((p01 >> 16) & 0xFF) + ((p11 >> 16) & 0xFF)) >> 2;
            const u32 g = (((p00 >> 8) & 0xFF) + ((p10 >> 8) & 0xFF) + ((p01 >> 8) & 0xFF) + ((p11 >> 8) & 0xFF)) >> 2;
            const u32 b = (((p00 >> 0) & 0xFF) + ((p10 >> 0) & 0xFF) + ((p01 >> 0) & 0xFF) + ((p11 >> 0) & 0xFF)) >> 2;

            // Reconstruct the downsampled pixel
            dst_pixels[y * dst->width + x] = (a << 24) | (r << 16) | (g << 8) | (b << 0);
        }
    }

    return LSRERR_OK;
}

static int texture_allocate(const char* name, int w, int h, texture_type type, texture** outObj) {
    if (w < 0 || h < 0 || outObj == NULL) {
        return LSRERR_INVALID_ARGUMENT;
    }

    if (type < TEXTURE_TYPE_SIMPLE || type >= TEXTURE_TYPE_COUNT) {
        return LSRERR_INVALID_ARGUMENT;
    }

    texture* t = (texture*)malloc(sizeof(texture));
    if (t == NULL) {
        return LSRERR_OUT_OF_MEMORY;
    }

    ZeroMemory(t, sizeof(texture));

    t->width = w;
    t->height = h;

    if (name != NULL) {
        t->name = (char*)malloc(strlen(name) + 1);
        if (t->name == NULL) {
            texture_release(t);
            return LSRERR_OUT_OF_MEMORY;
        }

        strcpy(t->name, name);
    }

    t->level_count = type == TEXTURE_TYPE_SIMPLE
        ? 1 : texture_calculate_mip_levels(w, h);

    const size_t levels_size =
        t->level_count * sizeof(texture_level);

    t->levels = (texture_level*)malloc(levels_size);
    if (t->levels == NULL) {
        texture_release(t);
        return LSRERR_OUT_OF_MEMORY;
    }

    ZeroMemory(t->levels, levels_size);

    for (int i = 0; i < t->level_count; i++) {
        t->levels[i].width = texture_get_mip_level_size(w, i);
        t->levels[i].height = texture_get_mip_level_size(h, i);

        const size_t level_size =
            t->levels[i].width * t->levels[i].height * sizeof(u32);

        t->levels[i].data = malloc(level_size);
        if (t->levels[i].data == NULL) {
            texture_release(t);
            return LSRERR_OUT_OF_MEMORY;
        }

        ZeroMemory(t->levels[i].data, level_size);
    }

    *outObj = t;

    return LSRERR_OK;
}

int texture_create(const char* name, int w, int h, texture_type type, texture** outObj) {
    if (w < 0 || h < 0 || outObj == NULL) {
        return LSRERR_INVALID_ARGUMENT;
    }

    if (type < TEXTURE_TYPE_SIMPLE || type >= TEXTURE_TYPE_COUNT) {
        return LSRERR_INVALID_ARGUMENT;
    }

    texture* t = NULL;
    int result = texture_allocate(name, w, h, type, &t);
    if (result != LSRERR_OK) {
        return result;
    }

    *outObj = t;

    return LSRERR_OK;
}

void texture_release(texture* t) {
    if (t != NULL) {
        if (t->name != NULL) {
            free(t->name);
        }

        if (t->levels != NULL) {
            for (int i = 0; i < t->level_count; i++) {
                if (t->levels[i].data != NULL) {
                    free(t->levels[i].data);
                }
            }

            free(t->levels);
        }

        free(t);
    }
}

int texture_load_bitmap(texture* t, const bmp* image) {
    if (t == NULL || image == NULL) {
        return LSRERR_INVALID_ARGUMENT;
    }

    if (image->info.biPlanes != 1
        || image->info.biBitCount != 24
        || image->info.biHeight < 0) {
        return LSRERR_NOT_SUPPORTED;
    }

    const int image_width = image->info.biWidth;
    const int image_height = image->info.biHeight;

    // Calculate minimum width and height
    const int w = t->width < image_width ? t->width : image_width;
    const int h = t->height < image_height ? t->height : image_height;

    // Convert image pixels to 32-bit RGBA
    for (int x = 0; x < h; x++) {
        u32* dst = (u32*)(t->levels[0].data + w * x * sizeof(u32));
        const RGBTRIPLE* src =
            (RGBTRIPLE*)(image->data + image_width * x * sizeof(RGBTRIPLE));
        for (int xx = 0; xx < w; xx++) {
            dst[xx] = (0xFF << 24) | (src[xx].rgbtRed << 16)
                | (src[xx].rgbtGreen << 8) | src[xx].rgbtBlue;
        }
    }

    int result = LSRERR_OK;

    if (t->level_count != 1) {
        for (int i = 1; i < t->level_count; i++) {
            if ((result = texture_generate_mip_level(&t->levels[i], &t->levels[i - 1])) != LSRERR_OK) {
                return result;
            }
        }
    }

    return result;
}

int texture_set_color(texture* t, const RECT* rect, int level, u32 color) {
    if (t == NULL) {
        return LSRERR_INVALID_ARGUMENT;
    }

    if (level < 0 || t->level_count <= level) {
        return LSRERR_INVALID_ARGUMENT;
    }

    const texture_level* lvl = &t->levels[level];

    if (rect != NULL) {
        // Check if the rectangle is outside the boundaries
        if (rect->right < 0 || rect->bottom < 0
            || rect->left > lvl->width || rect->top > lvl->height) {
            return LSRERR_OK;
        }
    }

    // Calculate actual intersection of a provided rectangle and the texture
    const int min_x = rect == NULL ? 0
        : (0 < rect->left ? rect->left : 0);
    const int max_x = rect == NULL ? lvl->width
        : (lvl->width < rect->right ? lvl->width : rect->right);

    const int min_y = rect == NULL ? 0
        : (0 < rect->top ? rect->top : 0);
    const int max_y = rect == NULL ? lvl->height
        : (lvl->height < rect->bottom ? lvl->height : rect->bottom);

    for (int x = min_y; x < max_y; x++) {
        u32* pixels = (u32*)(lvl->data + x * lvl->width * sizeof(u32));
        for (int xx = min_x; xx < max_x; xx++) {
            pixels[xx] = color;
        }
    }

    return LSRERR_OK;
}

int texture_blt(texture* t, const texture* image, int dst_level, int src_level) {
    if (t == NULL || image == NULL) {
        return LSRERR_INVALID_ARGUMENT;
    }

    if (dst_level < 0 || t->level_count <= dst_level) {
        return LSRERR_INVALID_ARGUMENT;
    }

    if (src_level < 0 || image->level_count <= src_level) {
        return LSRERR_INVALID_ARGUMENT;
    }

    const int w = t->levels[dst_level].width < image->levels[src_level].width
        ? t->levels[dst_level].width : image->levels[src_level].width;
    const int h = t->levels[dst_level].height < image->levels[src_level].height
        ? t->levels[dst_level].height : image->levels[src_level].height;

    const texture_level* dst_image = &t->levels[src_level];
    const texture_level* src_image = &image->levels[src_level];

    for (int i = 0; i < h; i++) {
        const u32* src = (u32*)(src_image->data + i * src_image->width * sizeof(u32));
        u32* dst = (u32*)(dst_image->data + i * dst_image->width * sizeof(u32));
        CopyMemory(dst, src, w * sizeof(u32));
    }

    return LSRERR_OK;
}

int texture_get_point_color(const texture* t, int level, int x, int y, u32* color) {
    if (t == NULL || color == NULL) {
        return LSRERR_INVALID_ARGUMENT;
    }

    if (level < 0 || t->level_count <= level) {
        return LSRERR_INVALID_ARGUMENT;
    }

    const texture_level* lvl = &t->levels[level];

    if (x < 0 || lvl->width <= x || y < 0 || lvl->height <= y) {
        return LSRERR_INVALID_ARGUMENT;
    }

    const u32* pixels = (u32*)(lvl->data + y * lvl->width * sizeof(u32));
    *color = pixels[x];

    return LSRERR_OK;
}

int texture_set_point_color(texture* t, int level, int x, int y, u32 color) {
    if (t == NULL) {
        return LSRERR_INVALID_ARGUMENT;
    }

    if (level < 0 || t->level_count <= level) {
        return LSRERR_INVALID_ARGUMENT;
    }

    const texture_level* lvl = &t->levels[level];

    if (x < 0 || lvl->width <= x || y < 0 || lvl->height <= y) {
        return LSRERR_INVALID_ARGUMENT;
    }

    u32* pixels = (u32*)(lvl->data + y * lvl->width * sizeof(u32));
    pixels[x] = color;

    return LSRERR_OK;
}

int texture_get_point_depth(const texture* t, int level, int x, int y, f32* depth) {
    if (t == NULL || depth == NULL) {
        return LSRERR_INVALID_ARGUMENT;
    }

    if (level < 0 || t->level_count <= level) {
        return LSRERR_INVALID_ARGUMENT;
    }

    const texture_level* lvl = &t->levels[level];

    if (x < 0 || lvl->width <= x || y < 0 || lvl->height <= y) {
        return LSRERR_INVALID_ARGUMENT;
    }

    const f32* pixels = (f32*)(lvl->data + y * lvl->width * sizeof(f32));
    *depth = pixels[x];

    return LSRERR_OK;
}

int texture_set_point_depth(texture* t, int level, int x, int y, f32 depth) {
    if (t == NULL) {
        return LSRERR_INVALID_ARGUMENT;
    }

    if (level < 0 || t->level_count <= level) {
        return LSRERR_INVALID_ARGUMENT;
    }

    const texture_level* lvl = &t->levels[level];

    if (x < 0 || lvl->width <= x || y < 0 || lvl->height <= y) {
        return LSRERR_INVALID_ARGUMENT;
    }

    f32* pixels = (f32*)(lvl->data + y * lvl->width * sizeof(f32));
    pixels[x] = depth;

    return LSRERR_OK;
}

int texture_sample(const texture* t, texture_sampling sampling, f32 u, f32 v, f32 lod, f32x4* color) {
    if (t == NULL || color == NULL) {
        return LSRERR_INVALID_ARGUMENT;
    }

    if (sampling < TEXTURE_SAMPLING_NEAREST || sampling >= TEXTURE_SAMPLING_COUNT) {
        return LSRERR_INVALID_ARGUMENT;
    }

    u = clampf(u, 0.0f, 1.0f);
    v = clampf(v, 0.0f, 1.0f);

    const f32 level = clampf(lod, 0.0f, (f32)(t->level_count - 1));

    switch (sampling) {
    case TEXTURE_SAMPLING_NEAREST: {
        return texture_sample_nearest(t, level, u, v, color);
    } break;
    case TEXTURE_SAMPLING_BILINEAR: {
        return texture_sample_bilinear(t, level, u, v, color);
    } break;
    case TEXTURE_SAMPLING_TRILINEAR: {
        return texture_sample_trilinear(t, level, u, v, color);
    } break;
    }

    return LSRERR_OK;
}

int texture_sample_nearest(const texture* t, f32 level, f32 u, f32 v, f32x4* color) {
    if (t == NULL || color == NULL) {
        return LSRERR_INVALID_ARGUMENT;
    }

    if (level < 0 || t->level_count <= level) {
        return LSRERR_INVALID_ARGUMENT;
    }

    int result = LSRERR_OK;

    const int lvl = (int)level;

    const int x = (int)((f32)(t->levels[lvl].width - 1) * u);
    const int y = (int)((f32)(t->levels[lvl].height - 1) * v);

    u32 tex = 0;
    if ((result = texture_get_point_color(t, lvl, x, y, &tex)) == LSRERR_OK) {
        result = f32x4_set_color(color, tex);
    }

    return result;
}

int texture_sample_bilinear(const texture* t, f32 level, f32 u, f32 v, f32x4* color) {
    if (t == NULL || color == NULL) {
        return LSRERR_INVALID_ARGUMENT;
    }

    if (level < 0 || t->level_count <= level) {
        return LSRERR_INVALID_ARGUMENT;
    }

    const int lvl = (int)level;

    const f32 x = u * (f32)(t->levels[lvl].width - 1);
    const f32 y = v * (f32)(t->levels[lvl].height - 1);

    const int x0 = (int)x;
    const int y0 = (int)y;

    const int x1 = x0 + 1 == t->levels[lvl].width ? x0 : x0 + 1;
    const int y1 = y0 + 1 == t->levels[lvl].height ? y0 : y0 + 1;

    const f32 dx = x - (f32)x0;
    const f32 dy = y - (f32)y0;

    // Calculate interpolation weights
    const f32 w00 = (1.0f - dx) * (1.0f - dy);
    const f32 w10 = dx * (1.0f - dy);
    const f32 w01 = (1.0f - dx) * dy;
    const f32 w11 = dx * dy;

    u32 c00 = 0, c01 = 0, c10 = 0, c11 = 0;
    f32x4 col00, col01, col10, col11;

    int result = LSRERR_OK;

    // Get four corner pixels
    if ((result = texture_get_point_color(t, lvl, x0, y0, &c00)) == LSRERR_OK) {
        if ((result = texture_get_point_color(t, lvl, x0, y1, &c01)) == LSRERR_OK) {
            if ((result = texture_get_point_color(t, lvl, x1, y0, &c10)) == LSRERR_OK) {
                if ((result = texture_get_point_color(t, lvl, x1, y1, &c11)) == LSRERR_OK) {
                    // Convert the colors
                    if ((result = f32x4_set_color(&col00, c00)) == LSRERR_OK) {
                        if ((result = f32x4_set_color(&col01, c01)) == LSRERR_OK) {
                            if ((result = f32x4_set_color(&col10, c10)) == LSRERR_OK) {
                                if ((result = f32x4_set_color(&col11, c11)) == LSRERR_OK) {
                                    // Interpolate the colors
                                    color->a = col00.a * w00 + col10.a * w10 + col01.a * w01 + col11.a * w11;
                                    color->r = col00.r * w00 + col10.r * w10 + col01.r * w01 + col11.r * w11;
                                    color->g = col00.g * w00 + col10.g * w10 + col01.g * w01 + col11.g * w11;
                                    color->b = col00.b * w00 + col10.b * w10 + col01.b * w01 + col11.b * w11;
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    return result;
}

static int texture_sample_trilinear(const texture* t, f32 level, f32 u, f32 v, f32x4* color) {
    if (t == NULL || color == NULL) {
        return LSRERR_INVALID_ARGUMENT;
    }

    if (level < 0 || t->level_count <= level) {
        return LSRERR_INVALID_ARGUMENT;
    }

    int result = LSRERR_OK;
    const f32 min = floorf(level), max = ceilf(level);

    if (min == max) {
        return texture_sample_bilinear(t, level, u, v, color);
    }

    f32x4 sample_min, sample_max;
    if ((result = texture_sample_bilinear(t, min, u, v, &sample_min)) == LSRERR_OK) {
        if ((result = texture_sample_bilinear(t, max, u, v, &sample_max)) == LSRERR_OK) {
            CopyMemory(color, &sample_min, sizeof(f32x4));
            result = f32x4_interpolate(color, &sample_max, max - min);
        }
    }

    return result;
}

int texute_get_line_level_of_detail(const texture* t,
    f32 x0, f32 y0, f32 x1, f32 y1, f32 u0, f32 v0, f32 u1, f32 v1, f32* lod) {
    if (t == NULL || lod == NULL) {
        return LSRERR_INVALID_ARGUMENT;
    }
    
    // Calculate the length of the line in screen space
    const f32 dx = (x1 - x0);
    const f32 dy = (y1 - y0);
    const f32 screen_length_squared = dx * dx + dy * dy;

    // Guard against zero-length lines to avoid division by zero
    if (screen_length_squared < 0.0001f) {
        *lod = 0.0f;
        return LSRERR_OK;
    }

    // Convert normalized u,v coordinates to absolute texel coordinates
    const f32 tx0 = u0 * t->width, ty0 = v0 * t->height;
    const f32 tx1 = u1 * t->width, ty1 = v1 * t->height;

    // Calculate the texture length in texels
    const f32 dxt = tx1 - tx0;
    const f32 dyt = ty1 - ty0;

    // Calculate the length of the line in texture space
    const f32 texture_length_squared = dxt * dxt + dyt * dyt;

    // Calculate the level of detail based on the ratio of lengths
    const f32 level = 0.5f * log2f(texture_length_squared / screen_length_squared);
    
    // Keep LOD from going negative
    *lod = clampf(level, 0.0f, (f32)(t->level_count - 1));

    return LSRERR_OK;
}

int texute_get_triangle_level_of_detail(const texture* t,
    f32 x0, f32 y0, f32 x1, f32 y1, f32 x2, f32 y2,
    f32 u0, f32 v0, f32 u1, f32 v1, f32 u2, f32 v2, f32* lod) {
    if (t == NULL || lod == NULL) {
        return LSRERR_INVALID_ARGUMENT;
    }

    // Calculate the area of the triangle in screen space (cross product)
    const f32 screen_area_squared =
        0.5f * fabsf((x1 - x0) * (y2 - y0) - (x2 - x0) * (y1 - y0));

    // Guard against zero-area triangles to avoid division by zero
    if (screen_area_squared < 0.0001f) {
        *lod = 0.0f;
        return LSRERR_OK;
    }

    // Calculate texture area in texels
    // Convert normalized vertex u,v to actual pixel dimensions
    const f32 tx0 = u0 * t->width, ty0 = v0 * t->height;
    const f32 tx1 = u1 * t->width, ty1 = v1 * t->height;
    const f32 tx2 = u2 * t->width, ty2 = v2 * t->height;
    const f32 texel_area_squared =
        0.5f * fabsf((tx1 - tx0) * (ty2 - ty0) - (tx2 - tx0) * (ty1 - ty0));

    // Calculate the level of detail based on the ratio of areas
    const f32 level = 0.5f * log2f(texel_area_squared / screen_area_squared);

    // Keep LOD from going negative
    *lod = clampf(level, 0.0f, (f32)(t->level_count - 1));

    return LSRERR_OK;
}
