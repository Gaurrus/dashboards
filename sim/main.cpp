#include <SDL2/SDL.h>

#include <cstdint>
#include <cstdio>

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

  ui::init();

  float t = 0.0f;
  uint32_t lastTick = SDL_GetTicks();

  for (int frame = 0; frame < 4; ++frame) {
    ui::update(demo::make(t));
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

    const bool viaSdl = argc >= 3 && SDL_strcmp(argv[2], "sdl") == 0;
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

  printf("VW T3 dashboard simulator running. Close the window or press Esc to exit.\n");
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
    }

    ui::update(demo::make(t));

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
