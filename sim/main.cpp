#include <SDL2/SDL.h>

#include <cstdint>
#include <cstdio>
#include <cstdlib>

#include "demo_values.h"
#include "lvgl.h"
#include "ui_lvgl.h"

namespace {

constexpr int kWidth = 800;
constexpr int kHeight = 480;

SDL_Window* gWindow = nullptr;
SDL_Renderer* gRenderer = nullptr;
SDL_Texture* gTexture = nullptr;
bool gDirty = false;

// Full-screen LVGL draw buffer; also used for BMP screenshots.
lv_color_t gFramebuffer[kWidth * kHeight];
lv_disp_draw_buf_t gDrawBuf;
lv_disp_drv_t gDispDrv;
lv_indev_drv_t gMouseDrv;

int gMouseX = 0;
int gMouseY = 0;
bool gMouseDown = false;
bool gIgnitionOn = true;

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

void flushCb(lv_disp_drv_t* drv, const lv_area_t* area, lv_color_t* color_p) {
  SDL_Rect rect;
  rect.x = area->x1;
  rect.y = area->y1;
  rect.w = area->x2 - area->x1 + 1;
  rect.h = area->y2 - area->y1 + 1;

  SDL_UpdateTexture(gTexture, &rect, color_p, kWidth * (int)sizeof(lv_color_t));
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
  fwrite(gFramebuffer, 1, (size_t)imageSize, file);
  fclose(file);
}

}  // namespace

int main(int argc, char** argv) {
  const bool screenshot = argc >= 2;
  const bool viaSdl = argc >= 3 && SDL_strcmp(argv[2], "sdl") == 0;
  const bool bootShot = argc >= 3 && SDL_strcmp(argv[2], "boot") == 0;
  const bool sweepShot = argc >= 3 && SDL_strcmp(argv[2], "sweep") == 0;
  const bool hotShot = argc >= 3 && SDL_strcmp(argv[2], "hot") == 0;
  const bool lowShot = argc >= 3 && SDL_strcmp(argv[2], "low") == 0;
  const bool videoShot = argc >= 3 && SDL_strcmp(argv[2], "video") == 0;
  const bool videoBrightShot = argc >= 3 && SDL_strcmp(argv[2], "video_bright") == 0;
  const bool videoAllShot = argc >= 3 && SDL_strcmp(argv[2], "video_all") == 0;
  const bool dimShot = argc >= 3 && SDL_strcmp(argv[2], "dim") == 0;
  const bool swipeShot = argc >= 3 && SDL_strcmp(argv[2], "swipe") == 0;
  const bool byeShot = argc >= 3 && SDL_strcmp(argv[2], "bye") == 0;
  const bool clockShot = argc >= 3 && SDL_strcmp(argv[2], "clock") == 0;
  const bool clocksetShot = argc >= 3 && SDL_strcmp(argv[2], "clockset") == 0;

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
  if (screenshot && !bootShot && !sweepShot && !hotShot && !videoShot &&
      !videoBrightShot && !videoAllShot) {
    ui::skipBoot();
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

  float t = lowShot ? 6.0f : 0.0f;
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

  printf("VW T3 dashboard simulator running. Esc exits, I toggles ignition.\n");
  fflush(stdout);

  while (true) {
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

    ui::update(demo::make(t, gIgnitionOn));

    const uint32_t now = SDL_GetTicks();
    lv_tick_inc(now - lastTick);
    lastTick = now;

    lv_timer_handler();
    if (gDirty) {
      present();
      gDirty = false;
    }

    SDL_Delay(5);
    t += 0.016f;
  }

  return 0;
}
