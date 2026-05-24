#pragma once

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Runs continuous detect/recognize loop (never returns). */
esp_err_t face_app_run(const char *db_path);

#ifdef __cplusplus
}
#endif
