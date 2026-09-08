#include <rtthread.h>
#include "lvgl.h"
#include "exam_shared.h"

static lv_obj_t *s_temp_value;
static lv_obj_t *s_humi_value;
static lv_obj_t *s_state_label;
static lv_obj_t *s_comfort_label;
static lv_obj_t *s_seq_label;
static lv_obj_t *s_touch_label;
static lv_obj_t *s_temp_card;
static lv_obj_t *s_humi_card;
static lv_obj_t *s_state_bar;

static lv_style_t s_style_bg;
static lv_style_t s_style_card;
static lv_style_t s_style_temp_card;
static lv_style_t s_style_humi_card;
static lv_style_t s_style_title;
static lv_style_t s_style_big_value;
static lv_style_t s_style_mid_value;
static lv_style_t s_style_caption;
static lv_style_t s_style_state;

static int ui_abs_int(int value)
{
    return value < 0 ? -value : value;
}

static void ui_set_label_x10(lv_obj_t *label, float value, const char *unit)
{
    char buf[32];
    int scaled;
    int whole;
    int frac;

    /* 当前工具链的 LVGL printf 未开启浮点格式化，因此先转成十分位整数再显示。 */
    if (value >= 0.0f)
    {
        scaled = (int)(value * 10.0f + 0.5f);
    }
    else
    {
        scaled = (int)(value * 10.0f - 0.5f);
    }

    whole = scaled / 10;
    frac = ui_abs_int(scaled % 10);

    if (scaled < 0 && whole == 0)
    {
        lv_snprintf(buf, sizeof(buf), "-%d.%d %s", ui_abs_int(whole), frac, unit);
    }
    else
    {
        lv_snprintf(buf, sizeof(buf), "%d.%d %s", whole, frac, unit);
    }

    lv_label_set_text(label, buf);
}

static const char *ui_comfort_text(float temp, float humi)
{
    switch (thermo_get_comfort_level(temp, humi))
    {
    case THERMO_COMFORT_ALARM:
        return "ALARM  High temp/humi";
    case THERMO_COMFORT_WARM:
        return "WARM   Ventilate";
    case THERMO_COMFORT_DRY_COOL:
        return "DRY/COOL  Check room";
    case THERMO_COMFORT_GOOD:
    default:
        return "COMFORT  Good";
    }
}

static void ui_set_state_color(rt_uint8_t sensor_ok, rt_uint8_t paused, rt_uint8_t alarm)
{
    lv_color_t color;

    if (!sensor_ok)
    {
        color = lv_color_hex(0xE74C3C);
    }
    else if (alarm)
    {
        color = lv_color_hex(0xF39C12);
    }
    else if (paused)
    {
        color = lv_color_hex(0x6C7A89);
    }
    else
    {
        color = lv_color_hex(0x2ECC71);
    }

    lv_obj_set_style_bg_color(s_state_bar, color, 0);
}

static void ui_timer_cb(lv_timer_t *timer)
{
    const volatile thermometer_shared_t *shared = &g_thermometer_shared;
    char buf[32];

    (void)timer;

    /* 这个工程由 M55 本核通过 I2C 直接读取 AHT20，不再等待跨核传感器数据。 */
    ui_set_label_x10(s_temp_value, shared->temperature, "C");
    ui_set_label_x10(s_humi_value, shared->humidity, "%");
    lv_label_set_text(s_comfort_label, ui_comfort_text(shared->temperature, shared->humidity));

    if (!shared->sensor_ok)
    {
        lv_label_set_text(s_state_label, "AHT20 OFFLINE / RETRY");
    }
    else if (shared->paused)
    {
        lv_label_set_text(s_state_label, "PAUSED BY KEY/TOUCH");
    }
    else if (shared->alarm)
    {
        lv_label_set_text(s_state_label, "ALARM: LIMIT EXCEEDED");
    }
    else
    {
        lv_label_set_text(s_state_label, "AHT20 ONLINE - I2C LOCAL");
    }

    lv_snprintf(buf, sizeof(buf), "sample: %u", shared->sequence);
    lv_label_set_text(s_seq_label, buf);
    lv_label_set_text(s_touch_label, shared->paused ? "Resume" : "Pause");
    ui_set_state_color(shared->sensor_ok, shared->paused, shared->alarm);
}

static void ui_touch_pause_cb(lv_event_t *e)
{
    (void)e;

    /* 触摸屏按钮和实体 KEY 共用同一个暂停标志，维护暂停逻辑时可从这里查看入口。 */
    g_thermometer_shared.paused = !g_thermometer_shared.paused;
}

static void ui_styles_init(void)
{
    lv_style_init(&s_style_bg);
    lv_style_set_bg_color(&s_style_bg, lv_color_hex(0xEAF6F4));
    lv_style_set_bg_grad_color(&s_style_bg, lv_color_hex(0xD7E9FF));
    lv_style_set_bg_grad_dir(&s_style_bg, LV_GRAD_DIR_VER);

    lv_style_init(&s_style_card);
    lv_style_set_radius(&s_style_card, 18);
    lv_style_set_bg_color(&s_style_card, lv_color_white());
    lv_style_set_border_width(&s_style_card, 0);
    lv_style_set_shadow_width(&s_style_card, 10);
    lv_style_set_shadow_color(&s_style_card, lv_color_hex(0x9FB5C8));
    lv_style_set_shadow_opa(&s_style_card, LV_OPA_30);
    lv_style_set_pad_all(&s_style_card, 14);

    lv_style_init(&s_style_temp_card);
    lv_style_set_bg_color(&s_style_temp_card, lv_color_hex(0xFFF6F1));

    lv_style_init(&s_style_humi_card);
    lv_style_set_bg_color(&s_style_humi_card, lv_color_hex(0xF0FAFF));

    lv_style_init(&s_style_title);
    lv_style_set_text_color(&s_style_title, lv_color_hex(0x1F3A4D));
    lv_style_set_text_font(&s_style_title, &lv_font_montserrat_24);

    lv_style_init(&s_style_big_value);
    lv_style_set_text_color(&s_style_big_value, lv_color_hex(0xFF6B6B));
    lv_style_set_text_font(&s_style_big_value, &lv_font_montserrat_24);

    lv_style_init(&s_style_mid_value);
    lv_style_set_text_color(&s_style_mid_value, lv_color_hex(0x2980B9));
    lv_style_set_text_font(&s_style_mid_value, &lv_font_montserrat_24);

    lv_style_init(&s_style_caption);
    lv_style_set_text_color(&s_style_caption, lv_color_hex(0x52616B));
    lv_style_set_text_font(&s_style_caption, &lv_font_montserrat_16);

    lv_style_init(&s_style_state);
    lv_style_set_radius(&s_style_state, 12);
    lv_style_set_bg_color(&s_style_state, lv_color_hex(0x2ECC71));
    lv_style_set_bg_opa(&s_style_state, LV_OPA_COVER);
    lv_style_set_pad_left(&s_style_state, 14);
    lv_style_set_pad_right(&s_style_state, 14);
    lv_style_set_pad_top(&s_style_state, 8);
    lv_style_set_pad_bottom(&s_style_state, 8);
}

static lv_obj_t *ui_create_card(lv_obj_t *parent, lv_coord_t w, lv_coord_t h, lv_style_t *tone_style)
{
    lv_obj_t *card = lv_obj_create(parent);

    lv_obj_set_size(card, w, h);
    lv_obj_add_style(card, &s_style_card, 0);
    lv_obj_add_style(card, tone_style, 0);
    lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);

    return card;
}

void ui_thermometer_init(void)
{
    lv_obj_t *scr = lv_scr_act();
    lv_obj_t *title;
    lv_obj_t *caption;
    lv_obj_t *touch_btn;

    ui_styles_init();
    lv_obj_add_style(scr, &s_style_bg, 0);

    title = lv_label_create(scr);
    lv_label_set_text(title, "Environment Monitor");
    lv_obj_add_style(title, &s_style_title, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 16);

    s_temp_card = ui_create_card(scr, 230, 185, &s_style_temp_card);
    lv_obj_align(s_temp_card, LV_ALIGN_TOP_LEFT, 22, 68);

    caption = lv_label_create(s_temp_card);
    lv_label_set_text(caption, "Temperature");
    lv_obj_add_style(caption, &s_style_caption, 0);
    lv_obj_align(caption, LV_ALIGN_TOP_LEFT, 0, 0);

    s_temp_value = lv_label_create(s_temp_card);
    lv_label_set_text(s_temp_value, "--.- C");
    lv_obj_add_style(s_temp_value, &s_style_big_value, 0);
    lv_obj_align(s_temp_value, LV_ALIGN_CENTER, 0, 18);

    s_humi_card = ui_create_card(scr, 210, 185, &s_style_humi_card);
    lv_obj_align(s_humi_card, LV_ALIGN_TOP_RIGHT, -22, 68);

    caption = lv_label_create(s_humi_card);
    lv_label_set_text(caption, "Humidity");
    lv_obj_add_style(caption, &s_style_caption, 0);
    lv_obj_align(caption, LV_ALIGN_TOP_LEFT, 0, 0);

    s_humi_value = lv_label_create(s_humi_card);
    lv_label_set_text(s_humi_value, "--.- %");
    lv_obj_add_style(s_humi_value, &s_style_mid_value, 0);
    lv_obj_align(s_humi_value, LV_ALIGN_CENTER, 0, 18);

    s_state_bar = lv_obj_create(scr);
    lv_obj_set_size(s_state_bar, 420, 68);
    lv_obj_add_style(s_state_bar, &s_style_state, 0);
    lv_obj_align(s_state_bar, LV_ALIGN_BOTTOM_LEFT, 22, -22);
    lv_obj_clear_flag(s_state_bar, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);

    s_state_label = lv_label_create(s_state_bar);
    lv_label_set_text(s_state_label, "AHT20 STARTING - I2C LOCAL");
    lv_obj_set_style_text_color(s_state_label, lv_color_white(), 0);
    lv_obj_align(s_state_label, LV_ALIGN_TOP_LEFT, 0, 0);

    s_comfort_label = lv_label_create(s_state_bar);
    lv_label_set_text(s_comfort_label, "COMFORT  Waiting data");
    lv_obj_set_style_text_color(s_comfort_label, lv_color_white(), 0);
    lv_obj_align(s_comfort_label, LV_ALIGN_BOTTOM_LEFT, 0, 0);

    s_seq_label = lv_label_create(scr);
    lv_label_set_text(s_seq_label, "sample: 0");
    lv_obj_add_style(s_seq_label, &s_style_caption, 0);
    lv_obj_align(s_seq_label, LV_ALIGN_TOP_LEFT, 30, 268);

    touch_btn = lv_button_create(scr);
    lv_obj_set_size(touch_btn, 118, 46);
    lv_obj_align(touch_btn, LV_ALIGN_BOTTOM_RIGHT, -22, -32);
    lv_obj_add_event_cb(touch_btn, ui_touch_pause_cb, LV_EVENT_CLICKED, RT_NULL);

    s_touch_label = lv_label_create(touch_btn);
    lv_label_set_text(s_touch_label, "Pause");
    lv_obj_center(s_touch_label);

    lv_timer_create(ui_timer_cb, 500, RT_NULL);
}
