#include <rtthread.h>
#include "lvgl.h"
#include "exam_shared.h"

static lv_obj_t *s_title_label;
static lv_obj_t *s_switch_label;
static lv_obj_t *s_thermo_panel;
static lv_obj_t *s_fall_panel;

static lv_obj_t *s_temp_value;
static lv_obj_t *s_humi_value;
static lv_obj_t *s_thermo_state_label;
static lv_obj_t *s_comfort_label;
static lv_obj_t *s_thermo_seq_label;
static lv_obj_t *s_thermo_state_bar;

static lv_obj_t *s_acc_value;
static lv_obj_t *s_gyro_value;
static lv_obj_t *s_fall_state_label;
static lv_obj_t *s_fall_detail_label;
static lv_obj_t *s_fall_seq_label;
static lv_obj_t *s_fall_state_bar;

/*
 * 调试覆盖层：摔倒检测要靠猛烈甩动开发板来验证，串口根本看不了，
 * 所以把状态机的判据原样搬到屏幕上，并做峰值保持（甩完再看也来得及）。
 */
static rt_bool_t s_dbg_enabled = RT_FALSE;
static lv_obj_t *s_dbg_btn;
static lv_obj_t *s_dbg_btn_label;
static lv_obj_t *s_debug_panel;
static lv_obj_t *s_dbg_acc_label;
static lv_obj_t *s_dbg_gyro_label;
static lv_obj_t *s_dbg_thr_label;
static lv_obj_t *s_dbg_cnt_label;
static lv_obj_t *s_dbg_flag_label;
static lv_obj_t *s_dbg_state_label;

/* 提前声明：ui_timer_cb 定义在 ui_dbg_update 之前，避免被隐式声明成非 static */
static void ui_dbg_update(void);

static lv_style_t s_style_bg;
static lv_style_t s_style_card;
static lv_style_t s_style_temp_card;
static lv_style_t s_style_humi_card;
static lv_style_t s_style_acc_card;
static lv_style_t s_style_gyro_card;
static lv_style_t s_style_title;
static lv_style_t s_style_temp_value;
static lv_style_t s_style_humi_value;
static lv_style_t s_style_acc_value;
static lv_style_t s_style_gyro_value;
static lv_style_t s_style_caption;
static lv_style_t s_style_orange_caption;
static lv_style_t s_style_blue_caption;
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

static void ui_format_x100(char *buf, rt_size_t size, const char *name, float value, const char *unit)
{
    int scaled;
    int whole;
    int frac;

    if (value >= 0.0f)
    {
        scaled = (int)(value * 100.0f + 0.5f);
    }
    else
    {
        scaled = (int)(value * 100.0f - 0.5f);
    }

    whole = scaled / 100;
    frac = ui_abs_int(scaled % 100);

    if (scaled < 0 && whole == 0)
    {
        lv_snprintf(buf, size, "%s -0.%02d %s", name, frac, unit);
    }
    else
    {
        lv_snprintf(buf, size, "%s %d.%02d %s", name, whole, frac, unit);
    }
}

static void ui_set_axis_text(lv_obj_t *label, float x, float y, float z, const char *unit, rt_bool_t gyro_mdps)
{
    char x_buf[28];
    char y_buf[28];
    char z_buf[28];
    char buf[96];
    float scale = gyro_mdps ? 0.001f : 1.0f;

    ui_format_x100(x_buf, sizeof(x_buf), "X", x * scale, unit);
    ui_format_x100(y_buf, sizeof(y_buf), "Y", y * scale, unit);
    ui_format_x100(z_buf, sizeof(z_buf), "Z", z * scale, unit);
    lv_snprintf(buf, sizeof(buf), "%s\n%s\n%s", x_buf, y_buf, z_buf);
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

static const char *ui_fall_state_text(rt_uint8_t state)
{
    switch (state)
    {
    case FALLCHECK_STATE_FALL:
        return "FALL";
    case FALLCHECK_STATE_NORMAL:
        return "NORMAL";
    case FALLCHECK_STATE_IDLE:
    default:
        return "IDLE";
    }
}

static void ui_set_thermo_state_color(rt_uint8_t sensor_ok, rt_uint8_t paused, rt_uint8_t alarm)
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

    lv_obj_set_style_bg_color(s_thermo_state_bar, color, 0);
}

static void ui_set_fall_state_color(rt_uint8_t state)
{
    lv_color_t bg;
    lv_color_t fg;

    switch (state)
    {
    case FALLCHECK_STATE_FALL:
        bg = lv_color_hex(0xE74C3C);
        fg = lv_color_white();
        break;
    case FALLCHECK_STATE_NORMAL:
        bg = lv_color_hex(0x2D9CDB);
        fg = lv_color_white();
        break;
    case FALLCHECK_STATE_IDLE:
    default:
        bg = lv_color_white();
        fg = lv_color_hex(0x1F3A4D);
        break;
    }

    lv_obj_set_style_bg_color(s_fall_state_bar, bg, 0);
    lv_obj_set_style_text_color(s_fall_state_label, fg, 0);
    lv_obj_set_style_text_color(s_fall_detail_label, fg, 0);
}

static void ui_apply_page(void)
{
    if (g_thermofall_page == THERMOFALL_PAGE_FALL)
    {
        lv_obj_add_flag(s_thermo_panel, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(s_fall_panel, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(s_dbg_btn, LV_OBJ_FLAG_HIDDEN);
        lv_label_set_text(s_title_label, "ThermoFall - FallCheck");
        lv_label_set_text(s_switch_label, "Thermo");
    }
    else
    {
        lv_obj_clear_flag(s_thermo_panel, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(s_fall_panel, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(s_dbg_btn, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(s_debug_panel, LV_OBJ_FLAG_HIDDEN);
        lv_label_set_text(s_title_label, "ThermoFall - Thermo");
        lv_label_set_text(s_switch_label, "Fall");
    }

    /* 调试层只在 Fall 页面 + 已开启时才可见 */
    if (g_thermofall_page == THERMOFALL_PAGE_FALL && s_dbg_enabled)
    {
        lv_obj_clear_flag(s_debug_panel, LV_OBJ_FLAG_HIDDEN);
    }
    else
    {
        lv_obj_add_flag(s_debug_panel, LV_OBJ_FLAG_HIDDEN);
    }
}

static void ui_timer_cb(lv_timer_t *timer)
{
    const volatile thermometer_shared_t *thermo = &g_thermometer_shared;
    const volatile fallcheck_shared_t *fall = &g_fallcheck_shared;
    char buf[48];

    (void)timer;

    ui_apply_page();

    ui_set_label_x10(s_temp_value, thermo->temperature, "C");
    ui_set_label_x10(s_humi_value, thermo->humidity, "%");
    lv_label_set_text(s_comfort_label, ui_comfort_text(thermo->temperature, thermo->humidity));

    if (!thermo->sensor_ok)
    {
        lv_label_set_text(s_thermo_state_label, "AHT20 OFFLINE / RETRY");
    }
    else if (thermo->paused)
    {
        lv_label_set_text(s_thermo_state_label, "PAUSED BY KEY");
    }
    else if (thermo->alarm)
    {
        lv_label_set_text(s_thermo_state_label, "ALARM: LIMIT EXCEEDED");
    }
    else
    {
        lv_label_set_text(s_thermo_state_label, "AHT20 ONLINE - I2C LOCAL");
    }

    lv_snprintf(buf, sizeof(buf), "sample: %u", thermo->sequence);
    lv_label_set_text(s_thermo_seq_label, buf);
    ui_set_thermo_state_color(thermo->sensor_ok, thermo->paused, thermo->alarm);

    if (fall->magic != FALLCHECK_SHARED_MAGIC || !fall->sensor_ok)
    {
        lv_label_set_text(s_fall_state_label, "IDLE");
        lv_label_set_text(s_fall_detail_label, "LSM6DS3 STARTING");
        lv_label_set_text(s_acc_value, "X --.-- mg\nY --.-- mg\nZ --.-- mg");
        lv_label_set_text(s_gyro_value, "X --.-- dps\nY --.-- dps\nZ --.-- dps");
        ui_set_fall_state_color(FALLCHECK_STATE_IDLE);
        return;
    }

    lv_label_set_text(s_fall_state_label, ui_fall_state_text(fall->state));
    ui_set_axis_text(s_acc_value, fall->ax, fall->ay, fall->az, "mg", RT_FALSE);
    ui_set_axis_text(s_gyro_value, fall->gx, fall->gy, fall->gz, "dps", RT_TRUE);

    if (fall->state == FALLCHECK_STATE_FALL)
    {
        lv_label_set_text(s_fall_detail_label, fall->muted ? "FALL DETECTED - MUTED" : "FALL DETECTED - ALARM");
    }
    else if (fall->state == FALLCHECK_STATE_NORMAL)
    {
        lv_label_set_text(s_fall_detail_label, "MOVING / WATCHING");
    }
    else
    {
        lv_label_set_text(s_fall_detail_label, "STILL / IDLE");
    }

    lv_snprintf(buf, sizeof(buf), "sample: %u", fall->sequence);
    lv_label_set_text(s_fall_seq_label, buf);
    ui_set_fall_state_color(fall->state);

    /* 调试层只在开启时刷新，关掉时不浪费 10Hz 的字符串格式化开销 */
    if (s_dbg_enabled && g_thermofall_page == THERMOFALL_PAGE_FALL)
    {
        ui_dbg_update();
    }
}

static void ui_switch_cb(lv_event_t *e)
{
    (void)e;

    if (g_thermofall_page == THERMOFALL_PAGE_FALL)
    {
        g_thermofall_page = THERMOFALL_PAGE_THERMO;
    }
    else
    {
        g_thermofall_page = THERMOFALL_PAGE_FALL;
    }

    ui_apply_page();
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

    lv_style_init(&s_style_acc_card);
    lv_style_set_bg_color(&s_style_acc_card, lv_color_hex(0xFFF1E8));

    lv_style_init(&s_style_gyro_card);
    lv_style_set_bg_color(&s_style_gyro_card, lv_color_hex(0xEAF7FF));

    lv_style_init(&s_style_title);
    lv_style_set_text_color(&s_style_title, lv_color_hex(0x1F3A4D));
    lv_style_set_text_font(&s_style_title, &lv_font_montserrat_24);

    lv_style_init(&s_style_temp_value);
    lv_style_set_text_color(&s_style_temp_value, lv_color_hex(0xFF6B6B));
    lv_style_set_text_font(&s_style_temp_value, &lv_font_montserrat_24);

    lv_style_init(&s_style_humi_value);
    lv_style_set_text_color(&s_style_humi_value, lv_color_hex(0x2980B9));
    lv_style_set_text_font(&s_style_humi_value, &lv_font_montserrat_24);

    lv_style_init(&s_style_acc_value);
    lv_style_set_text_color(&s_style_acc_value, lv_color_hex(0xE86B2A));
    lv_style_set_text_font(&s_style_acc_value, &lv_font_montserrat_16);

    lv_style_init(&s_style_gyro_value);
    lv_style_set_text_color(&s_style_gyro_value, lv_color_hex(0x1D78C1));
    lv_style_set_text_font(&s_style_gyro_value, &lv_font_montserrat_16);

    lv_style_init(&s_style_caption);
    lv_style_set_text_color(&s_style_caption, lv_color_hex(0x52616B));
    lv_style_set_text_font(&s_style_caption, &lv_font_montserrat_16);

    lv_style_init(&s_style_orange_caption);
    lv_style_set_text_color(&s_style_orange_caption, lv_color_hex(0xB94F1F));
    lv_style_set_text_font(&s_style_orange_caption, &lv_font_montserrat_16);

    lv_style_init(&s_style_blue_caption);
    lv_style_set_text_color(&s_style_blue_caption, lv_color_hex(0x1D78C1));
    lv_style_set_text_font(&s_style_blue_caption, &lv_font_montserrat_16);

    lv_style_init(&s_style_state);
    lv_style_set_radius(&s_style_state, 12);
    lv_style_set_bg_color(&s_style_state, lv_color_white());
    lv_style_set_bg_opa(&s_style_state, LV_OPA_COVER);
    lv_style_set_pad_left(&s_style_state, 14);
    lv_style_set_pad_right(&s_style_state, 14);
    lv_style_set_pad_top(&s_style_state, 8);
    lv_style_set_pad_bottom(&s_style_state, 8);
}

static lv_obj_t *ui_create_panel(lv_obj_t *parent)
{
    lv_obj_t *panel = lv_obj_create(parent);

    /* 页面面板必须覆盖整个屏幕；如果只用 480x272，
     * 这个 LCD 上底部状态条会漂到数据卡片中间。 */
    lv_obj_set_size(panel, lv_pct(100), lv_pct(100));
    lv_obj_set_style_bg_opa(panel, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(panel, 0, 0);
    lv_obj_set_style_pad_all(panel, 0, 0);
    lv_obj_clear_flag(panel, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);

    return panel;
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

static void ui_create_thermo_panel(lv_obj_t *scr)
{
    lv_obj_t *card;
    lv_obj_t *caption;

    s_thermo_panel = ui_create_panel(scr);

    card = ui_create_card(s_thermo_panel, 230, 170, &s_style_temp_card);
    lv_obj_align(card, LV_ALIGN_TOP_LEFT, 22, 68);
    caption = lv_label_create(card);
    lv_label_set_text(caption, "Temperature");
    lv_obj_add_style(caption, &s_style_caption, 0);
    lv_obj_align(caption, LV_ALIGN_TOP_LEFT, 0, 0);
    s_temp_value = lv_label_create(card);
    lv_label_set_text(s_temp_value, "--.- C");
    lv_obj_add_style(s_temp_value, &s_style_temp_value, 0);
    lv_obj_align(s_temp_value, LV_ALIGN_CENTER, 0, 18);

    card = ui_create_card(s_thermo_panel, 210, 170, &s_style_humi_card);
    lv_obj_align(card, LV_ALIGN_TOP_RIGHT, -22, 68);
    caption = lv_label_create(card);
    lv_label_set_text(caption, "Humidity");
    lv_obj_add_style(caption, &s_style_caption, 0);
    lv_obj_align(caption, LV_ALIGN_TOP_LEFT, 0, 0);
    s_humi_value = lv_label_create(card);
    lv_label_set_text(s_humi_value, "--.- %");
    lv_obj_add_style(s_humi_value, &s_style_humi_value, 0);
    lv_obj_align(s_humi_value, LV_ALIGN_CENTER, 0, 18);

    s_thermo_state_bar = lv_obj_create(s_thermo_panel);
    lv_obj_set_size(s_thermo_state_bar, 420, 68);
    lv_obj_add_style(s_thermo_state_bar, &s_style_state, 0);
    lv_obj_align(s_thermo_state_bar, LV_ALIGN_BOTTOM_LEFT, 22, -22);
    lv_obj_clear_flag(s_thermo_state_bar, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);

    s_thermo_state_label = lv_label_create(s_thermo_state_bar);
    lv_label_set_text(s_thermo_state_label, "AHT20 STARTING - I2C LOCAL");
    lv_obj_set_style_text_color(s_thermo_state_label, lv_color_white(), 0);
    lv_obj_align(s_thermo_state_label, LV_ALIGN_TOP_LEFT, 0, 0);

    s_comfort_label = lv_label_create(s_thermo_state_bar);
    lv_label_set_text(s_comfort_label, "COMFORT  Waiting data");
    lv_obj_set_style_text_color(s_comfort_label, lv_color_white(), 0);
    lv_obj_align(s_comfort_label, LV_ALIGN_BOTTOM_LEFT, 0, 0);

    s_thermo_seq_label = lv_label_create(s_thermo_panel);
    lv_label_set_text(s_thermo_seq_label, "sample: 0");
    lv_obj_add_style(s_thermo_seq_label, &s_style_caption, 0);
    lv_obj_align(s_thermo_seq_label, LV_ALIGN_TOP_LEFT, 30, 268);
}

static void ui_create_fall_panel(lv_obj_t *scr)
{
    lv_obj_t *card;
    lv_obj_t *caption;

    s_fall_panel = ui_create_panel(scr);

    card = ui_create_card(s_fall_panel, 230, 170, &s_style_acc_card);
    lv_obj_align(card, LV_ALIGN_TOP_LEFT, 22, 68);
    caption = lv_label_create(card);
    lv_label_set_text(caption, "ACC");
    lv_obj_add_style(caption, &s_style_orange_caption, 0);
    lv_obj_align(caption, LV_ALIGN_TOP_LEFT, 0, 0);
    s_acc_value = lv_label_create(card);
    lv_label_set_text(s_acc_value, "X --.-- mg\nY --.-- mg\nZ --.-- mg");
    lv_obj_add_style(s_acc_value, &s_style_acc_value, 0);
    lv_obj_align(s_acc_value, LV_ALIGN_TOP_LEFT, 0, 42);

    card = ui_create_card(s_fall_panel, 210, 170, &s_style_gyro_card);
    lv_obj_align(card, LV_ALIGN_TOP_RIGHT, -22, 68);
    caption = lv_label_create(card);
    lv_label_set_text(caption, "GYR");
    lv_obj_add_style(caption, &s_style_blue_caption, 0);
    lv_obj_align(caption, LV_ALIGN_TOP_LEFT, 0, 0);
    s_gyro_value = lv_label_create(card);
    lv_label_set_text(s_gyro_value, "X --.-- dps\nY --.-- dps\nZ --.-- dps");
    lv_obj_add_style(s_gyro_value, &s_style_gyro_value, 0);
    lv_obj_align(s_gyro_value, LV_ALIGN_TOP_LEFT, 0, 42);

    s_fall_state_bar = lv_obj_create(s_fall_panel);
    lv_obj_set_size(s_fall_state_bar, 420, 68);
    lv_obj_add_style(s_fall_state_bar, &s_style_state, 0);
    lv_obj_align(s_fall_state_bar, LV_ALIGN_BOTTOM_LEFT, 22, -22);
    lv_obj_clear_flag(s_fall_state_bar, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);

    s_fall_state_label = lv_label_create(s_fall_state_bar);
    lv_label_set_text(s_fall_state_label, "IDLE");
    lv_obj_set_style_text_color(s_fall_state_label, lv_color_hex(0x1F3A4D), 0);
    lv_obj_align(s_fall_state_label, LV_ALIGN_TOP_LEFT, 0, 0);

    s_fall_detail_label = lv_label_create(s_fall_state_bar);
    lv_label_set_text(s_fall_detail_label, "LSM6DS3 STARTING");
    lv_obj_set_style_text_color(s_fall_detail_label, lv_color_hex(0x1F3A4D), 0);
    lv_obj_align(s_fall_detail_label, LV_ALIGN_BOTTOM_LEFT, 0, 0);

    s_fall_seq_label = lv_label_create(s_fall_panel);
    lv_label_set_text(s_fall_seq_label, "sample: 0");
    lv_obj_add_style(s_fall_seq_label, &s_style_caption, 0);
    lv_obj_align(s_fall_seq_label, LV_ALIGN_TOP_LEFT, 30, 268);
}

static void ui_dbg_toggle_cb(lv_event_t *e)
{
    (void)e;

    s_dbg_enabled = s_dbg_enabled ? RT_FALSE : RT_TRUE;

    /* 进入调试视图时清零峰值，这样读到的峰值就是这一次甩板的 */
    if (s_dbg_enabled)
    {
        g_fallcheck_shared.dbg_reset = 1;
    }

    lv_label_set_text(s_dbg_btn_label, s_dbg_enabled ? "HIDE" : "DBG");
    ui_apply_page();
}

static void ui_dbg_flags_text(char *out, int len, rt_uint32_t flags, lv_color_t *color)
{
    lv_snprintf(out, len, "%s%s%s%s%s%s%s",
                (flags & FALLCHECK_FLAG_IMPACT) ? "IMPACT " : "",
                (flags & FALLCHECK_FLAG_DROP) ? "DROP " : "",
                (flags & FALLCHECK_FLAG_FAST_ROTATE) ? "ROTATE " : "",
                (flags & FALLCHECK_FLAG_IMPACT_ROTATE) ? "IMP_ROT " : "",
                (flags & FALLCHECK_FLAG_POSTURE) ? "POSTURE " : "",
                (flags & FALLCHECK_FLAG_UPRIGHT) ? "UPRIGHT " : "",
                (flags & FALLCHECK_FLAG_STILL) ? "STILL" : "");

    if (out[0] == '\0')
    {
        lv_snprintf(out, len, "-");
        *color = lv_color_hex(0x8FA3B0);
        return;
    }

    /* 会触发候选窗口的判据标红；只是安静/直立用绿色，说明不会误判 */
    if (flags & (FALLCHECK_FLAG_IMPACT | FALLCHECK_FLAG_DROP |
                 FALLCHECK_FLAG_FAST_ROTATE | FALLCHECK_FLAG_IMPACT_ROTATE |
                 FALLCHECK_FLAG_POSTURE))
    {
        *color = lv_color_hex(0xFF4D4D);
    }
    else
    {
        *color = lv_color_hex(0x5DDB8C);
    }
}

static void ui_dbg_update(void)
{
    const volatile fallcheck_shared_t *fall = &g_fallcheck_shared;
    char buf[80];
    lv_color_t color;

    if (fall->magic != FALLCHECK_SHARED_MAGIC || !fall->sensor_ok)
    {
        lv_label_set_text(s_dbg_acc_label, "ACC  |a|      -- mg     peak      -- mg");
        lv_label_set_text(s_dbg_gyro_label, "GYRO sum     -- mdps   peak      -- mdps");
        lv_label_set_text(s_dbg_cnt_label, "still=-  conf=-  hold=-  motion=-  warm=-");
        lv_label_set_text(s_dbg_flag_label, "waiting for LSM6DS3 ...");
        lv_obj_set_style_text_color(s_dbg_flag_label, lv_color_hex(0x8FA3B0), 0);
        lv_label_set_text(s_dbg_state_label, "IDLE   seq=0");
        return;
    }

    lv_snprintf(buf, sizeof(buf), "ACC  |a|  %6d mg   peak %6d mg",
                (int)fall->acc_mag, (int)fall->acc_mag_peak);
    lv_label_set_text(s_dbg_acc_label, buf);
    /* 颜色直接由状态机发布的判据决定，界面里不再重复算一遍阈值 */
    if (fall->flags & FALLCHECK_FLAG_IMPACT)
    {
        lv_obj_set_style_text_color(s_dbg_acc_label, lv_color_hex(0xFF4D4D), 0);
    }
    else if (fall->flags & FALLCHECK_FLAG_DROP)
    {
        lv_obj_set_style_text_color(s_dbg_acc_label, lv_color_hex(0xFFA640), 0);
    }
    else
    {
        lv_obj_set_style_text_color(s_dbg_acc_label, lv_color_white(), 0);
    }

    lv_snprintf(buf, sizeof(buf), "GYRO sum %7d    peak %7d",
                (int)fall->gyro_sum, (int)fall->gyro_sum_peak);
    lv_label_set_text(s_dbg_gyro_label, buf);
    if (fall->flags & (FALLCHECK_FLAG_FAST_ROTATE | FALLCHECK_FLAG_IMPACT_ROTATE))
    {
        lv_obj_set_style_text_color(s_dbg_gyro_label, lv_color_hex(0xFF4D4D), 0);
    }
    else
    {
        lv_obj_set_style_text_color(s_dbg_gyro_label, lv_color_white(), 0);
    }

    lv_snprintf(buf, sizeof(buf), "still=%u  conf=%u  hold=%u  motion=%u  warm=%u",
                fall->cnt_still, fall->cnt_confirm,
                fall->cnt_hold, fall->cnt_motion, fall->cnt_warmup);
    lv_label_set_text(s_dbg_cnt_label, buf);

    ui_dbg_flags_text(buf, sizeof(buf), fall->flags, &color);
    lv_label_set_text(s_dbg_flag_label, buf);
    lv_obj_set_style_text_color(s_dbg_flag_label, color, 0);

    lv_snprintf(buf, sizeof(buf), "%s   mute=%u   seq=%u",
                ui_fall_state_text(fall->state), fall->muted, fall->sequence);
    lv_label_set_text(s_dbg_state_label, buf);
}

static void ui_create_debug_panel(lv_obj_t *scr)
{
    lv_obj_t *title;

    /*
     * 只盖住下半屏：顶部留给页面切换和 DBG 按钮，否则调试层会把按钮挡住，
     * 就没法再点回普通视图了。
     */
    s_debug_panel = lv_obj_create(scr);
    lv_obj_set_size(s_debug_panel, lv_pct(100), 216);
    lv_obj_align(s_debug_panel, LV_ALIGN_BOTTOM_LEFT, 0, 0);
    lv_obj_set_style_bg_color(s_debug_panel, lv_color_hex(0x101820), 0);
    lv_obj_set_style_bg_opa(s_debug_panel, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(s_debug_panel, 0, 0);
    lv_obj_set_style_pad_all(s_debug_panel, 0, 0);
    lv_obj_set_style_radius(s_debug_panel, 0, 0);
    lv_obj_clear_flag(s_debug_panel, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);

    title = lv_label_create(s_debug_panel);
    lv_label_set_text(title, "FALL DEBUG - shake the board, peaks are held");
    lv_obj_set_style_text_color(title, lv_color_hex(0x8FA3B0), 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_16, 0);
    lv_obj_align(title, LV_ALIGN_TOP_LEFT, 16, 4);

    s_dbg_acc_label = lv_label_create(s_debug_panel);
    lv_label_set_text(s_dbg_acc_label, "ACC  |a|      -- mg     peak      -- mg");
    lv_obj_set_style_text_color(s_dbg_acc_label, lv_color_white(), 0);
    lv_obj_set_style_text_font(s_dbg_acc_label, &lv_font_montserrat_16, 0);
    lv_obj_align(s_dbg_acc_label, LV_ALIGN_TOP_LEFT, 16, 30);

    s_dbg_gyro_label = lv_label_create(s_debug_panel);
    lv_label_set_text(s_dbg_gyro_label, "GYRO sum     -- mdps   peak      -- mdps");
    lv_obj_set_style_text_color(s_dbg_gyro_label, lv_color_white(), 0);
    lv_obj_set_style_text_font(s_dbg_gyro_label, &lv_font_montserrat_16, 0);
    lv_obj_align(s_dbg_gyro_label, LV_ALIGN_TOP_LEFT, 16, 56);

    /* 阈值直接引用 exam_shared.h 的同一份常量，避免界面里另抄一份 */
    s_dbg_thr_label = lv_label_create(s_debug_panel);
    lv_label_set_text_fmt(s_dbg_thr_label,
                          "still %d~%d | impact>%d | drop %d~%d | rot>%d | imp_rot>%d",
                          (int)FALLCHECK_IDLE_ACC_LOW_MG, (int)FALLCHECK_IDLE_ACC_HIGH_MG,
                          (int)FALLCHECK_IMPACT_HIGH_MG, (int)FALLCHECK_IMPACT_LOW_MG,
                          (int)FALLCHECK_DROP_ACC_HIGH_MG,
                          (int)(FALLCHECK_ROTATE_SUM_MDPS / 1000),
                          (int)(FALLCHECK_ROTATE_WITH_IMPACT / 1000));
    lv_obj_set_style_text_color(s_dbg_thr_label, lv_color_hex(0x6E8494), 0);
    lv_obj_set_style_text_font(s_dbg_thr_label, &lv_font_montserrat_16, 0);
    lv_obj_align(s_dbg_thr_label, LV_ALIGN_TOP_LEFT, 16, 82);

    s_dbg_cnt_label = lv_label_create(s_debug_panel);
    lv_label_set_text(s_dbg_cnt_label, "still=-  conf=-  hold=-  motion=-  warm=-");
    lv_obj_set_style_text_color(s_dbg_cnt_label, lv_color_hex(0xC8D6E0), 0);
    lv_obj_set_style_text_font(s_dbg_cnt_label, &lv_font_montserrat_16, 0);
    lv_obj_align(s_dbg_cnt_label, LV_ALIGN_TOP_LEFT, 16, 108);

    s_dbg_flag_label = lv_label_create(s_debug_panel);
    lv_label_set_text(s_dbg_flag_label, "-");
    lv_obj_set_style_text_color(s_dbg_flag_label, lv_color_hex(0x8FA3B0), 0);
    lv_obj_set_style_text_font(s_dbg_flag_label, &lv_font_montserrat_16, 0);
    lv_obj_align(s_dbg_flag_label, LV_ALIGN_TOP_LEFT, 16, 134);

    s_dbg_state_label = lv_label_create(s_debug_panel);
    lv_label_set_text(s_dbg_state_label, "IDLE   seq=0");
    lv_obj_set_style_text_color(s_dbg_state_label, lv_color_hex(0x5DDB8C), 0);
    lv_obj_set_style_text_font(s_dbg_state_label, &lv_font_montserrat_16, 0);
    lv_obj_align(s_dbg_state_label, LV_ALIGN_TOP_LEFT, 16, 162);
}

void ui_thermofall_init(void)
{
    lv_obj_t *scr = lv_scr_act();
    lv_obj_t *switch_btn;

    ui_styles_init();
    lv_obj_add_style(scr, &s_style_bg, 0);

    s_title_label = lv_label_create(scr);
    lv_label_set_text(s_title_label, "ThermoFall - Thermo");
    lv_obj_add_style(s_title_label, &s_style_title, 0);
    lv_obj_align(s_title_label, LV_ALIGN_TOP_MID, -20, 16);

    switch_btn = lv_button_create(scr);
    lv_obj_set_size(switch_btn, 92, 42);
    lv_obj_align(switch_btn, LV_ALIGN_TOP_RIGHT, -18, 12);
    lv_obj_add_event_cb(switch_btn, ui_switch_cb, LV_EVENT_CLICKED, RT_NULL);

    s_switch_label = lv_label_create(switch_btn);
    lv_label_set_text(s_switch_label, "Fall");
    lv_obj_center(s_switch_label);

    /* 调试视图开关，放在切换按钮左边，仅在 Fall 页面显示 */
    s_dbg_btn = lv_button_create(scr);
    lv_obj_set_size(s_dbg_btn, 62, 42);
    lv_obj_align_to(s_dbg_btn, switch_btn, LV_ALIGN_OUT_LEFT_MID, -8, 0);
    lv_obj_add_event_cb(s_dbg_btn, ui_dbg_toggle_cb, LV_EVENT_CLICKED, RT_NULL);

    s_dbg_btn_label = lv_label_create(s_dbg_btn);
    lv_label_set_text(s_dbg_btn_label, "DBG");
    lv_obj_center(s_dbg_btn_label);

    ui_create_thermo_panel(scr);
    ui_create_fall_panel(scr);
    ui_create_debug_panel(scr);
    ui_apply_page();

    /* 100ms 与传感器采样周期一致，否则摔倒过程会被整拍跳过 */
    lv_timer_create(ui_timer_cb, 100, RT_NULL);
}
