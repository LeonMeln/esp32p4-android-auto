# VESC Cockpit Dashboard — Handoff to Claude Code

Промпт-документ для Claude Code. Содержит всё, что нужно для имплементации
дизайна на LVGL 8.3 + интеграции с SquareLine Studio (опционально).

---

## TL;DR — что нужно сделать

Реализовать UI-дашборд для электровелосипеда на VESC-контроллере.
Дисплей **800×480**, тёмная тема, акцент **кислотно-зелёный `#B6FF2E`**.
Источник дизайна — `dash-v1-cockpit.jsx` (JSX, рендерится в браузере 1:1).
Целевая платформа — **LVGL 8.3.x** (НЕ 9.x).

Готовый код: `cockpit_ui_init()` + `cockpit_ui_update(cockpit_data_t*)`,
вызывается из `main.c` после инициализации LVGL.

---

## Контекст проекта

- **Тип транспорта:** электровелосипед на VESC-контроллере
- **Дисплей:** 800×480, тач-скрин, ориентация landscape
- **MCU:** STM32 / ESP32-S3 (памяти достаточно — ~110KB под UI ок)
- **Версия LVGL:** 8.3.x (важно — есть отличия в API стилей от 9.x)
- **Цвет:** 16- или 32-бит, без разницы
- **Источник данных:** VESC по UART или CAN, протокол VESC packet (0x02/0x03 + payload)
- **Частота обновления UI:** ~4 Hz (каждые 250 мс) — от VESC получаем COMM_GET_VALUES

---

## Файлы-источники в этом репозитории

| Файл | Назначение |
|---|---|
| `dash-v1-cockpit.jsx` | **Главный референс** — JSX с точным layout, размерами, цветами |
| `dash-shared.jsx` | Helper'ы: `useLiveData`, `pad`, `pct`, `HBar`, `StatusBar` |
| `VESC Dashboard.html` | Открой в браузере чтобы увидеть как ДОЛЖНО выглядеть |
| `lvgl/SPEC.md` | Подробный спек с координатами и формулами |
| `lvgl/cockpit_ui.h` | Заготовка публичного API |

Открой эти файлы и прочитай прежде чем писать код.

---

## Архитектура решения

### Файлы которые нужно сгенерить

```
src/ui/
  cockpit_ui.h          — публичный API
  cockpit_ui.c          — создание виджетов + обновление значений
  cockpit_styles.c      — все lv_style_t
  cockpit_styles.h
  cockpit_fonts.h       — extern const lv_font_t * для всех шрифтов
  fonts/
    cockpit_font_speed_220.c   (Barlow Condensed Bold 220, только цифры 0-9)
    cockpit_font_big_64.c      (Barlow 64: 0-9, %, ., V, k, W, °)
    cockpit_font_med_32.c      (Barlow 32: 0-9, A-Z, ., /, °)
    cockpit_font_mono_11.c     (JetBrains Mono 11, ASCII 0x20-0x7F)
    cockpit_font_mono_9.c      (JetBrains Mono 9, ASCII 0x20-0x7F)
```

### Публичный API

```c
typedef enum {
    COCKPIT_MODE_ECO    = 1,
    COCKPIT_MODE_NORMAL = 2,
    COCKPIT_MODE_SPORT  = 3,
} cockpit_mode_t;

typedef struct {
    /* primary */
    uint16_t  speed_kmh;            /* 0..255 */
    uint16_t  speed_max_kmh;        /* масштаб полосы, обычно 60 */
    uint8_t   battery_percent;      /* 0..100 */
    float     voltage_v;
    uint16_t  power_w;              /* мощность ваттах */
    uint16_t  power_max_w;          /* для bar'a, обычно 4500 */
    float     motor_current_a;
    int16_t   motor_temp_c;
    int16_t   controller_temp_c;
    uint16_t  rpm;
    /* trip */
    float     trip_km;
    uint32_t  odometer_km;
    uint16_t  range_km;
    uint16_t  speed_avg_kmh;
    uint32_t  ride_time_s;
    /* status */
    cockpit_mode_t mode;
    bool      bluetooth;
    bool      gps;
} cockpit_data_t;

void cockpit_ui_init(void);
void cockpit_ui_update(const cockpit_data_t *d);
```

---

## Цветовая палитра (точно как в JSX `:root`)

```c
#define COCKPIT_BG_0       lv_color_hex(0x07090A)
#define COCKPIT_BG_1       lv_color_hex(0x0D1113)
#define COCKPIT_BG_2       lv_color_hex(0x161B1E)
#define COCKPIT_LINE       lv_color_hex(0x1F2629)
#define COCKPIT_LINE_2     lv_color_hex(0x2A3236)
#define COCKPIT_TEXT       lv_color_hex(0xE8EDEE)
#define COCKPIT_TEXT_DIM   lv_color_hex(0x8A9499)
#define COCKPIT_TEXT_FAINT lv_color_hex(0x4A5358)
#define COCKPIT_ACCENT     lv_color_hex(0xB6FF2E)
#define COCKPIT_WARN       lv_color_hex(0xFFB02E)
#define COCKPIT_DANGER     lv_color_hex(0xFF3B30)
```

**Логика цвета батареи:**
- `> 50%` → `ACCENT`
- `> 20%` → `WARN`
- иначе → `DANGER`

**Логика цвета температуры (motor/controller):**
- `> 80°C` → `DANGER`
- `> 60°C` → `WARN`
- иначе → `ACCENT`

---

## Layout (точные координаты, экран 800×480)

### 1. Status bar — y=0..32, x=0..800
- Высота 32, нижняя граница 1px `LINE`, фон `BG_0`
- Слева (gap 14px): `VESC` (accent, mono_11, bold) · `{mode_name}` (text_dim) · `{ride_time}` (text_dim)
- Справа (gap 14px): `BT` (accent если bluetooth) · `GPS` (если gps) · `{ctrl_temp}°C`

### 2. Battery panel — x=0..180, y=32..400
- Правая граница 1px `LINE`, padding: 24px top, 16px right, 16px bottom, 20px left
- "BATTERY" лейбл (mono_9, text_dim, letter_spacing wide)
- Большое число `{battery}` (Barlow 64, цвет по логике), знак `%` (mono_11/24px, dim)
- `{voltage:.1f} V` (mono_11, text_dim)
- **Сегментированный вертикальный бар:**
  - 14 ячеек, каждая 144×12, gap 3px между ними
  - Заполняется снизу вверх по проценту (`fill_count = round(battery/100 * 14)`)
  - Заполненные — цвет батареи, незаполненные — `BG_2`
- Внизу: `RANGE` (text_dim) ←→ `{range} KM` (text)

### 3. Speed panel — x=180..620, y=32..400
- "SPEED · KM/H" (mono_11/10px, letter_spacing 0.3em, text_dim)
- **Огромная цифра** `{speed:02d}` (Barlow Condensed Bold **220px**, color TEXT)
  - Размер реально 220px, lineHeight 0.85 (т.е. высота строки ~187px)
  - `pad(speed)` = всегда 2 цифры (32, 05, 60)
- **Полоса прогресса 320×6:**
  - Сегментированная на 12 частей (11 разделителей по 2px)
  - Заполнение `speed/speed_max` (по умолчанию `speed/60`)
  - Цвет — `ACCENT`
- Под полосой: `0` слева, `MAX 60` справа (mono_9, text_faint)

### 4. Power panel — x=620..800, y=32..400
- Левая граница 1px `LINE`, зеркало battery panel (align-right)
- "POWER" (mono_9)
- `{power_kw:.2f}` `kW` (Barlow 64, ACCENT)
- `{current:.1f} A · {rpm} RPM` (mono_11, text_dim)
- 14 сегментов вертикальный bar (всегда `ACCENT` на заполненных, `BG_2` на пустых)
- Заполнение `power_w / power_max_w`
- Внизу: `MAX` ←→ `{power_max_kw:.1f} KW`

### 5. Bottom strip — x=0..800, y=400..480
- Высота 80, верхняя граница 1px `LINE`, фон `BG_1`
- 5 колонок по 160px, между колонками 1px `LINE`
- В каждой по центру: лейбл (mono_9, text_dim, marginBottom 4) + значение (Barlow 32) + единица (mono_11/12px, text_dim, marginLeft 3)
- Содержимое колонок:
  1. `TRIP` / `{trip:.1f}` / `KM`
  2. `ODO` / `{odometer:.0f}` / `KM`
  3. `M·TEMP` / `{motor_temp}` / `°C`
  4. `C·TEMP` / `{controller_temp}` / `°C`
  5. `AVG` / `{speed_avg}` / `KM/H`

---

## Шрифты — конвертация

Скачай TTF:
- **Barlow Condensed Bold:** https://fonts.google.com/specimen/Barlow+Condensed
- **JetBrains Mono Regular:** https://fonts.google.com/specimen/JetBrains+Mono

Команды для `lv_font_conv` (npm: `npm i -g lv_font_conv`):

```bash
# Большая цифра скорости — только цифры, экономим флеш
lv_font_conv --font BarlowCondensed-Bold.ttf --size 220 \
  --bpp 4 --range 0x30-0x39 \
  --format lvgl -o cockpit_font_speed_220.c

# Цифры батареи и kW (Barlow 64)
# 0x25=%, 0x2E=., 0x30-39=0-9, 0x56=V, 0x57=W, 0x6B=k, 0xB0=°
lv_font_conv --font BarlowCondensed-Bold.ttf --size 64 \
  --bpp 4 --range 0x25,0x2E,0x30-0x39,0x56,0x57,0x6B,0xB0 \
  --format lvgl -o cockpit_font_big_64.c

# Нижние мини-метрики (Barlow 32)
# 0x2E=. 0x2F=/ 0x30-39=0-9 0x41-5A=A-Z 0xB0=°
lv_font_conv --font BarlowCondensed-Bold.ttf --size 32 \
  --bpp 4 --range 0x2E-0x39,0x41-0x5A,0xB0 \
  --format lvgl -o cockpit_font_med_32.c

# Лейблы (JetBrains Mono 11) — полный ASCII
lv_font_conv --font JetBrainsMono-Regular.ttf --size 11 \
  --bpp 2 --range 0x20-0x7F \
  --format lvgl -o cockpit_font_mono_11.c

# Совсем мелкие лейблы (JetBrains Mono 9)
lv_font_conv --font JetBrainsMono-Regular.ttf --size 9 \
  --bpp 2 --range 0x20-0x7F \
  --format lvgl -o cockpit_font_mono_9.c
```

В `cockpit_fonts.h`:

```c
LV_FONT_DECLARE(cockpit_font_speed_220);
LV_FONT_DECLARE(cockpit_font_big_64);
LV_FONT_DECLARE(cockpit_font_med_32);
LV_FONT_DECLARE(cockpit_font_mono_11);
LV_FONT_DECLARE(cockpit_font_mono_9);
```

---

## Сегментированные бары — реализация

В JSX это 14 div'ов в цикле. В LVGL 8 **не используй `lv_bar`** — она даёт сплошную полосу. Делай как массив `lv_obj_t *`:

```c
static lv_obj_t *batt_segs[14];
static lv_obj_t *power_segs[14];

/* в init() */
for (int i = 0; i < 14; i++) {
    batt_segs[i] = lv_obj_create(parent_battery_panel);
    lv_obj_set_size(batt_segs[i], 144, 12);
    /* y отсчитываем снизу: i=0 — самый нижний сегмент */
    lv_obj_set_pos(batt_segs[i], 0, bar_bottom_y - i * (12 + 3));
    lv_obj_set_style_radius(batt_segs[i], 1, 0);
    lv_obj_set_style_border_width(batt_segs[i], 0, 0);
    lv_obj_set_style_pad_all(batt_segs[i], 0, 0);
    lv_obj_clear_flag(batt_segs[i], LV_OBJ_FLAG_SCROLLABLE);
}

/* в update() */
uint8_t fill = (d->battery_percent * 14 + 50) / 100;  /* округление */
lv_color_t color = battery_color(d->battery_percent);
for (int i = 0; i < 14; i++) {
    lv_obj_set_style_bg_color(batt_segs[i],
        (i < fill) ? color : COCKPIT_BG_2, 0);
    lv_obj_set_style_bg_opa(batt_segs[i], LV_OPA_COVER, 0);
}
```

**Speed bar** (горизонтальная сегментированная полоса) — аналогично, 12 ячеек по `(320 - 11*2) / 12 ≈ 24.8px` шириной, gap 2px.

---

## Обновление значений в `cockpit_ui_update`

Используй `lv_label_set_text_fmt` для всех чисел:

```c
void cockpit_ui_update(const cockpit_data_t *d)
{
    /* Speed */
    lv_label_set_text_fmt(label_speed, "%02u", d->speed_kmh);

    /* Battery */
    lv_label_set_text_fmt(label_battery, "%u", d->battery_percent);
    lv_label_set_text_fmt(label_voltage, "%.1f V", d->voltage_v);
    lv_label_set_text_fmt(label_range, "%u KM", d->range_km);
    lv_color_t bcol = battery_color(d->battery_percent);
    lv_obj_set_style_text_color(label_battery, bcol, 0);
    /* ...сегменты bat_segs см. выше... */

    /* Power */
    lv_label_set_text_fmt(label_power, "%.2f", d->power_w / 1000.0f);
    lv_label_set_text_fmt(label_pwr_sub, "%.1f A · %u RPM",
                          d->motor_current_a, d->rpm);

    /* Bottom strip */
    lv_label_set_text_fmt(label_trip, "%.1f",  d->trip_km);
    lv_label_set_text_fmt(label_odo,  "%lu",   (unsigned long)d->odometer_km);
    lv_label_set_text_fmt(label_mtmp, "%d",    d->motor_temp_c);
    lv_label_set_text_fmt(label_ctmp, "%d",    d->controller_temp_c);
    lv_label_set_text_fmt(label_avg,  "%u",    d->speed_avg_kmh);

    /* Status bar */
    uint32_t s = d->ride_time_s;
    lv_label_set_text_fmt(label_time, "%02lu:%02lu:%02lu",
        (unsigned long)(s/3600), (unsigned long)((s/60)%60),
        (unsigned long)(s%60));

    static const char *MODE_NAMES[] = { "", "ECO", "NORMAL", "SPORT" };
    lv_label_set_text(label_mode, MODE_NAMES[d->mode]);

    lv_obj_set_style_text_opa(label_bt, d->bluetooth ? LV_OPA_COVER : LV_OPA_30, 0);
    lv_obj_set_style_text_opa(label_gps, d->gps ? LV_OPA_COVER : LV_OPA_30, 0);
}
```

Цвета-helper'ы:

```c
static lv_color_t battery_color(uint8_t pct) {
    if (pct > 50) return COCKPIT_ACCENT;
    if (pct > 20) return COCKPIT_WARN;
    return COCKPIT_DANGER;
}
static lv_color_t temp_color(int16_t t) {
    if (t > 80) return COCKPIT_DANGER;
    if (t > 60) return COCKPIT_WARN;
    return COCKPIT_ACCENT;
}
```

---

## Чего НЕ делать

- ❌ Не использовать `text-shadow` (в JSX он есть, но в LVGL не нужен)
- ❌ Не использовать `lv_arc` (Cockpit без круговых элементов)
- ❌ Не делать flex-layout для сегментов — координаты руками
- ❌ Не делать анимации в первой версии
- ❌ Не подключать `LV_FONT_MONTSERRAT_*` — выключи в `lv_conf.h` если включены, экономь флеш
- ❌ Не использовать `lv_bar` для сегментированных полос — только массив объектов

---

## Опциональные улучшения (после базовой версии)

1. **Плавная анимация цифры скорости** — `lv_anim_t` с user_data + callback на `lv_label_set_text_fmt`
2. **Pulsing accent при `battery < 20%`** — `lv_anim_t` на `bg_opa` сегментов
3. **Затухание текста при потере связи с VESC**
4. **Свайпы** — `lv_indev_get_gesture_dir()` для перехода на BT/Mode экраны

---

## Тестовые данные для первого запуска

```c
cockpit_data_t test = {
    .speed_kmh = 32,        .speed_max_kmh = 60,
    .battery_percent = 68,  .voltage_v = 54.2f,
    .power_w = 998,         .power_max_w = 4500,
    .motor_current_a = 18.4f,
    .motor_temp_c = 62,     .controller_temp_c = 48,
    .rpm = 2840,
    .trip_km = 14.2f,       .odometer_km = 4128,
    .range_km = 38,         .speed_avg_kmh = 24,
    .ride_time_s = 2538,    .mode = COCKPIT_MODE_SPORT,
    .bluetooth = true,      .gps = true,
};
cockpit_ui_init();
cockpit_ui_update(&test);
```

Должно отрендериться **точно как `dash-v1-cockpit.jsx` в браузере**:
крупная цифра `32` по центру, батарея `68%` слева зелёная (т.к. >50),
мощность `1.00 kW` справа, нижний ряд из 5 метрик.

---

## Альтернатива: SquareLine Studio

Если решишь собирать в SquareLine, а не писать руками:

1. **Create → Empty Project**, разрешение `800×480`, LVGL `8.3.x`, color depth `16-bit`
2. **Assets → Colors** — добавь все 11 цветов из палитры выше (имена: `bg_0`, `accent` и т.д.)
3. **Assets → Fonts** — Add new font, выбери TTF, размеры **32 / 64 / 220** (для 220 в Range укажи `0-9` иначе будет 200KB+)
4. Подложи скриншот Cockpit'а как Image-фон с `opacity 30%` чтобы расставлять виджеты pixel-perfect
5. Собери: Status bar (Panel 800×32) → Battery panel (Panel 180×368) → Speed panel (440×368) → Power panel (180×368) → Bottom strip (800×80)
6. Внутри каждой панели — Label'ы по координатам из секции Layout
7. **14 сегментов батареи**: один Panel 144×12, потом Ctrl+D 13 раз, расставить по Y c шагом 15
8. **Export → Export UI Files** → получаешь `ui.c`/`ui.h`/`ui_helpers.c`
9. Поверх экспорта пишешь свой `cockpit_ui_update()` который дёргает SquareLine'овские объекты (`ui_LabelSpeed`, `ui_LabelBattery` и т.д.) через `lv_label_set_text_fmt`

Минус: SquareLine не умеет привязку к данным — апдейт всё равно своими руками.
Плюс: статика собирается в 5 раз быстрее, чем `lv_obj_set_pos` руками.

---

## Что прислать обратно

После имплементации:
1. Покажи как выглядит на железе (фото дисплея)
2. Если будут расхождения с JSX — отметь в каком виджете
3. Если переходишь на LVGL 9.x — скажи, я адаптирую API стилей в спеке

Удачи! 🤘
