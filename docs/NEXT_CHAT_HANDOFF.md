# Handoff для следующего чата — Face Recognition (ESP-WHO / esp-dl)

> **Скопируйте этот файл (или ссылку на него) в начало нового чата**, чтобы агент сразу имел контекст.

---

## 1. Цель проекта

**Устройство:** ESP32-S3-N16R8-CAM  
- камера **OV3660**  
- **SD 32 GB** (SD_MMC 1-bit)  
- **8 MB PSRAM**, 16 MB flash (маркировка N16R8)

**Задача:** распознавание лиц (детекция + enrollment + «кто в кадре»).  
**Позже:** события в **Home Assistant** (MQTT/HTTP) — не в первом MVP.

**Сейчас не нужно:** обучение CNN на ПК, Edge Impulse, свой TFLite с нуля.

---

## 2. Выбранный стек

| Компонент | Решение |
|-----------|---------|
| Основа | **ESP-WHO** (логика enrollment + live camera) |
| ML / модели | **esp-dl 3.2.3** (`HumanFaceDetect`, `HumanFaceRecognizer`) |
| Toolchain | **ESP-IDF 5.3.5** (плагин Espressif в VS Code) |
| Старый референс | **PlatformIO + Arduino** — `cam_web_test` (веб-камера, пины проверены) |

Команда создания текущего проекта:

```bash
idf.py create-project-from-example "espressif/esp-dl=3.2.3:human_face_recognition"
```

> Это **упрощённый пример** esp-dl (демо на вшитых JPEG). Полный live-пайплайн с камерой — в [esp-who/examples/human_face_recognition](https://github.com/espressif/esp-who/tree/master/examples/human_face_recognition). План — сначала убедиться, что пример собирается, затем перейти на live camera + SD.

---

## 3. Два проекта в workspace

| Проект | Путь | Роль |
|--------|------|------|
| **IDF (активный)** | `d:\users\esp_projects\human_face_recognition` | Face ML, esp-dl, ESP-IDF |
| **Arduino (референс)** | `d:\users\esp_projects\cam_web_test` | Веб `/jpg`, `/stream`, Wi‑Fi; эталон пинов |

Доп. план (ранний): `cam_web_test/docs/FACE_RECOGNITION_PLAN.md`

---

## 4. Пины платы (ESP32-S3-N16R8-CAM)

Источник истины: `cam_web_test/include/camera.h`, `cam_web_test/include/sd_card.h`.

**Важно:** пины камеры и SD **совпадают с BSP `esp32_s3_eye`** в зависимости `espressif/esp32_s3_eye_noglib` — отдельный кастомный BSP может не понадобиться.

### 4.1 Камера OV3660 (DVP + SCCB)

| Сигнал | GPIO |
|--------|------|
| XCLK | 15 |
| PCLK | 13 |
| VSYNC | 6 |
| HREF (HSYNC) | 7 |
| D0 (Y2) | 11 |
| D1 (Y3) | 9 |
| D2 (Y4) | 8 |
| D3 (Y5) | 10 |
| D4 (Y6) | 12 |
| D5 (Y7) | 18 |
| D6 (Y8) | 17 |
| D7 (Y9) | 16 |
| SCCB SDA (SIOD) | 4 |
| SCCB SCL (SIOC) | 5 |
| PWDN | -1 (NC) |
| RESET | -1 (NC) |

Рабочие настройки из Arduino-референса (`camera.cpp`):

- `xclk_freq_hz = 8000000`
- для ML позже: `PIXFORMAT_RGB565`, `FRAMESIZE_QVGA` (сейчас в Arduino — JPEG VGA для веба)

### 4.2 SD карта (SD_MMC, 1-bit)

| Сигнал | GPIO |
|--------|------|
| CLK | 39 |
| CMD | 38 |
| D0 | 40 |

Mount point в esp-dl BSP: **`/sdcard`** (`CONFIG_BSP_SD_MOUNT_POINT`).

### 4.3 C / esp_camera формат (для копипаста в IDF)

```c
// camera_config_t — пины
.pin_pwdn  = -1,
.pin_reset = -1,
.pin_xclk  = 15,
.pin_sccb_sda = 4,
.pin_sccb_scl = 5,
.pin_d7 = 16, .pin_d6 = 17, .pin_d5 = 18, .pin_d4 = 12,
.pin_d3 = 10, .pin_d2 = 8,  .pin_d1 = 9,  .pin_d0 = 11,
.pin_vsync = 6, .pin_href = 7, .pin_pclk = 13,

// SD_MMC 1-bit (через BSP или вручную)
clk=39, cmd=38, d0=40
```

---

## 5. Состояние проекта `human_face_recognition`

### 5.1 Что уже сделано пользователем

- Установлен **ESP-IDF 5.3.5** (VS Code extension).
- Создан проект из component example **esp-dl 3.2.3 `human_face_recognition`**.
- Target: **esp32s3** (`sdkconfig`: `CONFIG_IDF_TARGET="esp32s3"`).
- Зависимость BSP: `espressif/esp32_s3_eye_noglib` (для S3).
- Сборка в `build/` присутствует (проект хотя бы раз конфигурировался).

### 5.2 Что делает текущий `app_main.cpp` (критично!)

Файл: `main/app_main.cpp`

Сейчас это **не live-камера**, а **офлайн-демо**:

1. Монтирует БД (сейчас **`CONFIG_DB_FATFS_FLASH`** → flash partition `storage`).
2. Декодирует **вшитые JPEG** (`bill1`, `bill2`, `musk1`, `musk2` из `main/CMakeLists.txt`).
3. `HumanFaceDetect` → `HumanFaceRecognizer::enroll()` на 3 лицах.
4. `recognize()` на `musk2` → в monitor: `id: X, sim: 0.75...`
5. `clear_all_feats()` и выход.

**Ожидаемый первый успех:** прошивка → Serial → строка `id: ..., sim: ...` без падений.

### 5.3 Текущий sdkconfig (важные опции)

| Опция | Сейчас | Нужно для задумки |
|-------|--------|-------------------|
| `CONFIG_DB_FATFS_FLASH` | **y** | переключить на **`CONFIG_DB_FATFS_SDCARD`** |
| `CONFIG_DB_FATFS_SDCARD` | n | **y** (база `face.db` на SD 32GB) |
| `CONFIG_ESPTOOLPY_FLASHSIZE` | 8MB | проверить под N16 (**16MB** если не совпадает) |
| `CONFIG_SPIRAM_*` | OCT PSRAM enabled | оставить (нужно для камеры + ML) |
| BSP | `esp32_s3_eye_noglib` | оставить (пины совпадают) |

Menuconfig: **example: human_face_recognition → database file system → fatfs_sdcard**.

База лиц: `/sdcard/face.db` (2050 байт на одну запись: 2 байта id + 2048 feature).

---

## 6. План работ по фазам (для агента в следующем чате)

### Фаза A — «Демо заводится» (текущий пример)

**Цель:** `idf.py flash monitor` → `id: N, sim: X.XX`

- [ ] `idf.py set-target esp32s3`
- [ ] `idf.py build flash monitor`
- [ ] При ошибках flash size / PSRAM — поправить `sdkconfig` под N16R8
- [ ] Зафиксировать вывод в Serial

**Не делать пока:** камера, Wi‑Fi.

---

### Фаза B — База лиц на SD

**Цель:** `face.db` на SD переживает перепрошивку

- [ ] menuconfig: `DB_FATFS_SDCARD`
- [ ] `bsp_sdcard_mount()` — пины 39/38/40 (уже в BSP S3-EYE)
- [ ] Проверить монтирование: лог + файл на `/sdcard/`
- [ ] Повторить enroll/recognize demo, убедиться что `face.db` создаётся на SD
- [ ] Убрать или закомментировать `clear_all_feats()` в конце теста, если нужно сохранять базу

---

### Фаза C — Live camera (переход к ESP-WHO-подобному поведению)

**Цель:** кадр с OV3660 → detect → recognize в цикле

Текущий esp-dl example **без камеры**. Варианты (по возрастанию сложности):

1. **Рекомендуется:** взять код live pipeline из **esp-who** `human_face_recognition` и перенести в этот проект (или переключить проект на esp-who example + те же пины).
2. **Минимальный путь:** `bsp_camera` / `esp_camera` из `esp32_s3_eye_noglib` + подать `RGB888`/`RGB565` кадр в `HumanFaceDetect::run()`.

- [ ] Инициализация камеры (RGB, QVGA, PSRAM framebuffer)
- [ ] Цикл: grab frame → detect → recognize → log
- [ ] Enrollment по событию (кнопка / Serial / позже HTTP): `enroll(name)`

**Критерий:** в Serial при показе лица в камеру — `id` + `similarity`.

---

### Фаза D — Enrollment UX

**Цель:** регистрировать реальных людей без пересборки прошивки

- [ ] Serial-команды: `enroll`, `delete`, `list` (или кнопка BOOT, GPIO0 на S3-EYE BSP)
- [ ] 5–20 снимков/кадров на человека, разный угол
- [ ] Порог similarity вынести в `#define` / menuconfig
- [ ] Опционально: сохранять raw crops на SD `/sdcard/enroll/`

---

### Фаза E — Wi‑Fi + HTTP (из `cam_web_test`)

**Цель:** привычный интерфейс с телефона/ПК

Перенести идеи из `cam_web_test/src/main.cpp`:

| Endpoint | Назначение |
|----------|------------|
| `GET /status` | camera, sd, heap, psram, faces_count |
| `GET /jpg` | снимок JPEG (опционально, отдельный режим камеры) |
| `GET /recognize` | JSON `{ "id": 1, "name": "ivan", "sim": 0.91 }` |
| `POST /enroll?name=ivan` | регистрация лица |

- [ ] `esp_wifi` + `esp_http_server`
- [ ] FreeRTOS: задача ML отдельно от HTTP
- [ ] Wi‑Fi credentials в `sdkconfig.defaults` / Kconfig (**не коммитить пароли**)

---

### Фаза F — Home Assistant (позже)

- [ ] MQTT: `home/esp32cam/person` → payload имя
- [ ] Debounce 10–30 с на одного человека
- [ ] Автоматизация в HA

---

## 7. Ключевые файлы IDF-проекта

```
human_face_recognition/
├── main/
│   ├── app_main.cpp          ← сейчас offline JPEG demo
│   ├── CMakeLists.txt        ← embed bill1.jpg, musk2.jpg, ...
│   ├── Kconfig.projbuild       ← DB: flash / sdcard / spiffs
│   └── idf_component.yml       ← esp32_s3_eye_noglib, human_face_recognition
├── sdkconfig                 ← DB_FATFS_FLASH=y (сменить на SDCARD)
├── partitions.csv            ← storage 1MB для flash DB
├── README.md                 ← ссылка на esp-who full example
└── docs/NEXT_CHAT_HANDOFF.md ← этот файл
```

API (esp-dl):

- `HumanFaceDetect` — детекция
- `HumanFaceRecognizer` — enroll / recognize / clear
- БД: `face.db`, ~2050 байт / лицо

---

## 8. Команды для агента

```bash
cd d:\users\esp_projects\human_face_recognition

idf.py set-target esp32s3
idf.py menuconfig    # DB → fatfs_sdcard; flash 16MB если нужно
idf.py build
idf.py -p COM3 flash monitor
```

Порт **COM3** — как в `cam_web_test/platformio.ini` (уточнить у пользователя при прошивке).

---

## 9. Известные риски / подсказки

1. **Пример ≠ ESP-WHO live** — после Фазы A не ожидать камеру; нужна Фаза C.
2. **OV3660** — при init ошибках проверить sensor PID, XCLK 8 MHz; примеры часто под OV2640, но пины у вас совпали с S3-EYE.
3. **Flash 8 vs 16 MB** — в sdkconfig сейчас 8MB; плата N16 → возможно нужен `CONFIG_ESPTOOLPY_FLASHSIZE_16MB`.
4. **Arduino проект** — не удалять; использовать как эталон HTTP и проверки камеры JPEG.
5. **Пароли Wi‑Fi** в `cam_web_test` были в открытом виде — в IDF вынести в `sdkconfig.local` / gitignore.

---

## 10. Чеклист MVP

- [ ] Демо esp-dl: `id` + `sim` в monitor
- [ ] `face.db` на SD, survives reboot
- [ ] Live OV3660: detect + recognize в цикле
- [ ] Enroll 2+ реальных людей через Serial/HTTP
- [ ] HTTP `/status` + `/recognize`
- [ ] (Позже) MQTT → Home Assistant

---

## 11. Промпт для вставки в новый чат (короткий)

```
Проект: ESP32-S3-N16R8-CAM, OV3660, SD 32GB, ESP-IDF 5.3.5.
IDF-проект: d:\users\esp_projects\human_face_recognition
(из esp-dl 3.2.3 human_face_recognition, BSP esp32_s3_eye — пины совпадают с платой).
Референс Arduino: d:\users\esp_projects\cam_web_test

Цель: face detect + enroll + recognize → потом HTTP → Home Assistant.
Контекст и пины: human_face_recognition/docs/NEXT_CHAT_HANDOFF.md

Сейчас app_main — offline demo на вшитых JPEG, БД в flash.
Следующий шаг: [указать фазу A/B/C из handoff].
```

---

*Документ создан для передачи контекста между чатами. Обновляйте секцию 5 по мере прогресса.*
