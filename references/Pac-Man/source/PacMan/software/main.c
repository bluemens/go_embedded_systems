#include <stdio.h>
#include "vga_ball.h"
#include "usbkeyboard.h"
#include <sys/ioctl.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <libusb-1.0/libusb.h>

#define WIDTH 640
#define HEIGHT 480
#include <stdint.h>
#include <stdbool.h>

#include <string.h>
#include <libusb-1.0/libusb.h>
// #include "usbkeyboard.h" // 你需要有这个头文件

#include "controller.h"
// #include <libusb-1.0/libusb.h>


#define INPUT_BUFFER_SIZE 256
#define CHAT_COLS 64
#define INPUT_ROWS 2
#define MAX_KEYS 6

#define CTRL_START       (1 << 0)
#define CTRL_RESET       (1 << 1)
#define CTRL_PAUSE       (1 << 2)
#define CTRL_VBLANK_ACK  (1 << 3)
#define CTRL_GAME_OVER   (1 << 4)

#define PELLET_NONE 0xFFFF
uint16_t last_pellet_index = PELLET_NONE;

typedef struct {
    uint8_t x, y;
    uint8_t frame;
    uint8_t visible;
    uint8_t direction;
    uint8_t type_id;
    uint8_t rsv1, rsv2;
} sprite_t;

// typedef struct {
//     sprite_t sprites[5];
//     uint16_t score;
//     uint8_t control;
// } vga_all_state_t;

// === USB Keyboard相关 ===
struct usb_keyboard_packet packet;
struct libusb_device_handle *keyboard;
uint8_t endpoint_address;

// 键盘码转ASCII
char usb_to_ascii(uint8_t keycode, uint8_t modifiers) {
    static char ascii_map[256] = {0};
    ascii_map[0x04] = 'a'; ascii_map[0x05] = 'b'; ascii_map[0x06] = 'c';
    ascii_map[0x07] = 'd'; ascii_map[0x08] = 'e'; ascii_map[0x09] = 'f';
    ascii_map[0x0A] = 'g'; ascii_map[0x0B] = 'h'; ascii_map[0x0C] = 'i';
    ascii_map[0x0D] = 'j'; ascii_map[0x0E] = 'k'; ascii_map[0x0F] = 'l';
    ascii_map[0x10] = 'm'; ascii_map[0x11] = 'n'; ascii_map[0x12] = 'o';
    ascii_map[0x13] = 'p'; ascii_map[0x14] = 'q'; ascii_map[0x15] = 'r';
    ascii_map[0x16] = 's'; ascii_map[0x17] = 't'; ascii_map[0x18] = 'u';
    ascii_map[0x19] = 'v'; ascii_map[0x1A] = 'w'; ascii_map[0x1B] = 'x';
    ascii_map[0x1C] = 'y'; ascii_map[0x1D] = 'z';
    ascii_map[0x1E] = '1'; ascii_map[0x1F] = '2'; ascii_map[0x20] = '3';
    ascii_map[0x21] = '4'; ascii_map[0x22] = '5'; ascii_map[0x23] = '6';
    ascii_map[0x24] = '7'; ascii_map[0x25] = '8'; ascii_map[0x26] = '9';
    ascii_map[0x27] = '0';
    ascii_map[0x2C] = ' '; // 空格
    ascii_map[0x28] = '\n'; // Enter
    ascii_map[0x2A] = '\b'; // Backspace
    ascii_map[0x50] = 2;    // Left arrow
    ascii_map[0x4F] = 3;    // Right arrow
    ascii_map[0x52] = 4;    // Up arrow
    ascii_map[0x51] = 5;    // Down arrow

    if (modifiers & USB_LSHIFT || modifiers & USB_RSHIFT) {
        if (keycode >= 0x04 && keycode <= 0x1D) {
            return ascii_map[keycode] - 32; // 大写
        }
    }
    return ascii_map[keycode];
}

// 接收方向键控制
void handle_input(char c) {
    extern uint8_t pacman_dir;
    if (c == 2) { // left
        pacman_dir = 1;
    } else if (c == 3) { // right
        pacman_dir = 3;
    } else if (c == 4) { // up
        pacman_dir = 0;
    } else if (c == 5) { // down
        pacman_dir = 2;
    }
}

// === Pac-Man 游戏逻辑 ===
// typedef struct {
//     uint8_t x, y;
//     uint8_t frame;
//     uint8_t visible;
//     uint8_t direction;
//     uint8_t type_id;
//     uint8_t rsv1, rsv2;
// } sprite_t;

typedef struct {
    int x, y;
    uint8_t dir;
    sprite_t* sprite;
} ghost_t;

#define TILE_WIDTH 8
#define TILE_HEIGHT 8
#define SCREEN_WIDTH_TILES 28
#define SCREEN_HEIGHT_TILES 33
#define NUM_GHOSTS 4
#define SPRITE_PACMAN 0
#define SPRITE_GHOST_0 1
#define SPRITE_GHOST_1 2
#define SPRITE_GHOST_2 3
#define SPRITE_GHOST_3 4


// 模拟RAM
uint8_t fake_tilemap[40 * 30];
uint32_t fake_pellet_ram[30];
sprite_t fake_sprites[5];
uint16_t fake_score;
uint8_t fake_control = 1;

#define TILEMAP_BASE     (fake_tilemap)
#define PELLET_RAM_BASE  (fake_pellet_ram)
#define SPRITE_BASE      ((uint8_t*) fake_sprites)
#define SCORE_REG        (&fake_score)
#define CONTROL_REG      (&fake_control)

ghost_t ghosts[NUM_GHOSTS];
sprite_t* sprites = (sprite_t*) SPRITE_BASE;
#define PACMAN_INIT_X 1 * TILE_WIDTH + TILE_WIDTH / 2
#define PACMAN_INIT_Y 1 * TILE_HEIGHT + TILE_HEIGHT / 2
int pacman_x = PACMAN_INIT_X;
int pacman_y = PACMAN_INIT_Y;
uint8_t pacman_dir = 1; // 初始向左
uint16_t score = 0;


void set_control_flag(uint8_t flag) {
    *CONTROL_REG |= flag;
}

void clear_control_flag(uint8_t flag) {
    *CONTROL_REG &= ~flag;
}
void clear_all_control_flags() {
    *CONTROL_REG = 0;
}

bool is_control_flag_set(uint8_t flag) {
    return (*CONTROL_REG & flag) != 0;
}

uint8_t direction_to_control(uint8_t dir) {
    switch (dir) {
        case 0: return 0; // up
        case 1: return 3; // left
        case 2: return 2; // down
        case 3: return 1; // right
        case 5: return 4; // eat pellet
        default: return 0;
    }
}
int vga_ball_fd;

void update_all_to_driver() {
    vga_all_state_t state;

    // 填入所有 sprite 信息
    for (int i = 0; i < 5; i++) {
        // state.sprites[i] = fake_sprites[i];
        state.sprites[i].x = sprites[i].x ;
        state.sprites[i].y = sprites[i].y ;
        state.sprites[i].frame = sprites[i].frame;
        state.sprites[i].visible = sprites[i].visible;
        state.sprites[i].direction = direction_to_control(sprites[i].direction);
        state.sprites[i].type_id = sprites[i].type_id;
        state.sprites[i].reserved1 = sprites[i].rsv1;
        state.sprites[i].reserved2 = sprites[i].rsv2;
    }

    state.score = * SCORE_REG;
    state.control = *CONTROL_REG;

    // state.pellet_to_eat = last_pellet_index;

    if (last_pellet_index != PELLET_NONE) {
        int pellet_x = last_pellet_index % SCREEN_WIDTH_TILES;
        int pellet_y = last_pellet_index / SCREEN_WIDTH_TILES;
        state.pellet_to_eat = pellet_y * 80 + pellet_x + 6 + 1220;
    } else {
        state.pellet_to_eat = 0xFFFF; // 无效值
    }

    last_pellet_index = PELLET_NONE;

    if (ioctl(vga_ball_fd, VGA_BALL_WRITE_ALL, &state)) {
        perror("ioctl(VGA_BALL_WRITE_ALL) failed");
    }



    // printf all content for debugging
    printf("=== State Debug ===\n");
    printf("Score: %d\n", state.score);
    printf("Control: 0x%02X\n", state.control);
    for (int i = 0; i < 5; i++) {
        printf("Sprite %d: x=%d, y=%d, frame=%d, visible=%d, direction=%d, type_id=%d\n",
               i,
               state.sprites[i].x,
               state.sprites[i].y,
               state.sprites[i].frame,
               state.sprites[i].visible,
               state.sprites[i].direction,
               state.sprites[i].type_id);
    }
    printf("Pellet to eat: %d\n", state.pellet_to_eat);
    printf("====================\n");
}

// 工具函数
void set_tile(int x, int y, uint8_t tile_id) {
    TILEMAP_BASE[y * SCREEN_WIDTH_TILES + x] = tile_id;
}

uint8_t get_pellet_bit(int x, int y) {
    return (PELLET_RAM_BASE[y] >> (31 - x)) & 1;
}

void clear_pellet_bit(int x, int y) {
    PELLET_RAM_BASE[y] &= ~(1 << (31 - x));
}

bool can_move_to(int px, int py) {
    int tx = px / TILE_WIDTH;
    int ty = py / TILE_HEIGHT;
    // if (tx < 0 || tx >= 40 || ty < 0 || ty >= 30) return false;
    uint8_t tile = TILEMAP_BASE[ty * SCREEN_WIDTH_TILES + tx];
    if (tile == 0x40 || tile == 1 || tile == 0) {
        
    }
    else{
        printf("Tile at (%d, %d) is not walkable: %d\n", tx, ty, tile);
    }
    return (tile == 0x40 || tile == 1 || tile == 0 || tile == 0xFD);
}
const int step_size = TILE_HEIGHT ;

uint16_t generate_packed_score(uint16_t score) {
    int s = score % 10000;  // 保证最多4位
    return ((s / 1000) << 12) |
           (((s / 100) % 10) << 8) |
           (((s / 10) % 10) << 4) |
           (s % 10);
}

void update_pacman() {
    int new_x = pacman_x;
    int new_y = pacman_y;
    switch (pacman_dir) {
        case 0: new_y -= step_size ; break;
        case 1: new_x -= step_size ; break;
        case 2: new_y += step_size ; break;
        case 3: new_x += step_size ; break;
    }

    if (can_move_to(new_x, new_y)) {
        pacman_x = new_x;
        pacman_y = new_y;
    }

    int tile_x = pacman_x / TILE_WIDTH;
    int tile_y = pacman_y / TILE_HEIGHT;
    // if (get_pellet_bit(tile_x, tile_y)) {
    //     clear_pellet_bit(tile_x, tile_y);
    //     set_tile(tile_x, tile_y, 0);
    //     score += 10;
    //     *SCORE_REG = generate_packed_score(score);
    //     printf("[Pac-Man] Ate pellet at (%d, %d). Score = %d\n", tile_x, tile_y, score);
    // }
    if (get_pellet_bit(tile_x, tile_y)) {
        clear_pellet_bit(tile_x, tile_y);
        set_tile(tile_x, tile_y, 0);
        pacman_dir = 5; // Eat pellet animation direction
        score += 10;
        *SCORE_REG = generate_packed_score(score);
        last_pellet_index = tile_y * SCREEN_WIDTH_TILES + tile_x;
        printf("[Pac-Man] Ate pellet at (%d, %d). Score = %d\n", tile_x, tile_y, score);
    } else {
        last_pellet_index = PELLET_NONE;
    }
    sprite_t* pac = &sprites[SPRITE_PACMAN];


    // TP
    if (pacman_x <= 2 * TILE_WIDTH && pacman_y == 14 * TILE_HEIGHT + TILE_HEIGHT / 2){
        pacman_x = 25 * TILE_WIDTH + TILE_WIDTH / 2;
        printf("Teleport to (%d, %d)\n", pacman_x, pacman_y);
    }
    else if (pacman_x >= 26 * TILE_WIDTH && pacman_y == 14 * TILE_HEIGHT + TILE_HEIGHT / 2){
        pacman_x = 2 * TILE_WIDTH + TILE_WIDTH / 2;
        printf("Teleport to (%d, %d)\n", pacman_x, pacman_y);
    }

    pac->x = pacman_x;
    pac->y = pacman_y;
    pac->visible = 1;
    pac->direction = pacman_dir;
    pac->frame = 0;
}

bool is_ghost_tile(int x, int y) {
    for (int i = 0; i < NUM_GHOSTS; i++) {
        if (ghosts[i].x / TILE_WIDTH == x && ghosts[i].y / TILE_HEIGHT == y) return true;
    }
    return false;
}

void print_tilemap() {
    printf("=== Tilemap View ===\n");
    for (int y = 0; y <= 30; y++) {
        for (int x = 0; x < 28; x++) {
            uint8_t tile = TILEMAP_BASE[y * SCREEN_WIDTH_TILES + x];
            if (pacman_x / TILE_WIDTH == x && pacman_y / TILE_HEIGHT == y) {
                putchar('P');
            } else if (is_ghost_tile(x, y)) {
                putchar('G');
            } else if (tile == 0 || tile == 0x40) {
                putchar(' ');
            } else if (tile == 1) {
                putchar('.');
            } else {
                putchar('#');
            }
        }
        putchar('\n');
    }
    printf("====================\n");
}



bool is_tile_occupied_by_other_ghost(int gx, int gy, int self_index) {
    for (int i = 0; i < NUM_GHOSTS; i++) {
        if (i == self_index) continue; // 不检查自己
        int other_tx = ghosts[i].x / TILE_WIDTH;
        int other_ty = ghosts[i].y / TILE_HEIGHT;
        if (other_tx == gx && other_ty == gy) {
            return true;
        }
    }
    return false;
}
#include <stdlib.h>  // for rand()

#define EPSILON 0.2f  // 20% 概率做随机动作

// 计算 Manhattan 距离
int manhattan_distance(int x1, int y1, int x2, int y2) {
    return abs(x1 - x2) + abs(y1 - y2);
}

// 返回一个随机方向 [0, 1, 2, 3]
int random_direction() {
    return rand() % 4;
}

void update_ghosts() {
    static int ghost_tick = 0;
    ghost_tick++;
    if (ghost_tick % 10 != 0) return;

    for (int i = 0; i < NUM_GHOSTS; i++) {
        ghost_t* g = &ghosts[i];
        int best_dir = g->dir;
        int best_dist = 1 << 30; // 初始为超大值

        // 当前 tile
        int curr_tx = g->x / TILE_WIDTH;
        int curr_ty = g->y / TILE_HEIGHT;

        // 目标 tile计算（根据不同策略）
        int target_x = pacman_x, target_y = pacman_y;

        if (i == 0) {
            // Blinky: 追踪 Pac-Man
            target_x = pacman_x;
            target_y = pacman_y;
        } else if (i == 1) {
            // Pinky: 预测 Pac-Man 前方 4 格
            target_x = pacman_x;
            target_y = pacman_y;
            switch (pacman_dir) {
                case 0: target_y -= TILE_HEIGHT * 4; break;
                case 1: target_x -= TILE_WIDTH * 4; break;
                case 2: target_y += TILE_HEIGHT * 4; break;
                case 3: target_x += TILE_WIDTH * 4; break;
            }
        } else if (i == 2) {
            // Inky: 使用 Blinky 和 Pac-Man 构建向量，做镜像追踪
            int bx = ghosts[0].x;
            int by = ghosts[0].y;
            int ref_x = pacman_x, ref_y = pacman_y;
            switch (pacman_dir) {
                case 0: ref_y -= TILE_HEIGHT * 2; break;
                case 1: ref_x -= TILE_WIDTH * 2; break;
                case 2: ref_y += TILE_HEIGHT * 2; break;
                case 3: ref_x += TILE_WIDTH * 2; break;
            }
            target_x = ref_x + (ref_x - bx);
            target_y = ref_y + (ref_y - by);
        } else if (i == 3) {
            // Clyde: 如果离 Pac-Man 太近则逃跑
            int d = abs(g->x - pacman_x) + abs(g->y - pacman_y);
            if (d < TILE_WIDTH * 8) {
                target_x = 0;
                target_y = 0; // 逃到角落
            }
        }

        // 搜索四个方向，选择最优方向（含ε随机性）
        for (int d = 0; d < 4; d++) {
            int dx = 0, dy = 0;
            switch (d) {
                case 0: dy = -step_size; break;
                case 1: dx = -step_size; break;
                case 2: dy = +step_size; break;
                case 3: dx = +step_size; break;
            }

            int new_x = g->x + dx;
            int new_y = g->y + dy;
            int tx = new_x / TILE_WIDTH;
            int ty = new_y / TILE_HEIGHT;

            if (!can_move_to(new_x, new_y) || is_tile_occupied_by_other_ghost(tx, ty, i)) continue;

            int dist = abs(new_x - target_x) + abs(new_y - target_y);

            // ε-greedy 探索: 有一定概率选择非最优路径
            if (rand() % 100 < 20) { // 20% 概率随机选择合法方向
                best_dir = d;
                break;
            }

            if (dist < best_dist) {
                best_dist = dist;
                best_dir = d;
            }
        }

        // 更新方向和位置
        g->dir = best_dir;
        switch (best_dir) {
            case 0: g->y -= step_size; break;
            case 1: g->x -= step_size; break;
            case 2: g->y += step_size; break;
            case 3: g->x += step_size; break;
        }

        g->sprite->x = g->x;
        g->sprite->y = g->y;
    }
}
// void update_ghosts() {
//     static int ghost_tick = 0;
//     ghost_tick++;
//     if (ghost_tick % 10 != 0) return;

//     for (int i = 0; i < NUM_GHOSTS; i++) {
//         ghost_t* g = &ghosts[i];
//         int new_x = g->x;
//         int new_y = g->y;

//         // 计算目标 tile
//         int new_tx = g->x / TILE_WIDTH;
//         int new_ty = g->y / TILE_HEIGHT;

//         switch (g->dir) {
//             case 0: new_y -= step_size; new_ty--; break;
//             case 1: new_x -= step_size;  new_tx--; break;
//             case 2: new_y += step_size; new_ty++; break;
//             case 3: new_x += step_size;  new_tx++; break;
//         }

//         // 判断是否能移动且目标 tile 未被其他幽灵占用
//         if (can_move_to(new_x, new_y) &&
//             !is_tile_occupied_by_other_ghost(new_tx, new_ty, i)) {
//             g->x = new_x;
//             g->y = new_y;
//         } else {
//             // 不能前进就随机换一个方向（避开死循环）
//             g->dir = (g->dir + 1) % 4;
//         }

//         // 更新 sprite 位置
//         g->sprite->x = g->x;
//         g->sprite->y = g->y;
//     }
// }

void game_init_playfield(void) {
    static const char* tiles =
       //0123456789012345678901234567
        "0UUUUUUUUUUUU45UUUUUUUUUUUU1" // 3
        "L............rl............R" // 4
        "L.ebbf.ebbbf.rl.ebbbf.ebbf.R" // 5
        "L.r  l.r   l.rl.r   l.r  l.R" // 6
        "L.guuh.guuuh.gh.guuuh.guuh.R" // 7
        "L..........................R" // 8
        "L.ebbf.ef.ebbbbbbf.ef.ebbf.R" // 9
        "L.guuh.rl.guuyxuuh.rl.guuh.R" // 10
        "L......rl....rl....rl......R" // 11
        "2BBBBf.rzbbf.rl.ebbwl.eBBBB3" // 12
        "     L.rxuuh.gh.guuyl.R     " // 13
        "     L.rl..........rl.R     " // 14
        "     L.rl.mjs  tjn.rl.R     " // 15
        "UUUUUh.gh.i      q.gh.gUUUUU" // 16
        "      .   i      q   .      " // 17
        "BBBBBf.ef.i      q.ef.eBBBBB" // 18
        "     L.rl.okkkkkkp.rl.R     " // 19
        "     L.rl.        .rl.R     " // 20
        "     L.rl.ebbbbbbf.rl.R     " // 21
        "0UUUUh.gh.guuyxuuh.gh.gUUUU1" // 22
        "L............rl............R" // 23
        "L.ebbf.ebbbf.rl.ebbbf.ebbf.R" // 24
        "L.guyl.guuuh.gh.guuuh.rxuh.R" // 25
        "L...rl................rl...R" // 26
        "6bf.rl.ef.ebbbbbbf.ef.rl.eb8" // 27
        "7uh.gh.rl.guuyxuuh.rl.gh.gu9" // 28
        "L......rl....rl....rl......R" // 29
        "L.ebbbbwzbbf.rl.ebbwzbbbbf.R" // 30
        "L.guuuuuuuuh.gh.guuuuuuuuh.R" // 31
        "L..........................R" // 32
        "2BBBBBBBBBBBBBBBBBBBBBBBBBB3"; // 33
       //0123456789012345678901234567

    uint8_t t[128];
    for (int i = 0; i < 128; i++) t[i] = 1;
    t[' '] = 0x40; t['0'] = 0xD1; t['1'] = 0xD0; t['2'] = 0xD5; t['3'] = 0xD4;
    t['4'] = 0xFB; t['5'] = 0xFA; t['6'] = 0xD7; t['7'] = 0xD9;
    t['8'] = 0xD6; t['9'] = 0xD8; t['U'] = 0xDB; t['L'] = 0xD3;
    t['R'] = 0xD2; t['B'] = 0xDC; t['b'] = 0xDF; t['e'] = 0xE7;
    t['f'] = 0xE6; t['g'] = 0xEB; t['h'] = 0xEA; t['l'] = 0xE8;
    t['r'] = 0xE9; t['u'] = 0xE5; t['w'] = 0xF5; t['x'] = 0xF2;
    t['y'] = 0xF3; t['z'] = 0xF4; t['m'] = 0xED; t['n'] = 0xEC;
    t['o'] = 0xEF; t['p'] = 0xEE; t['j'] = 0xDD; t['i'] = 0xD2;
    t['k'] = 0xDB; t['q'] = 0xD3; t['s'] = 0xF1; t['t'] = 0xF0;
    t['-'] = 0xFE; t['P'] = 0xFD;

    for (int y = 0, i = 0; y <= 30; y++) {
        for (int x = 0; x < 28; x++, i++) {
            set_tile(x, y, t[tiles[i] & 127]);
            if (tiles[i] == '.' || tiles[i] == 'P') {
                PELLET_RAM_BASE[y] |= (1 << (31 - x));
            }
        }
    }
}
void init_ghosts() {
    const int start_positions[4][2] = {
        {13, 14}, {14, 14}, {13, 15}, {14, 15}
        
    };
    for (int i = 0; i < NUM_GHOSTS; i++) {
        ghosts[i].x = start_positions[i][0] * TILE_WIDTH + TILE_WIDTH / 2;
        ghosts[i].y = start_positions[i][1] * TILE_HEIGHT + TILE_HEIGHT / 2;
        ghosts[i].dir = i % 4;
        ghosts[i].sprite = &sprites[SPRITE_GHOST_0 + i];
        ghost_t* g = &ghosts[i];
        g->sprite->x = g->x;
        g->sprite->y = g->y;
        g->sprite->visible = 1;
        g->sprite->frame = 0;
    }
}

// void wait_for_start_signal() {
//     printf("Waiting for START signal...\n");
//     int transferred;
//     while (1) {
//         // update_all_to_driver();
//         int r = libusb_interrupt_transfer(keyboard, endpoint_address,
//                                           (unsigned char *)&packet, sizeof(packet),
//                                           &transferred, 1);
//         if (r == 0 && transferred == sizeof(packet)) {
//             for (int i = 0; i < MAX_KEYS; i++) {
//                 uint8_t key = packet.keycode[i];
//                 if (key != 0) {
//                     char c = usb_to_ascii(key, packet.modifiers);
//                     if (c == '\n') {
//                         // 收到回车键，表示开始游戏
//                         set_control_flag(CTRL_START);
//                         printf("START signal received. Game starting...\n");
//                         return;
//                     }
//                 }
//             }
//         }
//         usleep(10000); // 等待一段时间再检查
//     }

//     printf("Game started!\n");
// }
bool paused = false;

void process_control_keys(char c) {
    if (c == ' ') {  // 空格键切换暂停
        paused = !paused;
        if (paused) {
            set_control_flag(CTRL_PAUSE);
            printf("Game paused.\n");
        } else {
            clear_control_flag(CTRL_PAUSE);
            printf("Game resumed.\n");
        }
    } else if (c == 'r' || c == 'R') {
        set_control_flag(CTRL_RESET);
        printf("Game reset signal sent.\n");
        // 可以选择立刻退出 game_loop()，回到 main 中重新初始化
        exit(0);
    }
}
bool check_collision() {
    for (int i = 0; i < NUM_GHOSTS; i++) {
        if ((ghosts[i].x / TILE_WIDTH == pacman_x / TILE_WIDTH) &&
            (ghosts[i].y / TILE_HEIGHT == pacman_y / TILE_HEIGHT)) {
            return true;
        }
    }
    return false;
}

void game_init() {
    *SCORE_REG = 0;
    *CONTROL_REG = 0;
    // fake_control = CTRL_START;
    game_init_playfield();
    init_ghosts();
    sprite_t* pac = &sprites[SPRITE_PACMAN];
    pacman_x = PACMAN_INIT_X;
    pacman_y = PACMAN_INIT_Y;
    pac->x = pacman_x;
    pac->y = pacman_y;
    pac->visible = 1;
    pac->frame = 0;
    clear_all_control_flags();
    update_all_to_driver();
    
}

// void game_loop() {
//     wait_for_start_signal();
//     printf("Game loop started. Press ESC to exit.\n");

//     int transferred;
//     while (1) {
//         int r = libusb_interrupt_transfer(keyboard, endpoint_address,
//                                           (unsigned char *)&packet, sizeof(packet),
//                                           &transferred, 1);

//         if (r == 0 && transferred == sizeof(packet)) {
//             for (int i = 0; i < MAX_KEYS; i++) {
//                 uint8_t key = packet.keycode[i];
//                 if (key != 0) {
//                     char c = usb_to_ascii(key, packet.modifiers);
//                     handle_input(c);
//                     process_control_keys(c);
//                 }
//             }
//             if (packet.keycode[0] == 0x29) { // ESC退出
//                 printf("ESC pressed. Exiting.\n");
//                 break;
//             }
//         }

//         if (paused || is_control_flag_set(CTRL_PAUSE)) {
//             usleep(100000);
//             continue;
//         }

//         update_pacman();
//         update_ghosts();

//         if (check_collision()) {
//             set_control_flag(CTRL_GAME_OVER);
//             printf("Game Over! Pac-Man was caught by a ghost.\n");
//             break;
//         }

//         update_all_to_driver(); 
//         print_tilemap();
//         usleep(100000);
//     }
// }
int abs(int value) {
    return value < 0 ? -value : value;
}
bool check_gameover() {
    for (int i = 0; i < NUM_GHOSTS; i++) {
        int dx = abs(pacman_x - ghosts[i].x);
        int dy = abs(pacman_y - ghosts[i].y);
        if (dx < TILE_WIDTH && dy < TILE_HEIGHT) {
            return true;
        }
    }
    //检查score
    if (score >= 2680 + 60) {
        printf("Game Over! You win!\n");
        return true;
    }
    return false;
}
// void game_loop() {
//     printf("Waiting for START signal...\n");
//     wait_for_start_signal();

//     printf("Game loop started. Press ESC to exit.\n");
//     int transferred;

//     while (1) {
//         update_all_to_driver();
//         int r = libusb_interrupt_transfer(keyboard, endpoint_address,
//                                           (unsigned char *)&packet, sizeof(packet),
//                                           &transferred, 1);
//         if (r == 0 && transferred == sizeof(packet)) {
//             for (int i = 0; i < MAX_KEYS; i++) {
//                 uint8_t key = packet.keycode[i];
//                 if (key != 0) {
//                     char c = usb_to_ascii(key, packet.modifiers);
//                     if (c == '\x1b') {
//                         printf("ESC pressed. Exiting.\n");
//                         *CONTROL_REG |= CTRL_GAME_OVER;
//                         return;
//                     } else if (c == ' ') {
//                         *CONTROL_REG ^= CTRL_PAUSE;
//                     } else if (c == 'r' || c == 'R') {
//                         *CONTROL_REG |= CTRL_RESET;
//                         return;
//                     } else {
//                         handle_input(c);
//                     }
//                 }
//             }
//         }

//         if (*CONTROL_REG & CTRL_PAUSE) {
//             usleep(10000);
//             continue;
//         }

//         update_pacman();
//         update_ghosts();

//         print_tilemap();

//         // 检查 Game Over 条件
//         if (check_gameover()) {
//             *CONTROL_REG |= CTRL_GAME_OVER;
//             printf("[Game] Game Over! Press 'r' to restart...\n");
//         }

//         update_all_to_driver();
//         usleep(100000);

//         // 如果 Game Over，暂停游戏，直到 reset
//         if (*CONTROL_REG & CTRL_GAME_OVER) {
//             while (!(*CONTROL_REG & CTRL_RESET)) {
//                 int r = libusb_interrupt_transfer(keyboard, endpoint_address,
//                                                   (unsigned char *)&packet, sizeof(packet),
//                                                   &transferred, 1);
//                 if (r == 0 && transferred == sizeof(packet)) {
//                     for (int i = 0; i < MAX_KEYS; i++) {
//                         uint8_t key = packet.keycode[i];
//                         if (key != 0) {
//                             char c = usb_to_ascii(key, packet.modifiers);
//                             if (c == 'r' || c == 'R') {
//                                 *CONTROL_REG |= CTRL_RESET;
//                             }
//                         }
//                     }
//                 }
//                 usleep(100000);
//             }
//             return;  // reset 被设置，退出 game_loop()，由 main() 重启游戏
//         }
//     }
// }


// void update_pacman_position(unsigned short x, unsigned short y, unsigned short old_x, unsigned short old_y) {
//     vga_ball_arg_t vla;
//     vla.pacman_x = x;
//     vla.pacman_y = y;
//     vla.old_pacman_x = old_x;
//     vla.old_pacman_y = old_y;

//     if (ioctl(vga_ball_fd, VGA_BALL_WRITE_PACMAN_POS, &vla)) {
//         perror("ioctl(VGA_BALL_WRITE_PACMAN_POS) failed");
//     }
// }

// int main() {
    

//     if ((keyboard = openkeyboard(&endpoint_address)) == NULL) {
//         fprintf(stderr, "Cannot find USB keyboard.\n");
//         return 1;
//     }
//     printf("USB keyboard found, start playing...\n");


//     // unsigned short x = 320, y = 240;
//     // unsigned short old_x = x, old_y = y;

//     uint8_t endpoint_address;
//     struct libusb_device_handle *keyboard;
//     struct usb_keyboard_packet packet;

//     vga_ball_fd = open("/dev/vga_ball", O_RDWR);
//     if (vga_ball_fd == -1) {
//         perror("Failed to open /dev/vga_ball");
//         return 1;
//     }

//     // keyboard = openkeyboard(&endpoint_address);
//     // if (!keyboard) {
//     //     fprintf(stderr, "Could not find a keyboard\n");
//     //     return 1;
//     // }

//     // printf("Pac-Man USB keyboard control started\n");
//     while(1)
//     {
//         game_init();
//         printf("Starting game loop...\n");
//         game_loop();
        
    
//     }

//     libusb_close(keyboard);
//     libusb_exit(NULL);
//     close(vga_ball_fd);
//     return 0;
// }

int main() {
    struct controller_list controller = open_controller();
    uint8_t endpoint_address = controller.device1_addr;
    struct libusb_device_handle* device = controller.device1;
    struct controller_pkt packet;

    vga_ball_fd = open("/dev/vga_ball", O_RDWR);
    if (vga_ball_fd == -1) {
        perror("Failed to open /dev/vga_ball");
        return 1;
    }

    printf("Controller connected. Starting Pac-Man...\n");

    while (1) {
        game_init();
        printf("Waiting for START (A button)...\n");

        int transferred;
        while (!(is_control_flag_set(CTRL_START))) {
            if (libusb_interrupt_transfer(device, endpoint_address,
                                          (unsigned char*)&packet, sizeof(packet),
                                          &transferred, 0) == 0 && transferred == 7) {
                if (packet.ab & 0x20) {  // A button
                    set_control_flag(CTRL_START);
                    break;
                }
            }
            usleep(10000);
        }

        printf("Game started. Use controller to play.\n");

        while (1) {
            if (libusb_interrupt_transfer(device, endpoint_address,
                                          (unsigned char*)&packet, sizeof(packet),
                                          &transferred, 0) == 0 && transferred == 7) {

                if (packet.dir_x == 0x00) pacman_dir = 1; // left
                if (packet.dir_x == 0xff) pacman_dir = 3; // right
                if (packet.dir_y == 0x00) pacman_dir = 0; // up
                if (packet.dir_y == 0xff) pacman_dir = 2; // down

                if (packet.ab & 0x10) {
                    *CONTROL_REG ^= CTRL_PAUSE;
                    usleep(200000); // debounce
                }
            }

            if (*CONTROL_REG & CTRL_PAUSE) {
                usleep(100000);
                continue;
            }

            update_pacman();
            update_ghosts();

            if (check_gameover()) {
                *CONTROL_REG |= CTRL_GAME_OVER;
                update_all_to_driver();
                printf("[Game] Game Over! Press A to restart.\n");
                break;
            }

            update_all_to_driver();
            usleep(100000);
        }

        while (!(*CONTROL_REG & CTRL_RESET)) {
            if (libusb_interrupt_transfer(device, endpoint_address,
                                          (unsigned char*)&packet, sizeof(packet),
                                          &transferred, 0) == 0 && transferred == 7) {
                if (packet.ab & 0x20) { // A again = restart
                    *CONTROL_REG |= CTRL_RESET;
                }
            }
            usleep(100000);
        }
    }

    libusb_close(device);
    libusb_exit(NULL);
    close(vga_ball_fd);
    return 0;
}