#pragma once

#include "esp_camera.h"
#include "human_face_detect.hpp"

#include <list>

#ifdef __cplusplus
extern "C" {
#endif

void face_preview_init(void);

/** Encode JPEG with face rectangles (yellow=unknown, green=recognized). */
void face_preview_update(camera_fb_t *fb,
                         const std::list<dl::detect::result_t> &faces,
                         int matched_id,
                         float matched_sim);

/** Copy latest JPEG into out_buf (returns false if no frame yet). */
bool face_preview_copy_jpeg(uint8_t *out_buf, size_t out_buf_size, size_t *out_len);

size_t face_preview_jpeg_size(void);

void face_preview_get_status(size_t *faces, int *matched_id, float *matched_sim);

#ifdef __cplusplus
}
#endif
