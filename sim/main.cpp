#include <SDL2/SDL.h>

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>

#include "demo_values.h"
#include "lvgl.h"
#include "ui_lvgl.h"
#include "version.h"

namespace {

constexpr int kWidth = 800;
constexpr int kHeight = 480;
constexpr uint32_t kFrameMs = 33;  // ~30 FPS, same cadence as the dashboard
constexpr float kEspRenderSlowdown = 2.0f;  // host -> ESP32-S3 render cost model

SDL_Window* gWindow = nullptr;
SDL_Renderer* gRenderer = nullptr;
SDL_Texture* gTexture = nullptr;
bool gDirty = false;

// Full-screen LVGL draw buffer; also used for BMP screenshots.
lv_color_t gFramebuffer[kWidth * kHeight];
lv_disp_draw_buf_t gDrawBuf;
lv_disp_drv_t gDispDrv;
lv_indev_drv_t gMouseDrv;

lv_obj_t* gPerfPanel = nullptr;
lv_obj_t* gPerfFpsLabel = nullptr;
lv_obj_t* gPerfCpuLabel = nullptr;
lv_obj_t* gPerfEspLabel = nullptr;
lv_obj_t* gPerfMemLabel = nullptr;
uint32_t gFrameCounter = 0;
uint32_t gPerfLastMs = 0;
clock_t gPerfLastCpu = 0;
uint64_t gPerfFreq = 0;
float gRenderMs = 0.0f;

int gMouseX = 0;
int gMouseY = 0;
bool gMouseDown = false;
bool gIgnitionOn = true;

void perfUpdateCb(lv_timer_t* /*timer*/) {
  const uint32_t now = SDL_GetTicks();
  const uint32_t elapsed = now - gPerfLastMs;
  if (elapsed < 100) {
    return;
  }

  const float seconds = (float)elapsed / 1000.0f;
  const uint32_t fps = (uint32_t)((float)gFrameCounter / seconds + 0.5f);
  gFrameCounter = 0;
  gPerfLastMs = now;

  const clock_t cpuNow = clock();
  const float cpu =
      (float)(cpuNow - gPerfLastCpu) / (float)CLOCKS_PER_SEC / seconds * 100.0f;
  gPerfLastCpu = cpuNow;

  lv_mem_monitor_t mon;
  lv_mem_monitor(&mon);
  const uint32_t usedKb = (mon.total_size - mon.free_size) / 1024U;
  const float espCpu = fminf(
      100.0f, gRenderMs * kEspRenderSlowdown / (1000.0f / 30.0f) * 100.0f);

  char text[48];
  snprintf(text, sizeof(text), "FPS: %u", (unsigned)fps);
  lv_label_set_text(gPerfFpsLabel, text);
  snprintf(text, sizeof(text), "CPU host: %u%%", (unsigned)(cpu + 0.5f));
  lv_label_set_text(gPerfCpuLabel, text);
  snprintf(text, sizeof(text), "CPU esp: %u%%", (unsigned)(espCpu + 0.5f));
  lv_label_set_text(gPerfEspLabel, text);
  snprintf(text, sizeof(text), "RAM: %u%% %uK frag %u%%", (unsigned)mon.used_pct,
           (unsigned)usedKb, (unsigned)mon.frag_pct);
  lv_label_set_text(gPerfMemLabel, text);
}

void createPerfOverlay() {
  gPerfPanel = lv_obj_create(lv_layer_sys());
  lv_obj_remove_style_all(gPerfPanel);
  lv_obj_set_size(gPerfPanel, 195, 78);
  lv_obj_align(gPerfPanel, LV_ALIGN_BOTTOM_LEFT, 8, -8);
  lv_obj_set_style_bg_color(gPerfPanel, lv_color_hex(0x11161B), 0);
  lv_obj_set_style_bg_opa(gPerfPanel, (lv_opa_t)215, 0);
  lv_obj_set_style_border_width(gPerfPanel, 1, 0);
  lv_obj_set_style_border_color(gPerfPanel, lv_color_hex(0x2A2F35), 0);
  lv_obj_set_style_radius(gPerfPanel, 6, 0);
  lv_obj_clear_flag(gPerfPanel, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_clear_flag(gPerfPanel, LV_OBJ_FLAG_CLICKABLE);

  auto make = [&](int y) {
    lv_obj_t* label = lv_label_create(gPerfPanel);
    lv_obj_set_style_text_font(label, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(label, lv_color_hex(0xE6E6E6), 0);
    lv_obj_set_pos(label, 10, y);
    return label;
  };
  gPerfFpsLabel = make(5);
  gPerfCpuLabel = make(23);
  gPerfEspLabel = make(41);
  gPerfMemLabel = make(59);
  lv_label_set_text(gPerfFpsLabel, "FPS: --");
  lv_label_set_text(gPerfCpuLabel, "CPU host: --");
  lv_label_set_text(gPerfEspLabel, "CPU esp: --");
  lv_label_set_text(gPerfMemLabel, "RAM: --");

  gPerfFreq = SDL_GetPerformanceFrequency();
  gPerfLastMs = SDL_GetTicks();
  gPerfLastCpu = clock();
  lv_timer_create(perfUpdateCb, 500, nullptr);
}

void mouseReadCb(lv_indev_drv_t* /*drv*/, lv_indev_data_t* data) {
  data->point.x = gMouseX;
  data->point.y = gMouseY;
  data->state = gMouseDown ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
}

// Drives a scripted brightness swipe for the preview video:
// 7.0-8.2 s swipe down, 10.0-11.2 s swipe up.
void updateBrightnessDemoMouse(float t) {
  gMouseX = 760;
  if (t >= 7.0f && t < 8.2f) {
    gMouseDown = true;
    gMouseY = 120 + (int)((t - 7.0f) / 1.2f * 240.0f);
  } else if (t >= 10.0f && t < 11.2f) {
    gMouseDown = true;
    gMouseY = 360 - (int)((t - 10.0f) / 1.2f * 180.0f);
  } else {
    gMouseDown = false;
    gMouseY = 360;
  }
}

// Timeline for the "all features" preview:
// 8-9.2 s dim, 11-12.2 s brighten, 16-16.8 s back to 100%.
void updateAllDemoMouse(float t) {
  gMouseX = 760;
  gMouseDown = false;
  gMouseY = 360;
  if (t >= 7.5f && t < 8.7f) {
    gMouseDown = true;
    gMouseY = 120 + (int)((t - 7.5f) / 1.2f * 240.0f);
  } else if (t >= 10.2f && t < 11.4f) {
    gMouseDown = true;
    gMouseY = 360 - (int)((t - 10.2f) / 1.2f * 180.0f);
  } else if (t >= 12.8f && t < 13.0f) {
    // Tap the analog clock to open SET TIME.
    gMouseX = 120;
    gMouseY = 230;
    gMouseDown = (t < 12.92f);
  } else if (t >= 13.6f && t < 14.4f) {
    // Drag the minute roller.
    gMouseX = 500;
    gMouseDown = true;
    gMouseY = 240 - (int)((t - 13.6f) / 0.8f * 50.0f);
  } else if (t >= 15.0f && t < 15.15f) {
    // Tap SET.
    gMouseX = 500;
    gMouseY = 345;
    gMouseDown = (t < 15.07f);
  } else if (t >= 16.0f && t < 16.8f) {
    gMouseDown = true;
    gMouseY = 360 - (int)((t - 16.0f) / 0.8f * 240.0f);
  }
}

void present() {
  SDL_RenderClear(gRenderer);
  SDL_RenderCopy(gRenderer, gTexture, nullptr, nullptr);
  SDL_RenderPresent(gRenderer);
}

uint32_t toArgb(lv_color_t color) {
  return lv_color_to32(color);
}

void flushCb(lv_disp_drv_t* drv, const lv_area_t* area, lv_color_t* color_p) {
  static uint32_t row[kWidth];

  const int w = area->x2 - area->x1 + 1;
  for (int y = area->y1; y <= area->y2; ++y) {
    for (int x = 0; x < w; ++x) {
      row[x] = toArgb(color_p[x]);
    }
    SDL_Rect rect;
    rect.x = area->x1;
    rect.y = y;
    rect.w = w;
    rect.h = 1;
    SDL_UpdateTexture(gTexture, &rect, row, w * (int)sizeof(uint32_t));
    color_p += w;
  }

  gDirty = true;
  lv_disp_flush_ready(drv);
}

void put16(uint8_t* p, uint16_t v) {
  p[0] = (uint8_t)(v & 0xFF);
  p[1] = (uint8_t)((v >> 8) & 0xFF);
}

void put32(uint8_t* p, uint32_t v) {
  p[0] = (uint8_t)(v & 0xFF);
  p[1] = (uint8_t)((v >> 8) & 0xFF);
  p[2] = (uint8_t)((v >> 16) & 0xFF);
  p[3] = (uint8_t)((v >> 24) & 0xFF);
}

void saveBmp(const char* path) {
  const int rowBytes = kWidth * 4;
  const int imageSize = rowBytes * kHeight;

  uint8_t header[54] = {};
  header[0] = 'B';
  header[1] = 'M';
  put32(header + 2, (uint32_t)(54 + imageSize));
  put32(header + 10, 54);
  put32(header + 14, 40);
  put32(header + 18, (uint32_t)kWidth);
  put32(header + 22, (uint32_t)(-kHeight));
  put16(header + 26, 1);
  put16(header + 28, 32);
  put32(header + 34, (uint32_t)imageSize);

  FILE* file = fopen(path, "wb");
  if (!file) {
    return;
  }
  fwrite(header, 1, sizeof(header), file);

  static uint32_t row[kWidth];
  for (int y = 0; y < kHeight; ++y) {
    const lv_color_t* src = &gFramebuffer[y * kWidth];
    for (int x = 0; x < kWidth; ++x) {
      row[x] = toArgb(src[x]);
    }
    fwrite(row, 1, (size_t)rowBytes, file);
  }
  fclose(file);
}

}  // namespace

int main(int argc, char** argv) {
  if (argc >= 2 && strcmp(argv[1], "--version") == 0) {
    printf("%s\n", T3DASH_VERSION);
    return 0;
  }

  const bool screenshot = argc >= 2;
  const bool viaSdl = argc >= 3 && SDL_strcmp(argv[2], "sdl") == 0;
  const bool bootShot = argc >= 3 && SDL_strcmp(argv[2], "boot") == 0;
  const bool sweepShot = argc >= 3 && SDL_strcmp(argv[2], "sweep") == 0;
  const bool hotShot = argc >= 3 && SDL_strcmp(argv[2], "hot") == 0;
  const bool lowShot = argc >= 3 && SDL_strcmp(argv[2], "low") == 0;
  const bool brakeShot = argc >= 3 && SDL_strcmp(argv[2], "brake") == 0;
  const bool brakeFaultShot = argc >= 3 && SDL_strcmp(argv[2], "brakefault") == 0;
  const bool videoShot = argc >= 3 && SDL_strcmp(argv[2], "video") == 0;
  const bool videoBrightShot = argc >= 3 && SDL_strcmp(argv[2], "video_bright") == 0;
  const bool videoAllShot = argc >= 3 && SDL_strcmp(argv[2], "video_all") == 0;
  const bool dimShot = argc >= 3 && SDL_strcmp(argv[2], "dim") == 0;
  const bool swipeShot = argc >= 3 && SDL_strcmp(argv[2], "swipe") == 0;
  const bool byeShot = argc >= 3 && SDL_strcmp(argv[2], "bye") == 0;
  const bool clockShot = argc >= 3 && SDL_strcmp(argv[2], "clock") == 0;
  const bool clocksetShot = argc >= 3 && SDL_strcmp(argv[2], "clockset") == 0;
  const bool perfShot = argc >= 3 && SDL_strcmp(argv[2], "perf") == 0;
  const bool dialMenuShot = argc >= 3 && SDL_strcmp(argv[2], "dialmenu") == 0;
  const bool dialPickShot = argc >= 3 && SDL_strcmp(argv[2], "dialpick") == 0;
  const bool dialShot = dialMenuShot || dialPickShot;
  const bool design2ColorShot =
      argc >= 3 && SDL_strcmp(argv[2], "design2color") == 0;
  const bool designListShot =
      argc >= 3 && SDL_strcmp(argv[2], "designlist") == 0;
  const bool design3ColorShot =
      argc >= 3 && SDL_strcmp(argv[2], "design3color") == 0;
  const bool design3Shot =
      design3ColorShot || (argc >= 3 && SDL_strcmp(argv[2], "design3") == 0);
  const bool design2Shot =
      design2ColorShot || design3Shot ||
      (argc >= 3 && SDL_strcmp(argv[2], "design2") == 0);

  if (SDL_Init(SDL_INIT_VIDEO) != 0) {
    fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
    return 1;
  }

  gWindow = SDL_CreateWindow("VW T3 Dashboard", SDL_WINDOWPOS_CENTERED,
                             SDL_WINDOWPOS_CENTERED, kWidth, kHeight, 0);
  if (!gWindow) {
    fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError());
    return 1;
  }
  SDL_RaiseWindow(gWindow);

  gRenderer = SDL_CreateRenderer(gWindow, -1,
                                 SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
  if (!gRenderer) {
    gRenderer = SDL_CreateRenderer(gWindow, -1, SDL_RENDERER_SOFTWARE);
  }
  if (!gRenderer) {
    fprintf(stderr, "SDL_CreateRenderer failed: %s\n", SDL_GetError());
    return 1;
  }

  gTexture = SDL_CreateTexture(gRenderer, SDL_PIXELFORMAT_ARGB8888,
                               SDL_TEXTUREACCESS_STREAMING, kWidth, kHeight);
  if (!gTexture) {
    fprintf(stderr, "SDL_CreateTexture failed: %s\n", SDL_GetError());
    return 1;
  }

  SDL_SetRenderDrawColor(gRenderer, 0, 0, 0, 255);
  SDL_RenderClear(gRenderer);
  SDL_RenderPresent(gRenderer);

  lv_init();

  lv_disp_draw_buf_init(&gDrawBuf, gFramebuffer, nullptr, kWidth * kHeight);
  lv_disp_drv_init(&gDispDrv);
  gDispDrv.hor_res = kWidth;
  gDispDrv.ver_res = kHeight;
  gDispDrv.flush_cb = flushCb;
  gDispDrv.draw_buf = &gDrawBuf;
  gDispDrv.full_refresh = 1;
  lv_disp_drv_register(&gDispDrv);

  lv_indev_drv_init(&gMouseDrv);
  gMouseDrv.type = LV_INDEV_TYPE_POINTER;
  gMouseDrv.read_cb = mouseReadCb;
  lv_indev_drv_register(&gMouseDrv);

  ui::init();
  createPerfOverlay();
  if (screenshot && !perfShot) {
    lv_obj_add_flag(lv_layer_sys(), LV_OBJ_FLAG_HIDDEN);
  }
  if (screenshot && !bootShot && !sweepShot && !hotShot && !videoShot &&
      !videoBrightShot && !videoAllShot && !perfShot && !dialShot) {
    ui::skipBoot();
  }
  if (dialShot) {
    ui::update(demo::make(0.0f));
    // Hold inside the speedometer dial for 3 s to open the arc color menu.
    gMouseX = 400;
    gMouseY = 233;
    gMouseDown = true;
    for (int i = 0; i < 40; ++i) {
      lv_tick_inc(90);
      lv_timer_handler();
    }
    gMouseDown = false;
    lv_tick_inc(90);
    lv_timer_handler();
    if (dialPickShot) {
      // Two full pick cycles in one run: green (row 0 col 1), then blue
      // (row 1 col 0). Ends blue only if repeat picks keep working.
      for (int pick = 0; pick < 2; ++pick) {
        for (int i = 0; i < 4; ++i) {
          lv_tick_inc(60);
          lv_timer_handler();
        }
        gMouseX = 400;
        gMouseY = 233;
        gMouseDown = true;
        for (int i = 0; i < 40; ++i) {
          lv_tick_inc(90);
          lv_timer_handler();
        }
        gMouseDown = false;
        for (int i = 0; i < 4; ++i) {
          lv_tick_inc(60);
          lv_timer_handler();
        }
        gMouseX = pick == 0 ? 358 : 274;
        gMouseY = pick == 0 ? 209 : 293;
        gMouseDown = true;
        for (int i = 0; i < 4; ++i) {
          lv_tick_inc(60);
          lv_timer_handler();
        }
        gMouseDown = false;
        for (int i = 0; i < 4; ++i) {
          lv_tick_inc(60);
          lv_timer_handler();
        }
      }
    }
  }
  if (design2Shot) {
    ui::update(demo::make(0.6f));
    lv_tick_inc(40);
    lv_timer_handler();
    // Tap the gear button in the bottom-left corner.
    gMouseX = 44;
    gMouseY = 436;
    gMouseDown = true;
    for (int i = 0; i < 4; ++i) {
      lv_tick_inc(40);
      lv_timer_handler();
    }
    gMouseDown = false;
    for (int i = 0; i < 4; ++i) {
      lv_tick_inc(40);
      lv_timer_handler();
    }
    // Pick COMPACT / SEGMENTED from the design list.
    gMouseX = 400;
    gMouseY = design3Shot ? 246 : 302;
    gMouseDown = true;
    for (int i = 0; i < 4; ++i) {
      lv_tick_inc(40);
      lv_timer_handler();
    }
    gMouseDown = false;
    for (int i = 0; i < 4; ++i) {
      lv_tick_inc(40);
      lv_timer_handler();
    }
    // Align the blink phase so the hazard arrows are visible in the shot.
    if (((lv_tick_get() / 350U) & 1U) != 0U) {
      lv_tick_inc(350);
      lv_timer_handler();
    }
    ui::update(demo::make(0.6f));
    lv_tick_inc(40);
    lv_timer_handler();
  }
  if (designListShot) {
    ui::update(demo::make(0.0f));
    lv_tick_inc(40);
    lv_timer_handler();
    gMouseX = 44;
    gMouseY = 436;
    gMouseDown = true;
    for (int i = 0; i < 4; ++i) {
      lv_tick_inc(40);
      lv_timer_handler();
    }
    gMouseDown = false;
    for (int i = 0; i < 4; ++i) {
      lv_tick_inc(40);
      lv_timer_handler();
    }
    ui::update(demo::make(0.0f));
    lv_tick_inc(40);
    lv_timer_handler();
  }
  if (design2ColorShot || design3ColorShot) {
    // Hold the speed area for 3 s to open ARC COLOR.
    gMouseX = design3ColorShot ? 400 : 200;
    gMouseY = design3ColorShot ? 233 : 180;
    gMouseDown = true;
    for (int i = 0; i < 40; ++i) {
      lv_tick_inc(90);
      lv_timer_handler();
    }
    gMouseDown = false;
    for (int i = 0; i < 6; ++i) {
      lv_tick_inc(60);
      lv_timer_handler();
    }
    // Pick green (row 0, column 1).
    gMouseX = 358;
    gMouseY = 209;
    gMouseDown = true;
    for (int i = 0; i < 4; ++i) {
      lv_tick_inc(60);
      lv_timer_handler();
    }
    gMouseDown = false;
    for (int i = 0; i < 6; ++i) {
      lv_tick_inc(60);
      lv_timer_handler();
    }
    ui::update(demo::make(2.0f));
    lv_tick_inc(40);
    lv_timer_handler();
  }
  if (perfShot) {
    ui::skipBoot();
    float pt = 0.0f;
    for (int i = 0; i < 60; ++i) {
      const uint32_t frameStart = SDL_GetTicks();
      const uint64_t renderStart = SDL_GetPerformanceCounter();
      ui::update(demo::make(pt));
      lv_tick_inc(33);
      lv_timer_handler();
      if (gPerfFreq > 0) {
        const float ms = (float)(SDL_GetPerformanceCounter() - renderStart) *
                         1000.0f / (float)gPerfFreq;
        gRenderMs = gRenderMs * 0.9f + ms * 0.1f;
      }
      ++gFrameCounter;
      pt += 0.033f;
      const uint32_t frameTime = SDL_GetTicks() - frameStart;
      if (frameTime < kFrameMs) {
        SDL_Delay(kFrameMs - frameTime);
      }
    }
  }
  if (sweepShot) {
    ui::update(demo::make(0.0f));
    lv_tick_inc(2300);
  }
  if (hotShot) {
    ui::update(demo::make(0.0f));
    lv_tick_inc(2750);
  }
  if (dimShot) {
    ui::setBrightness(40);
  }
  if (swipeShot) {
    // Synthetic swipe down along the right edge lowers the brightness.
    gMouseX = 760;
    gMouseY = 120;
    gMouseDown = true;
    for (int i = 0; i <= 10; ++i) {
      gMouseY = 120 + i * 20;
      lv_tick_inc(16);
      lv_timer_handler();
    }
    gMouseDown = false;
    lv_tick_inc(16);
    lv_timer_handler();
  }
  if (clockShot) {
    ui::setClock(10, 8, 0);
  }
  if (clocksetShot) {
    ui::setClock(10, 8, 0);
    ui::update(demo::make(0.0f));
    gMouseX = 120;
    gMouseY = 230;
    gMouseDown = true;
    for (int i = 0; i < 3; ++i) {
      lv_tick_inc(20);
      lv_timer_handler();
    }
    gMouseDown = false;
    for (int i = 0; i < 3; ++i) {
      lv_tick_inc(20);
      lv_timer_handler();
    }
  }
  if (byeShot) {
    // Optional third argument: how long the ignition stays off, in ms.
    const int holdMs = argc >= 4 ? atoi(argv[3]) : 400;
    ui::update(demo::make(0.0f, false));
    for (int i = 0; i < holdMs / 20; ++i) {
      lv_tick_inc(20);
      ui::update(demo::make(0.0f, false));
      lv_timer_handler();
    }
  }

  if (videoShot || videoBrightShot || videoAllShot) {
    // Usage: t3dash_sim <output_dir> video|video_bright|video_all [seconds] [fps]
    const float seconds = argc >= 4 ? (float)atof(argv[3]) : 10.0f;
    const int fps = argc >= 5 ? atoi(argv[4]) : 30;
    const int frameCount = (int)(seconds * fps + 0.5f);
    for (int i = 0; i < frameCount; ++i) {
      const float ft = (float)i / (float)fps;
      bool ignitionOn = true;
      if (videoBrightShot) {
        updateBrightnessDemoMouse(ft);
      } else if (videoAllShot) {
        updateAllDemoMouse(ft);
        ignitionOn = !(ft >= 18.0f && ft < 23.0f);
      }
      ui::update(demo::make(ft, ignitionOn));
      lv_tick_inc(1000 / fps);
      lv_timer_handler();
      lv_refr_now(lv_disp_get_default());
      char path[512];
      snprintf(path, sizeof(path), "%s/frame_%04d.bmp", argv[1], i);
      saveBmp(path);
    }
    return 0;
  }

  float t = lowShot ? 6.0f
                    : (brakeShot ? 4.0f
                                 : (brakeFaultShot ? 24.0f
                                                   : (design2Shot ? 2.0f : 0.0f)));
  uint32_t lastTick = SDL_GetTicks();

  for (int frame = 0; frame < 4; ++frame) {
    ui::update(demo::make(t, !byeShot));
    const uint32_t now = SDL_GetTicks();
    lv_tick_inc(now - lastTick);
    lastTick = now;
    lv_timer_handler();
    SDL_Delay(16);
    t += 0.016f;
  }

  if (screenshot) {
    lv_obj_invalidate(lv_scr_act());
    lv_refr_now(lv_disp_get_default());

    if (viaSdl) {
      present();
      SDL_Surface* surface = SDL_CreateRGBSurfaceWithFormat(
          0, kWidth, kHeight, 32, SDL_PIXELFORMAT_ARGB8888);
      if (surface) {
        SDL_RenderReadPixels(gRenderer, nullptr, SDL_PIXELFORMAT_ARGB8888,
                             surface->pixels, surface->pitch);
        SDL_SaveBMP(surface, argv[1]);
        SDL_FreeSurface(surface);
      }
    } else {
      saveBmp(argv[1]);
    }
    return 0;
  }

  printf("VW T3 dashboard simulator v%s (ESP32-S3 emulation: 16-bit color, 6 MB LVGL heap, 30 FPS).\n", T3DASH_VERSION);
  printf("Esc exits, I toggles ignition, P toggles FPS/CPU/RAM overlay.\n");
  fflush(stdout);

  while (true) {
    const uint32_t frameStart = SDL_GetTicks();

    SDL_Event event;
    while (SDL_PollEvent(&event)) {
      if (event.type == SDL_QUIT) {
        return 0;
      }
      if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE) {
        return 0;
      }
      if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_i) {
        gIgnitionOn = !gIgnitionOn;
      }
      if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_p) {
        if (lv_obj_has_flag(lv_layer_sys(), LV_OBJ_FLAG_HIDDEN)) {
          lv_obj_clear_flag(lv_layer_sys(), LV_OBJ_FLAG_HIDDEN);
        } else {
          lv_obj_add_flag(lv_layer_sys(), LV_OBJ_FLAG_HIDDEN);
        }
      }
      if (event.type == SDL_MOUSEMOTION) {
        gMouseX = event.motion.x;
        gMouseY = event.motion.y;
      }
      if (event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_LEFT) {
        gMouseDown = true;
        gMouseX = event.button.x;
        gMouseY = event.button.y;
      }
      if (event.type == SDL_MOUSEBUTTONUP && event.button.button == SDL_BUTTON_LEFT) {
        gMouseDown = false;
        gMouseX = event.button.x;
        gMouseY = event.button.y;
      }
    }

    const uint64_t renderStart = SDL_GetPerformanceCounter();
    ui::update(demo::make(t, gIgnitionOn));

    const uint32_t now = SDL_GetTicks();
    lv_tick_inc(now - lastTick);
    lastTick = now;

    lv_timer_handler();
    if (gPerfFreq > 0) {
      const float ms = (float)(SDL_GetPerformanceCounter() - renderStart) *
                       1000.0f / (float)gPerfFreq;
      gRenderMs = gRenderMs * 0.9f + ms * 0.1f;
    }
    if (gDirty) {
      present();
      gDirty = false;
      ++gFrameCounter;
    }

    t += 0.033f;
    const uint32_t frameTime = SDL_GetTicks() - frameStart;
    if (frameTime < kFrameMs) {
      SDL_Delay(kFrameMs - frameTime);
    }
  }

  return 0;
}
