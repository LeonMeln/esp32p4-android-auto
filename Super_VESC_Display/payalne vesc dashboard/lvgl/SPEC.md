# Spec: VESC Cockpit Dashboard → LVGL 8.3 implementation

## Source of truth
- `dash-v1-cockpit.jsx` — JSX-исходник, описывающий весь layout
- `dash-shared.jsx` — общие helper'ы (`useLiveData`, `pad`, `pct`, `HBar`, `StatusBar`)

Все размеры, цвета и шрифты в JSX заданы в пикселях/hex и переносятся в C 1:1.

## Target

- **LVGL 8.3.x** (НЕ 9.x — у пользователя 8.x в проекте)
- Дисплей **800×480**, 16- или 32-битный цвет
- Тёмная тема, без анимаций кроме плавного апдейта значений

## Файлы которые нужно сгенерить

| Файл | Что внутри |
|---|---|
| `cockpit_ui.h` | Публичный API: `cockpit_ui_init()`, `cockpit_ui_update(const cockpit_data_t*)`, struct `cockpit_data_t` |
| `cockpit_ui.c` | Создание всех виджетов в `cockpit_ui_init()`, апдейт значений в `cockpit_ui_update()` |
| `cockpit_styles.c` | Объявления `lv_style_t` для текстов, рамок, фонов |
| `cockpit_fonts.h` | `extern const lv_font_t` для всех шрифтов |
| `fonts/` | Сгенерированные `.c` шрифты (см. ниже) |

## Цветовая палитра (точно как в `:root` у JSX)

```c
#define COCKPIT_BG_0       lv_color_hex(0x07090A)  /* deepest black */
#define COCKPIT_BG_1       lv_color_hex(0x0D1113)  /* card */
#define COCKPIT_BG_2       lv_color_hex(0x161B1E)  /* chip / unfilled bar */
#define COCKPIT_LINE       lv_color_hex(0x1F2629)  /* divider */
#define COCKPIT_LINE_2     lv_color_hex(0x2A3236)  /* divider stronger */
#define COCKPIT_TEXT       lv_color_hex(0xE8EDEE)
#define COCKPIT_TEXT_DIM   lv_color_hex(0x8A9499)
#define COCKPIT_TEXT_FAINT lv_color_hex(0x4A5358)
#define COCKPIT_ACCENT     lv_color_hex(0xB6FF2E)  /* VESC acid green */
#define COCKPIT_WARN       lv_color_hex(0xFFB02E)
#define COCKPIT_DANGER     lv_color_hex(0xFF3B30)
```

Логика цвета батареи: `>50% → ACCENT`, `>20% → WARN`, иначе `DANGER`.

## Шрифты (Barlow Condensed Bold + JetBrains Mono Regular)

Сгенерировать через `lv_font_conv`:

```bash
# Большая цифра скорости (220px) — только цифры, экономим флеш
lv_font_conv --font Barlow_Condensed_Bold.ttf --size 220 \
  --bpp 4 --range 0x30-0x39 \
  --format lvgl -o cockpit_font_speed_220.c

# Цифры батареи и kW (64px) — цифры, точка, %, k, W, V, °
lv_font_conv --font Barlow_Condensed_Bold.ttf --size 64 \
  --bpp 4 --range 0x25-0x25,0x2E,0x30-0x39,0x56,0x57,0x6B,0xB0 \
  --format lvgl -o cockpit_font_big_64.c

# Нижние мини-метрики (32px) — цифры, единицы
lv_font_conv --font Barlow_Condensed_Bold.ttf --size 32 \
  --bpp 4 --range 0x2E,0x30-0x39,0x41-0x5A,0xB0,0x2F \
  --format lvgl -o cockpit_font_med_32.c

# Лейблы (11px ASCII)
lv_font_conv --font JetBrainsMono_Regular.ttf --size 11 \
  --bpp 2 --range 0x20-0x7F \
  --format lvgl -o cockpit_font_mono_11.c

# Совсем мелкие лейблы (9px)
lv_font_conv --font JetBrainsMono_Regular.ttf --size 9 \
  --bpp 2 --range 0x20-0x7F \
  --format lvgl -o cockpit_font_mono_9.c
```

Имена в коде:
- `cockpit_font_speed_220` (Barlow 220, цифры)
- `cockpit_font_big_64` (Barlow 64, для % и kW)
- `cockpit_font_med_32` (Barlow 32, мини-метрики)
- `cockpit_font_mono_11` (JetBrains 11)
- `cockpit_font_mono_9` (JetBrains 9)

## Layout (точные координаты)

Экран 800×480, абсолютное позиционирование.

### Status bar (top, 0..32 y)
- Высота 32, нижняя граница 1px `LINE`
- Слева (x=16): "VESC" (accent, mono_11) · "{mode}" (text_dim, mono_11) · "{ride_time}" (mono_11)
- Справа (x=784): "BT" (accent если bluetooth=true) · "GPS" · "{ctrl_temp}°C"

### Battery panel (left, x=0..180, y=32..400)
- Правая граница 1px `LINE`
- Padding: 24px top, 16px right, 16px bottom, 20px left
- "BATTERY" лейбл (mono_9, text_dim, letter_spacing)
- Большое число `{battery}` (Barlow 64, цвет по логике), рядом "%" (mono_11, dim)
- "{voltage} V" (mono_11, dim)
- **Сегментированный вертикальный bar**: 14 ячеек, каждая 12px высотой, gap 3px, заполнение снизу вверх по проценту батареи
- Внизу: "RANGE" + "{range} KM" между left/right

### Speed panel (center, x=180..620, y=32..400)
- "SPEED · KM/H" лейбл (mono_9 при 10px, letter_spacing 0.3em)
- Огромная цифра `{speed:02d}` (Barlow 220, color TEXT, **lineHeight 0.85** — важно!)
- Полоса прогресса 320×6, сегментированная на 12 частей
- Под ней: "0" слева, "MAX 60" справа (mono_9, faint)

### Power panel (right, x=620..800, y=32..400)
- Левая граница 1px `LINE`
- Зеркально battery panel, но align-right
- "POWER" → `{power_kw:.2f}` "kW" (Barlow 64, ACCENT) → "{current} A · {rpm} RPM"
- 14 сегментов вертикальный bar (как у батареи, но всегда ACCENT)
- "MAX" + "4.5 KW"

### Bottom strip (y=400..480, x=0..800)
- Высота 80, верхняя граница 1px `LINE`, фон `BG_1`
- 5 колонок равной ширины (160px каждая), между ними 1px `LINE`
- В каждой: лейбл (mono_9, text_dim) + значение (Barlow 32) + единица (mono_11, dim)
- Колонки: TRIP / ODO / M·TEMP / C·TEMP / AVG

## Структура `cockpit_data_t`

```c
typedef struct {
    uint16_t  speed_kmh;
    uint16_t  speed_max_kmh;       /* 60 для масштаба полосы */
    uint8_t   battery_percent;     /* 0..100 */
    float     voltage_v;
    uint16_t  power_w;
    uint16_t  power_max_w;         /* 4500 */
    float     motor_current_a;
    int16_t   motor_temp_c;
    int16_t   controller_temp_c;
    uint16_t  rpm;
    float     trip_km;
    uint32_t  odometer_km;
    uint16_t  range_km;
    uint16_t  speed_avg_kmh;
    uint32_t  ride_time_s;
    uint8_t   mode;                /* 1=ECO 2=NORMAL 3=SPORT */
    bool      bluetooth;
    bool      gps;
} cockpit_data_t;
```

## Сегментированные bar'ы — реализация

В JSX это 14 div'ов в цикле. В LVGL 8 — **не используй `lv_bar`**, она даёт сплошную полосу. Делай так:

```c
/* массив 14 указателей на lv_obj_t — каждый сегмент */
static lv_obj_t *batt_segs[14];

/* в init() — создать в цикле */
for (int i = 0; i < 14; i++) {
    batt_segs[i] = lv_obj_create(parent);
    lv_obj_set_size(batt_segs[i], full_width, 12);
    lv_obj_set_pos(batt_segs[i], x, y_bottom - i * 15);
    lv_obj_set_style_radius(batt_segs[i], 1, 0);
    lv_obj_set_style_border_width(batt_segs[i], 0, 0);
}

/* в update() — перекрасить */
for (int i = 0; i < 14; i++) {
    bool filled = (13 - i) / 14.0f < d->battery_percent / 100.0f;
    lv_obj_set_style_bg_color(batt_segs[i],
        filled ? batt_color : COCKPIT_BG_2, 0);
}
```

## Чего НЕ делать
- ❌ Не использовать `text-shadow` (в JSX он есть, но в LVGL не нужен)
- ❌ Не использовать `lv_arc` (Cockpit без круговых элементов)
- ❌ Не использовать flex для сегментов — координаты руками
- ❌ Не делать анимации перехода значений в первой версии (добавим потом если надо)
- ❌ Не подключать `LV_FONT_MONTSERRAT_*` — они нам не нужны, только наши 5 шрифтов

## Опционально (можешь предложить как enhancement)
- Плавная анимация цифры скорости через `lv_anim_t` с callback на `lv_label_set_text_fmt`
- Pulsing accent при `battery < 20%`
- Затухание текста при `bluetooth=false`

## Тестовые данные для первого запуска

```c
cockpit_data_t test = {
    .speed_kmh = 32, .speed_max_kmh = 60,
    .battery_percent = 68, .voltage_v = 54.2f,
    .power_w = 998, .power_max_w = 4500,
    .motor_current_a = 18.4f,
    .motor_temp_c = 62, .controller_temp_c = 48,
    .rpm = 2840,
    .trip_km = 14.2f, .odometer_km = 4128, .range_km = 38,
    .speed_avg_kmh = 24, .ride_time_s = 2538,
    .mode = 3, .bluetooth = true, .gps = true,
};
cockpit_ui_init();
cockpit_ui_update(&test);
```
