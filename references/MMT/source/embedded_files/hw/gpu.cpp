// https://zipcpu.com/blog/2017/06/21/looking-at-verilator.html

#include <SDL3/SDL.h>
#include <SDL3/SDL_hints.h>
#include <SDL3/SDL_main.h>
#include <SDL3/SDL_pixels.h>
#include <SDL3/SDL_render.h>
#include <cassert>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <verilated.h>

#include "Vgpu.h"

static inline void SDL_Die(const char *msg) {
  SDL_Log("%s (%s)\n", msg, SDL_GetError());
  exit(EXIT_FAILURE);
}

using u8 = uint8_t;
using u16 = uint16_t;
using u32 = uint32_t;
using u64 = uint64_t;

constexpr int VGA_H_ACTIVE = 640;
constexpr int VGA_H_BACK_PORCH = 48;
constexpr int VGA_H_SYNC = 96;
constexpr int VGA_H_FRONT_PORCH = 16;
// constexpr int VGA_H_ACTIVE = 1;
// constexpr int VGA_H_BACK_PORCH = 1;
// constexpr int VGA_H_SYNC = 1;
// constexpr int VGA_H_FRONT_PORCH = 1;
constexpr int VGA_H_TOTAL =
    VGA_H_ACTIVE + VGA_H_BACK_PORCH + VGA_H_SYNC + VGA_H_FRONT_PORCH;

constexpr int VGA_V_ACTIVE = 480;
constexpr int VGA_V_BACK_PORCH = 33;
constexpr int VGA_V_SYNC = 2;
constexpr int VGA_V_FRONT_PORCH = 10;
// constexpr int VGA_V_ACTIVE = 1;
// constexpr int VGA_V_BACK_PORCH = 1;
// constexpr int VGA_V_SYNC = 1;
// constexpr int VGA_V_FRONT_PORCH = 1;
constexpr int VGA_V_TOTAL =
    VGA_V_ACTIVE + VGA_V_BACK_PORCH + VGA_V_SYNC + VGA_H_FRONT_PORCH;

typedef struct {
  uint8_t b, g, r, a;
} Color;

typedef struct {
  u32 x : 9;
  u32 y : 8;
  u32 tile_ind : 10;
  u32 palette_ind : 3;
  u32 flip_h : 1;
  u32 flip_v : 1;
} Sprite;

typedef struct {
  u16 texture_ind : 10;
  u16 palette_ind : 3;
  u16 priority : 1;
  u16 flip_h : 1;
  u16 flip_v : 1;
} Tile;

typedef struct {
  u32 slivers[8];
} Texture;

typedef struct {
  uint16_t addr;
  uint32_t data;
  uint8_t be;
} WriteCmd;

template <class M> class Testbench {
protected:
  uint64_t ticks;
  M *module;

public:
  Testbench(void) {
    module = new M;
    ticks = 0l;
  }

  virtual ~Testbench(void) {
    module->final();
    delete module;
    module = NULL;
  }

  virtual void reset(void) {
    module->reset = 1;
    // Make sure any inheritance gets applied
    this->tick();
    module->reset = 0;
  }

  virtual void tick(void) {
    // Increment our own internal time reference
    ticks++;

    // Make sure any combinatorial logic depending upon
    // inputs that may have changed before we called tick()
    // has settled before the rising edge of the clock.
    module->clk = 0;
    module->eval();

    // Toggle the clock

    // Rising edge
    module->clk = 1;
    module->eval();

    // Falling edge
    module->clk = 0;
    module->eval();
  }

  virtual bool done(void) { return (Verilated::gotFinish()); }
};

constexpr u32 TEXTURE_ADDR_START = 0x0000;
constexpr u32 TILE_ADDR_START = 0x8000;
constexpr u32 PALETTE_ADDR_START = 0xF400;
constexpr u32 SPRITE_ADDR_START = 0xF000;

class GPUTestBench : public Testbench<Vgpu> {
  SDL_Window *window = nullptr;
  SDL_Renderer *renderer = nullptr;
  SDL_Texture *texture = nullptr;

  Color *screen_buffer = nullptr;

  // Negative edge == reset
  bool prev_hs = true, prev_vs = true;
  int h_count = 0, v_count = 0;
  uint64_t vga_ticks = 0;

public:
  GPUTestBench() : Testbench<Vgpu>() {
    if (!SDL_CreateWindowAndRenderer("GPU Verilator Testing", VGA_H_ACTIVE * 3,
                                     VGA_V_ACTIVE * 3, 0, &window, &renderer))
      SDL_Die("SDL_CreateWindowAndRenderer()");

    if ((texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888,
                                     SDL_TEXTUREACCESS_STATIC, VGA_H_ACTIVE,
                                     VGA_V_ACTIVE)) == nullptr)
      SDL_Die("SDL_CreateTexture()");

    screen_buffer = new Color[VGA_H_ACTIVE * VGA_V_ACTIVE];

    SDL_Log("Finished initializing SDL\n");
  }

  ~GPUTestBench() {

    delete screen_buffer;

    SDL_DestroyRenderer(renderer);
    renderer = nullptr;
    SDL_DestroyWindow(window);
    window = nullptr;
  }

  void iowrite32(u16 addr, u32 data) {
    module->address = addr >> 2;
    module->writedata = data;
    module->byteenable = 0x0F;
    module->chipselect = 1;
    module->write = 1;

    tick();

    module->chipselect = 0;
    module->write = 0;
  }
  void iowrite16(u16 addr, u16 data) {
    module->address = addr >> 2;
    module->writedata = ((u32)data) << ((addr & 0b10) * 8);
    module->byteenable = 0b0011 << (addr & 0b10);
    module->chipselect = 1;
    module->write = 1;

    tick();

    module->chipselect = 0;
    module->write = 0;
  }
  u16 ioread16(u16 addr) {
    module->address = addr >> 2;
    module->chipselect = 1;
    module->read = 1;

    tick();

    module->chipselect = 0;
    module->read = 0;

    return (addr & 0b10) ? module->readdata >> 16 : module->readdata & 0xFFFF;
  }
  void iowrite8(u16 addr, u8 data) {
    module->address = addr >> 2;
    module->writedata = ((u32)data) << ((addr & 0b11) * 8);
    module->byteenable = 0b0001 << (addr & 0b11);
    module->chipselect = 1;
    module->write = 1;

    tick();

    module->chipselect = 0;
    module->write = 0;
  }

  virtual void reset(void) {
    Testbench<Vgpu>::reset();
    h_count = 0;
    v_count = 0;
    prev_hs = true;
    prev_vs = true;
  }

  virtual void tick(void) {

    // Model ticking, based on the component's current state
    if (module->VGA_CLK) {
      // printf("%d %d\n", h_count, v_count);
      // Only render when not blanking!

      if (!module->VGA_VS && prev_vs)
        v_count = -(VGA_V_BACK_PORCH + VGA_V_SYNC);

      if (!module->VGA_HS && prev_hs) {
        h_count = -(VGA_H_BACK_PORCH + VGA_H_SYNC);
        v_count++;
      }
      h_count++;

      prev_hs = module->VGA_HS;
      prev_vs = module->VGA_VS;
    }

    // printf("\nTick %8ld (Reset = [%d], VGA_CLK = [%d])\n", ticks,
    // module->reset, module->VGA_CLK);

    // Assert that timings are correct

    // Tick the actual component
    Testbench<Vgpu>::tick();

    if (module->VGA_CLK) {
      if (module->VGA_BLANK_N) {
        assert(h_count >= 0);
        assert(h_count < VGA_H_ACTIVE);
        assert(v_count >= 0);
        assert(v_count < VGA_V_ACTIVE);

        screen_buffer[v_count * VGA_H_ACTIVE + h_count] = {
            module->VGA_B, module->VGA_G, module->VGA_R, 0xFF};
      } else {
        assert(module->VGA_R == 0);
        assert(module->VGA_G == 0);
        assert(module->VGA_B == 0);
      }

      if (!module->VGA_VS && prev_vs) {
        // Render once per frame, just for performance
        SDL_UpdateTexture(texture, NULL, screen_buffer,
                          VGA_H_ACTIVE * sizeof(Color));
        SDL_RenderClear(renderer);
        SDL_RenderTexture(renderer, texture, NULL, NULL);
        SDL_RenderPresent(renderer);
      }
    }
  }
};

int main(int argc, char **argv) {
  Verilated::commandArgs(argc, argv);

  if (!SDL_Init(SDL_INIT_VIDEO))
    SDL_Die("SDL_Init()");

  GPUTestBench *tb = new GPUTestBench();
  bool running = true;
  SDL_Event e;
  SDL_zero(e);

  std::cout << "Starting simulation...\n";

  tb->reset();

  // Move all the unused OAM sprites offscreen
  Sprite empty_sprite = {320, 240, 0, 0, 0, 0};
  for (u32 i = 0; i < 256; i++) {
    tb->iowrite32(SPRITE_ADDR_START + (i << 2), *(u32 *)&empty_sprite);
    tb->tick();
  }

  // Palette 0 will be various shades of gray
  for (u32 i = 0; i < 16; i++)
    tb->iowrite32(PALETTE_ADDR_START + (i << 2), 0x0F0F0F00 * i);

  // Palette 1 has purple at offset 1
  tb->iowrite32(PALETTE_ADDR_START + 64 + 4, 0xFF00FF00);

  // A gradient for texture 1
  for (u32 i = 0; i < 8; i++)
    tb->iowrite32(TEXTURE_ADDR_START + 32 + i * 4,
                  0x123456789ABCDEF >> (i * 4));

  // And just color 1 for texture 2
  for (u32 i = 0; i < 8; i++)
    tb->iowrite32(TEXTURE_ADDR_START + 64 + i * 4, 0x11111111);

  // for (u32 ind = 3; ind < 1024; ind++)
  //   for (u32 i = 0; i < 8; i++)
  //     tb->iowrite32(TEXTURE_ADDR_START + ind * 32 + i * 4,
  //                   0x123456789ABCDEF >> (i * 4));

  // Draw a checkerboard

  Tile tile = {};
  // printf("Printing tile\n");
  // tb->iowrite32(TILE_ADDR_START + 0x2000, 0xFFFFFFFF);
  // tb->iowrite32(TILE_ADDR_START + 0x2000, 0xFFFFFFFF);
  // tb->iowrite16(TILE_ADDR_START + 0x2000, *(u16 *)&tile);
  for (u32 y = 0; y < 64; y++) {
    for (u32 x = 0; x < 64; x++) {
      tile.texture_ind = ((x & 0b1) ^ (y & 0b1)) + 1;
      tile.palette_ind = (x & 0b1) ^ (y & 0b1);
      // printf("%d %d %d\n", y, x, tile.texture_ind);
      tb->iowrite16(TILE_ADDR_START + (y << 7) + (x << 1), *(u16 *)&tile);
      // tb->iowrite16(TILE_ADDR_START + (0x2000) + (y << 7) + (x << 1),
      //               *(u16 *)&tile);
    }
  }

  // Draw a sprite
  Sprite test_sprite = {20, 20, 1, 0, 0, 0};
  tb->iowrite32(SPRITE_ADDR_START + 255 * 4, *(u32 *)&test_sprite);

  // Finally, disable forced blanking
  tb->iowrite8(0xFF00, 0);

  u64 frame = 0;
  bool blank = false;

  for (u32 i = 0; !tb->done() && running; i++) {
    u16 v_count = tb->ioread16(0xFF0E);
    if (!blank && v_count == 0)
      blank = true;
    else if (blank && v_count == 480) {
      blank = false;
      frame++;
    }
    else if (!blank && v_count == 481) {
      tb->iowrite16(0xFF04, frame);
      tb->iowrite16(0xFF06, frame);
    }

    while (SDL_PollEvent(&e))
      if (e.type == SDL_EVENT_QUIT)
        running = false;
  }

  delete tb;

  SDL_Quit();

  return EXIT_SUCCESS;
}
