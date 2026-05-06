# VESC Cockpit Dashboard — LVGL Implementation

Полный пакет для имплементации Cockpit-дизайна на LVGL 8.x (тестировалось на 8.3) для дисплея 800×480.

## Файлы

| Файл | Назначение |
|------|-----------|
| `cockpit_ui.h` | Публичный API — функции инициализации и обновления данных |
| `cockpit_ui.c` | Создание всех виджетов, стилей, layout |
| `cockpit_styles.c` | Все `lv_style_t` (цвета, шрифты, рамки) |
| `SPEC.md` | Точные пиксели/цвета/размеры для референса |
| `FONTS.md` | Команды для конвертации шрифтов |

## Использование в твоём проекте

```c
#include "cockpit_ui.h"

void app_main(void) {
    lv_init();
    // ... инициализация дисплея 800x480 ...

    cockpit_ui_init();          // создаёт экран и все виджеты

    // в основном цикле, каждые ~250ms когда приходят данные с VESC по CAN/UART:
    cockpit_data_t data = {
        .speed_kmh        = 32,
        .battery_percent  = 68,
        .voltage_v        = 54.2f,
        .power_w          = 998,
        .motor_current_a  = 18.4f,
        .motor_temp_c     = 62,
        .controller_temp_c = 48,
        .rpm              = 2840,
        .trip_km          = 14.2f,
        .odometer_km      = 4128,
        .range_km         = 38,
        .speed_avg_kmh    = 24,
        .ride_time_s      = 2538,    // 00:42:18
        .mode             = COCKPIT_MODE_SPORT,
        .bluetooth        = true,
        .gps              = true,
    };
    cockpit_ui_update(&data);
}
```

## Зависимости

- LVGL 8.3.x (если нужно для 9.x — скажи, перепишу)
- Поддержка кастомных шрифтов (`LV_FONT_MONTSERRAT_*` отключи, нам не нужны)
- Цветовая глубина 16 или 32 бит

## Память (приблизительно)

| Что | Размер |
|---|---|
| Шрифт Barlow_Condensed_Bold_220 (только цифры) | ~80 KB |
| Шрифт Barlow_Condensed_Bold_64 (цифры + %, V, kW) | ~12 KB |
| Шрифт Barlow_Condensed_Bold_32 | ~7 KB |
| Шрифт JetBrainsMono_Regular_11 (ASCII) | ~6 KB |
| Шрифт JetBrainsMono_Regular_9 | ~5 KB |
| Сами виджеты в RAM | ~8 KB |
| **Итого флеш под UI** | **~110 KB** |
| **Итого RAM под виджеты** | **~8 KB** |

Вписывается в любой STM32H7/F7/ESP32-S3.
