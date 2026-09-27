/**
 * @file    ui_core.c
 * @brief   Điều phối: header (tiêu đề + đồng hồ), footer (gợi ý / thông báo + số trang),
 *          chuyển màn hình, phân phối phím.
 */
#include "ui_internal.h"

ui_ctx_t g_ui;

static const ui_screen_t *const s_screens[SCR_COUNT] = {
    [SCR_MAIN]   = &scr_main,
    [SCR_RUN]    = &scr_run,
    [SCR_TIMER]  = &scr_timer,
    [SCR_FAULTS] = &scr_faults,
    [SCR_PRESET] = &scr_preset,
    [SCR_EDIT]   = &scr_edit,
    [SCR_TECH]   = &scr_tech,
};

/* ---------------- header / footer ---------------- */
static void draw_header(const ui_screen_t *s)
{
    ILI9341_FillRect(0, 0, TFT_WIDTH, UI_HEAD_H, UC_HEAD);
    ILI9341_DrawString(8, 6, s->title, C_WHITE, UC_HEAD, 2);
    g_ui.clock_drawn[0] = '\0';
}

static void draw_clock(void)
{
    char c[16];
    if (g_ui.v.clock_ok) snprintf(c, sizeof(c), "%02u:%02u:%02u", g_ui.v.hour, g_ui.v.min, g_ui.v.sec);
    else                 snprintf(c, sizeof(c), "--:--:--");
    if (strcmp(c, g_ui.clock_drawn) == 0) return;
    strcpy(g_ui.clock_drawn, c);
    ILI9341_DrawString(TFT_WIDTH - 8 - 8 * 12, 6, c, g_ui.v.clock_ok ? C_WHITE : UC_LABEL, UC_HEAD, 2);
}

static void draw_footer_static(const ui_screen_t *s)
{
    ILI9341_FillRect(0, UI_FOOT_Y, TFT_WIDTH, TFT_HEIGHT - UI_FOOT_Y, UC_FOOT);
    if (s->page >= 0) {
        for (int i = 0; i < UI_PAGE_COUNT; i++) {
            ILI9341_FillRect((int16_t)(270 + i * 12), 228, 9, 7, (i == s->page) ? UC_ACCENT : UC_OFF);
        }
    }
    g_ui.foot_drawn[0] = '\0';
}

static void draw_footer_text(const ui_screen_t *s)
{
    bool msg = g_ui.msg[0] && (g_ui.now - g_ui.msg_tick) < UI_MSG_MS;
    const char *txt = msg ? g_ui.msg : (s->hint ? s->hint() : "");
    char key[48];
    snprintf(key, sizeof(key), "%c%s", msg ? 'M' : 'H', txt);
    if (strcmp(key, g_ui.foot_drawn) == 0) return;
    strcpy(g_ui.foot_drawn, key);

    int16_t w = (s->page >= 0) ? 262 : TFT_WIDTH;
    ILI9341_FillRect(0, UI_FOOT_Y, w, TFT_HEIGHT - UI_FOOT_Y, UC_FOOT);
    if (msg) ILI9341_DrawString(6, UI_FOOT_Y + 2, txt, UC_ACCENT, UC_FOOT, 2);
    else     ILI9341_DrawString(6, UI_FOOT_Y + 6, txt, UC_LABEL, UC_FOOT, 1);
}

/* ---------------- API nội bộ ---------------- */
void ui_goto(ui_scr_t s)
{
    if (s >= SCR_COUNT) return;
    g_ui.scr = s;
    g_ui.redraw = true;
    if (s_screens[s]->enter) s_screens[s]->enter();
}

bool ui_send(const ui_cmd_t *cmd)
{
    return (g_ui.cfg && g_ui.cfg->on_cmd) ? g_ui.cfg->on_cmd(cmd) : false;
}

bool ui_page_nav(ui_key_t k, ui_press_t p)
{
    int8_t page = s_screens[g_ui.scr]->page;
    if (page < 0) return false;

    if (k == UI_KEY_ENTER && p == UI_PRESS_LONG) { ui_goto(SCR_PRESET); return true; }
    if (p != UI_PRESS_SHORT) return false;
    switch (k) {
    case UI_KEY_UP:   ui_goto((ui_scr_t)((page + UI_PAGE_COUNT - 1) % UI_PAGE_COUNT)); return true;
    case UI_KEY_DOWN: ui_goto((ui_scr_t)((page + 1) % UI_PAGE_COUNT));                 return true;
    case UI_KEY_EXIT: if (g_ui.scr != SCR_MAIN) { ui_goto(SCR_MAIN); return true; }  break;
    default: break;
    }
    return false;
}

/* ---------------- API ---------------- */
void UI_Init(const ui_config_t *cfg)
{
    memset(&g_ui, 0, sizeof(g_ui));
    g_ui.cfg = cfg;
    ui_goto(SCR_MAIN);
}

void UI_Message(const char *msg)
{
    snprintf(g_ui.msg, sizeof(g_ui.msg), "%s", msg);
    g_ui.msg_tick = g_ui.now;
}

void UI_Key(ui_key_t key, ui_press_t press)
{
    const ui_screen_t *s = s_screens[g_ui.scr];
    if (s->key) s->key(key, press);
    g_ui.dirty = true;
}

void UI_Update(const ui_view_t *view, uint32_t now_ms)
{
    g_ui.v = *view;
    g_ui.now = now_ms;
    const ui_screen_t *s = s_screens[g_ui.scr];

    if (g_ui.redraw) {
        g_ui.redraw = false;
        ILI9341_FillRect(0, UI_BODY_Y, TFT_WIDTH, UI_FOOT_Y - UI_BODY_Y, UC_BG);
        draw_header(s);
        draw_footer_static(s);
        if (s->draw_static) s->draw_static();
        if (s->draw_values) s->draw_values(true);
        g_ui.last_draw = now_ms;
        g_ui.dirty = false;
    } else if (g_ui.dirty || (now_ms - g_ui.last_draw) >= s->refresh_ms) {
        if (s->draw_values) s->draw_values(false);
        g_ui.last_draw = now_ms;
        g_ui.dirty = false;
    }
    draw_clock();
    draw_footer_text(s);
}
