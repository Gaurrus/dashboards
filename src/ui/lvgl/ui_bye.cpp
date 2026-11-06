#include "ui_bye.h"

#include "bye_gif.h"
#include "lvgl.h"
#include "ui_theme.h"

namespace {

using namespace ui::theme;

constexpr uint32_t BYE_FADE_IN_MS = 350;
constexpr uint32_t BYE_HOLD_MS = 2200;
constexpr uint32_t BYE_FADE_OUT_MS = 700;


lv_obj_t* g_bye = nullptr;
lv_obj_t* g_byeGif = nullptr;
lv_obj_t* g_byeContent = nullptr;
bool g_ignitionOff = false;
bool g_byeFadingOut = false;
uint32_t g_ignitionOffMs = 0;


void byeContentOpaAnimCb(void* var, int32_t value) {
  lv_obj_set_style_opa((lv_obj_t*)var, (lv_opa_t)value, 0);
}

lv_obj_t* createByeScreen(lv_obj_t* parent) {
  lv_obj_t* bye = lv_obj_create(parent);
  lv_obj_remove_style_all(bye);
  lv_obj_set_size(bye, LV_HOR_RES, LV_VER_RES);
  lv_obj_set_pos(bye, 0, 0);
  lv_obj_set_style_bg_color(bye, lv_color_black(), 0);
  lv_obj_set_style_bg_opa(bye, LV_OPA_COVER, 0);
  lv_obj_clear_flag(bye, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_clear_flag(bye, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_flag(bye, LV_OBJ_FLAG_HIDDEN);

  g_byeContent = lv_obj_create(bye);
  lv_obj_remove_style_all(g_byeContent);
  lv_obj_set_size(g_byeContent, LV_HOR_RES, LV_VER_RES);
  lv_obj_set_pos(g_byeContent, 0, 0);
  lv_obj_set_style_bg_opa(g_byeContent, LV_OPA_TRANSP, 0);
  lv_obj_clear_flag(g_byeContent, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_clear_flag(g_byeContent, LV_OBJ_FLAG_CLICKABLE);

  lv_obj_t* gif = lv_gif_create(g_byeContent);
  lv_gif_set_src(gif, &bye_gif);
  lv_obj_align(gif, LV_ALIGN_CENTER, 0, 0);
  g_byeGif = gif;
  lv_timer_pause(((lv_gif_t*)gif)->timer);

  lv_obj_t* label =
      makeLabel(g_byeContent, "Transporter", &lv_font_montserrat_32, COL_TEXT);
  lv_obj_align(label, LV_ALIGN_BOTTOM_MID, 0, -16);

  lv_obj_move_foreground(bye);
  return bye;
}

void showByeScreen() {
  g_ignitionOff = true;
  g_byeFadingOut = false;
  g_ignitionOffMs = lv_tick_get();

  if (g_byeContent != nullptr) {
    lv_anim_del(g_byeContent, byeContentOpaAnimCb);
    lv_obj_set_style_opa(g_byeContent, LV_OPA_TRANSP, 0);

    lv_anim_t anim;
    lv_anim_init(&anim);
    lv_anim_set_var(&anim, g_byeContent);
    lv_anim_set_values(&anim, LV_OPA_TRANSP, LV_OPA_COVER);
    lv_anim_set_time(&anim, BYE_FADE_IN_MS);
    lv_anim_set_exec_cb(&anim, byeContentOpaAnimCb);
    lv_anim_start(&anim);
  }

  lv_obj_clear_flag(g_bye, LV_OBJ_FLAG_HIDDEN);
  lv_obj_move_foreground(g_bye);

  if (g_byeGif != nullptr) {
    lv_gif_restart(g_byeGif);
  }
}

void hideByeScreen() {
  g_ignitionOff = false;
  g_byeFadingOut = false;
  if (g_byeContent != nullptr) {
    lv_anim_del(g_byeContent, byeContentOpaAnimCb);
    lv_obj_set_style_opa(g_byeContent, LV_OPA_COVER, 0);
  }
  lv_obj_add_flag(g_bye, LV_OBJ_FLAG_HIDDEN);
  if (g_byeGif != nullptr) {
    lv_timer_pause(((lv_gif_t*)g_byeGif)->timer);
  }
}


}  // namespace

namespace ui {
namespace internal {

void byeInit(lv_obj_t* parent) {
  g_bye = createByeScreen(parent);
}

bool byeUpdate(bool ignitionOn) {
  if (!ignitionOn) {
    if (!g_ignitionOff) {
      showByeScreen();
    } else if (!g_byeFadingOut &&
               lv_tick_elaps(g_ignitionOffMs) >= BYE_HOLD_MS) {
      g_byeFadingOut = true;
      lv_anim_t anim;
      lv_anim_init(&anim);
      lv_anim_set_var(&anim, g_byeContent);
      lv_anim_set_values(&anim, LV_OPA_COVER, LV_OPA_TRANSP);
      lv_anim_set_time(&anim, BYE_FADE_OUT_MS);
      lv_anim_set_exec_cb(&anim, byeContentOpaAnimCb);
      lv_anim_start(&anim);
    }
    return true;
  }

  if (g_ignitionOff) {
    hideByeScreen();
  }
  return false;
}

bool byeActive() {
  return g_ignitionOff;
}

}  // namespace internal
}  // namespace ui
