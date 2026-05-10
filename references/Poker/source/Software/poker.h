/*
 * Poker utils for tile drawing logic
 * 
 *
 * Timothy Melendez
 * Columbia University
 */

#include <stdlib.h>
#include <stdio.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>
#include "vga_poker.h"

/* Defined dynamic tile rows and cols to update */
#define TARGET_SCORE_ROW 6
#define ROUND_SCORE_ROW 12
#define SCORE_DIGITS 6
#define SCORE_COL_START 6
#define SCORE_COL_END 11

#define HAND_TYPE_ROW 16
#define HAND_TYPE_COL_START 2
#define HAND_TYPE_COL_END 16

#define CHIP_MULT_CALC_ROW 20
#define CHIP_COL_START 3
#define CHIP_COL_END 6
#define MULT_COL_START 10
#define MULT_COL_END 13

#define HANDS_LEFT_ROW 26
#define HANDS_LEFT_COL 9

#define DISCARD_ROW 32
#define DISCARD_COL 9

#define CARDS_IN_DECK_ROW 38
#define CARDS_IN_DECK_COL 8 /* ends at 9*/

#define ANTE_ROUND_ROW 44
#define ANTE_COL 3
#define ROUND_COL 11

#define JOKER_NAME_ROW 49
#define JOKER_INFO_ROW_START 51
#define JOKER_INFO_ROW_END 55
#define JOKER_NAME_COL_START 2
#define JOKER_NAME_COL_END 15

#define JOKER_ROW 2
#define JOKER_1_COL 23
#define JOKER_2_COL 34
#define JOKER_3_COL 45
#define JOKER_4_COL 56
#define JOKER_5_COL 67

#define CURSOR_ROW 57
#define CURSOR_COL 27
#define SELECTED_ROW 40
#define SELECTED_COL 27

#define PLAYED_ROW 22
#define PLAYED_1_COL 23
#define PLAYED_2_COL 34
#define PLAYED_3_COL 45
#define PLAYED_4_COL 56
#define PLAYED_5_COL 67

#define HAND_ROW_START 42
#define HAND_ROW_END 55
#define HAND_1_COL_START 27
#define HAND_1_COL_END 31
#define HAND_2_COL_START 32
#define HAND_2_COL_END 36
#define HAND_3_COL_START 37
#define HAND_3_COL_END 41
#define HAND_4_COL_START 42
#define HAND_4_COL_END 46
#define HAND_5_COL_START 47
#define HAND_5_COL_END 51
#define HAND_6_COL_START 52
#define HAND_6_COL_END 56
#define HAND_7_COL_START 57
#define HAND_7_COL_END 61
#define HAND_8_COL_START 62
#define HAND_8_COL_END 71

#define CARD_TILE_ID_ROWS 14
#define CARD_TILE_ID_COLS 10

int vga_poker_fd;
/* row col */
int tile_map[60][80];
/* C, D, S H*/
uint8_t deck[52][CARD_TILE_ID_ROWS][CARD_TILE_ID_COLS];
uint8_t jokers[10][CARD_TILE_ID_ROWS][CARD_TILE_ID_COLS];
uint8_t card[CARD_TILE_ID_ROWS][CARD_TILE_ID_COLS];

uint8_t card_slot[CARD_TILE_ID_ROWS][CARD_TILE_ID_COLS] = {
  { 0xEE, 0xF8, 0xF8, 0xF8, 0xF8, 0xF8, 0xF8, 0xF8, 0xF8, 0xEF },
  { 0xF9, 0x5F, 0x5F, 0x5F, 0x5F, 0x5F, 0x5F, 0x5F, 0x5F, 0xFA },
  { 0xF9, 0x5F, 0x5F, 0x5F, 0x5F, 0x5F, 0x5F, 0x5F, 0x5F, 0xFA },
  { 0xF9, 0x5F, 0x5F, 0x5F, 0x5F, 0x5F, 0x5F, 0x5F, 0x5F, 0xFA },
  { 0xF9, 0x5F, 0x5F, 0x5F, 0x5F, 0x5F, 0x5F, 0x5F, 0x5F, 0xFA },
  { 0xF9, 0x5F, 0x5F, 0x5F, 0x5F, 0x5F, 0x5F, 0x5F, 0x5F, 0xFA },
  { 0xF9, 0x5F, 0x5F, 0x5F, 0x5F, 0x5F, 0x5F, 0x5F, 0x5F, 0xFA },
  { 0xF9, 0x5F, 0x5F, 0x5F, 0x5F, 0x5F, 0x5F, 0x5F, 0x5F, 0xFA },
  { 0xF9, 0x5F, 0x5F, 0x5F, 0x5F, 0x5F, 0x5F, 0x5F, 0x5F, 0xFA },
  { 0xF9, 0x5F, 0x5F, 0x5F, 0x5F, 0x5F, 0x5F, 0x5F, 0x5F, 0xFA },
  { 0xF9, 0x5F, 0x5F, 0x5F, 0x5F, 0x5F, 0x5F, 0x5F, 0x5F, 0xFA },
  { 0xF9, 0x5F, 0x5F, 0x5F, 0x5F, 0x5F, 0x5F, 0x5F, 0x5F, 0xFA },
  { 0xF9, 0x5F, 0x5F, 0x5F, 0x5F, 0x5F, 0x5F, 0x5F, 0x5F, 0xFA },
  { 0xF0, 0xFB, 0xFB, 0xFB, 0xFB, 0xFB, 0xFB, 0xFB, 0xFB, 0xF1 }
};

void set_palette(const vga_poker_palette_t *arg)
{
  vga_poker_arg_t vga;
  vga.palette_t = *arg;

  if (ioctl(vga_poker_fd, VGA_POKER_SET_PALETTE, &vga)) {
    perror("ioctl(vga_poker_SET_BACKGROUND) failed");
    return;
  }
}

void set_pixels(const vga_poker_pixels_t *arg)
{
  vga_poker_arg_t vga;
  vga.pixel_coors = *arg;

  if (ioctl(vga_poker_fd, VGA_POKER_SET_PIXELS, &vga)) {
    perror("ioctl(vga_poker_SET_BACKGROUND) failed");
    return;
  }
}

void set_tile(const vga_poker_tile_coords_t *arg)
{
  vga_poker_arg_t vga;
  vga.tile_coords = *arg;

  if (ioctl(vga_poker_fd, VGA_POKER_SET_TILE, &vga)) {
    perror("ioctl(vga_poker_SET_BACKGROUND) failed");
    return;
  }
}

/* resources found online to convert */
void load_palette(const char *fn) {
  FILE *f = fopen(fn, "r");
  if (!f) { perror("fopen palette"); return; }

  char line[32];
  for (int i = 0; i < 16; i++) {
      if (!fgets(line, sizeof line, f)) break;
      unsigned int word;
      if (sscanf(line, "%8x", &word) != 1) {
        printf("err\n");
      }
      vga_poker_palette_t p = {
          .index = i,
          .red   = (word >> 24) & 0xFF,
          .green = (word >> 16) & 0xFF,
          .blue  = (word >>  8) & 0xFF
      };
      set_palette(&p);
  }
  fclose(f);
}

/* resources found online to convert */
void load_tileset(const char *fn) {
  FILE *f = fopen(fn, "r");
  if (!f) { perror("fopen tileset"); return; }

  char line[128];
  int tile_id = 0, row = 0;
  vga_poker_pixels_t px = {0};

  while (fgets(line, sizeof line, f)) {

      // parse 8 hex values
      unsigned int vals[8];
      sscanf(line,
                  "%2x %2x %2x %2x %2x %2x %2x %2x",
                  &vals[0],&vals[1],&vals[2],&vals[3],
                  &vals[4],&vals[5],&vals[6],&vals[7]);

      // on first row of a new tile, set px.tile_id
      px.tile_id = tile_id;
      // copy this row into pixels[ row*8 + col ]
      for (int col = 0; col < 8; col++) {
          px.pixels[row*8 + col] = vals[col] & 0x0F;
      }

      row++;
      if (row == 8) {
          // we have one full tile → send to hardware
          set_pixels(&px);
          row = 0;
          tile_id++;
          if (tile_id >= 256) break;  // guard
      }
  }
  fclose(f);
}

/* resources found online to convert */
void load_tilemap(const char *fn) {
  FILE *f = fopen(fn, "r");
  if (!f) {
      perror("fopen tilemap");
      return;
  }

  char line[512];
  for (int row = 0; row < 60; row++) {
      if (!fgets(line, sizeof line, f)) {
          fprintf(stderr, "tilemap: missing row %d\n", row);
          break;
      }

      // Tokenize the line on whitespace
      char *tok = strtok(line, " \t\r\n");
      for (int col = 0; col < 80; col++) {

          // Parse two hex digits into an integer
          unsigned int tid = (unsigned int)strtoul(tok, NULL, 16);

          // Clamp to 0–255
          tid &= 0xFF;

          vga_poker_tile_coords_t tc = {
              .row     = row,
              .col     = col,
              .tile_id = tid
          };
          set_tile(&tc);
          tile_map[row][col] = tid;
          tok = strtok(NULL, " \t\r\n");
      }
  }

  fclose(f);
}

/* resources found online to convert */
int load_deck(const char *fn)
{
    FILE *f = fopen(fn, "r");
    if (!f) {
        fprintf(stderr, "file open error");
        return -1;
    }

    char line[256];
    for (int line_no = 0; line_no < 52 * CARD_TILE_ID_ROWS; line_no++) {
        if (!fgets(line, sizeof(line), f)) {
        fclose(f);
        return -1;
    }

    // Which card and which row within that card?
    int card    = line_no / CARD_TILE_ID_ROWS;
    int row_in  = line_no % CARD_TILE_ID_ROWS;

    // Tokenize on whitespace
    char *tok = strtok(line, " \t\r\n");
    for (int col = 0; col < CARD_TILE_ID_COLS; col++) {
        if (!tok) {
            fclose(f);
            return -1;
        }
        unsigned int tid = strtoul(tok, NULL, 16) & 0xFF;
        deck[card][row_in][col] = (uint8_t)tid;
        tok = strtok(NULL, " \t\r\n");
    }
    // any extra tokens on the line are simply ignored
    }

    fclose(f);
    return 0;
}

/* resources found online to convert */
int load_jokers(const char *fn)
{
    FILE *f = fopen(fn, "r");
    if (!f) {
        fprintf(stderr, "file open error");
        return -1;
    }

    char line[256];
    for (int line_no = 0; line_no < 10 * CARD_TILE_ID_ROWS; line_no++) {
        if (!fgets(line, sizeof(line), f)) {
        fclose(f);
        return -1;
    }

    // Which card and which row within that card?
    int card    = line_no / CARD_TILE_ID_ROWS;
    int row_in  = line_no % CARD_TILE_ID_ROWS;

    // Tokenize on whitespace
    char *tok = strtok(line, " \t\r\n");
    for (int col = 0; col < CARD_TILE_ID_COLS; col++) {
        if (!tok) {
            fclose(f);
            return -1;
        }
        unsigned int tid = strtoul(tok, NULL, 16) & 0xFF;
        jokers[card][row_in][col] = (uint8_t)tid;
        tok = strtok(NULL, " \t\r\n");
    }
    // any extra tokens on the line are simply ignored
    }

    fclose(f);
    return 0;
}

/* tile id 201 - 226 for A - Z */
uint8_t char_to_letter_id(char letter)
{
  if (letter == ' ') return 250;
  if (letter < 'A' || letter > 'Z') return -1;
  return letter + 136;
}

/* tile_id 228 - 237 for 0 - 9 */
uint8_t score_to_digit_id(char digit)
{
  if (digit < '0' || digit > '9') return -1;
  return digit + 179;
}

/* redraws the entire screen baesed on whats in the tilemap[][] */
void redraw_screen(void)
{
  for (int row = 0; row < 60; row++) {
    for (int col = 0; col < 80; col++) {
      vga_poker_tile_coords_t tile = {
        .row     = row,
        .col     = col,
        .tile_id = tile_map[row][col]
      };
      set_tile(&tile);
    }
  }
}

void draw_chip(char *digits)
{
  for (uint8_t col = 0; col < SCORE_DIGITS - 2; col++) {
    uint8_t tid = score_to_digit_id(digits[col]);
    tile_map[CHIP_MULT_CALC_ROW][CHIP_COL_START + col] = tid;
    vga_poker_tile_coords_t tile = {
      .row     = CHIP_MULT_CALC_ROW,
      .col     = CHIP_COL_START + col,
      .tile_id = tid
    };
    set_tile(&tile);
  }
}

void draw_mult(char *digits)
{
  for (uint8_t col = 0; col < SCORE_DIGITS - 2; col++) {
    uint8_t tid = score_to_digit_id(digits[col]);
    tile_map[CHIP_MULT_CALC_ROW][MULT_COL_START + col] = tid;
    vga_poker_tile_coords_t tile = {
      .row     = CHIP_MULT_CALC_ROW,
      .col     = MULT_COL_START + col,
      .tile_id = tid
    };
    set_tile(&tile);
  }
}
/* Draws the target score digits on screen given a array[6] of chars (i.e. 001256) */
void draw_target_score(char *digits)
{
  // MAP TILE_ID TO VAL
  for (uint8_t col = 0; col < SCORE_DIGITS; col++) {
    uint8_t tid = score_to_digit_id(digits[col]);
    tile_map[TARGET_SCORE_ROW][SCORE_COL_START + col] = tid;
    vga_poker_tile_coords_t tile = {
      .row     = TARGET_SCORE_ROW,
      .col     = SCORE_COL_START + col,
      .tile_id = tid
    };
    set_tile(&tile);
  }
}

/* Draws the round score digits on screen given a array[6] of chars (i.e. 001256) */
void draw_round_score(char *digits)
{
  for (uint8_t col = 0; col < SCORE_DIGITS; col++) {
    uint8_t tid = score_to_digit_id(digits[col]);
    tile_map[ROUND_SCORE_ROW][SCORE_COL_START + col] = tid;
    vga_poker_tile_coords_t tile = {
      .row     = ROUND_SCORE_ROW,
      .col     = SCORE_COL_START + col,
      .tile_id = tid
    };
    set_tile(&tile);
  }
}

/* Draws the hand type letters on screen given an array[15] where spaces are included to center (i.e. ___FLUSH___) */
void draw_hand_type(char *hand_type)
{
  size_t len = strlen(hand_type);
  if (len > (HAND_TYPE_COL_END - HAND_TYPE_COL_START + 1))
      len = HAND_TYPE_COL_END - HAND_TYPE_COL_START + 1;

  for (uint8_t col = 0; col < len; col++) {
    uint8_t tid = char_to_letter_id(hand_type[col]);
    if (tid == 250) tid = 96;
    tile_map[HAND_TYPE_ROW][HAND_TYPE_COL_START + col] = tid;
    vga_poker_tile_coords_t tile = {
      .row     = HAND_TYPE_ROW,
      .col     = HAND_TYPE_COL_START + col,
      .tile_id = tid
    };
    set_tile(&tile);
  }
}

#define JOKER_NAME_ROW 49
#define JOKER_INFO_ROW_START 51
#define JOKER_INFO_ROW_END 55
#define JOKER_NAME_COL_START 2
#define JOKER_NAME_COL_END 15


/* Draws the hands_left digit on screen given a char digit */
void draw_hands_left(char digit)
{
  uint8_t tid = score_to_digit_id(digit);
  tile_map[HANDS_LEFT_ROW][HANDS_LEFT_COL] = tid;
  vga_poker_tile_coords_t tile = {
    .row     = HANDS_LEFT_ROW,
    .col     = HANDS_LEFT_COL,
    .tile_id = tid
  };
  set_tile(&tile);
}

/* Draws the discard digit on screen given a char digit */
void draw_discards(char digit)
{
  uint8_t tid = score_to_digit_id(digit);
  tile_map[DISCARD_ROW][DISCARD_COL] = tid;
  vga_poker_tile_coords_t tile = {
    .row     = DISCARD_ROW,
    .col     = DISCARD_COL,
    .tile_id = tid
  };
  set_tile(&tile);
}

/* Draws card in deck digits on screen given an array[2] where each index contains a char digit */
void draw_cards_in_deck(char *cards_left)
{
  uint8_t tid_1 = score_to_digit_id(cards_left[0]);
  uint8_t tid_2 = score_to_digit_id(cards_left[1]);
  tile_map[CARDS_IN_DECK_ROW][CARDS_IN_DECK_COL] = tid_1;
  tile_map[CARDS_IN_DECK_ROW][CARDS_IN_DECK_COL + 1] = tid_2;
  vga_poker_tile_coords_t tile = {
    .row     = CARDS_IN_DECK_ROW,
    .col     = CARDS_IN_DECK_COL,
    .tile_id = tid_1
  };
  set_tile(&tile);
  tile.col = CARDS_IN_DECK_COL + 1;
  tile.tile_id = tid_2;
  set_tile(&tile);
}

/* Draws the corresponding round digit on screen given an round_number char */
void draw_round(char round_number)
{
  uint8_t tid = score_to_digit_id(round_number);
  tile_map[ANTE_ROUND_ROW][ROUND_COL] = tid;
  vga_poker_tile_coords_t tile = {
    .row     = ANTE_ROUND_ROW,
    .col     = ROUND_COL,
    .tile_id = tid
  };
  set_tile(&tile);
}

/* Draws the corresponding ante digit on screen given an ante_number char */
void draw_ante(char ante_number)
{
  // MAP TILE_ID TO VAL
  uint8_t tid = score_to_digit_id(ante_number);
  tile_map[ANTE_ROUND_ROW][ANTE_COL] = tid;
  vga_poker_tile_coords_t tile = {
    .row     = ANTE_ROUND_ROW,
    .col     = ANTE_COL,
    .tile_id = tid
  };
  set_tile(&tile);
}


/* cards_g is a 5 integer array containing all of deck indicies */
void draw_jokers(uint8_t *cards_g)
{
  uint8_t valid_jokers[5];
  for (uint8_t i = 0; i < 5; i++) {
    if (cards_g[i] == 10) valid_jokers[i] = 0;
    else valid_jokers[i] = 1;
  }
  for (uint8_t row = 0; row < CARD_TILE_ID_ROWS; row++) {
    for (uint8_t col = 0; col < CARD_TILE_ID_COLS; col++) {
        tile_map[JOKER_ROW + row][JOKER_1_COL + col] = valid_jokers[0] == 1 ? jokers[cards_g[0]][row][col] : card_slot[row][col];
        tile_map[JOKER_ROW + row][JOKER_2_COL + col] = valid_jokers[1] == 1 ? jokers[cards_g[1]][row][col] : card_slot[row][col];
        tile_map[JOKER_ROW + row][JOKER_3_COL + col] = valid_jokers[2] == 1 ? jokers[cards_g[2]][row][col] : card_slot[row][col];
        tile_map[JOKER_ROW + row][JOKER_4_COL + col] = valid_jokers[3] == 1 ? jokers[cards_g[3]][row][col] : card_slot[row][col];
        tile_map[JOKER_ROW + row][JOKER_5_COL + col] = valid_jokers[4] == 1 ? jokers[cards_g[4]][row][col] : card_slot[row][col];
    }
  }
  redraw_screen();
}
/* cards_g is a 5 integer array containing all of deck indicies */
void draw_played_cards(uint8_t *cards_g)
{
  /* optimization to only draw tiles you change
  vga_poker_tile_coords_t tile = {
        .row     = row,
        .col     = col,
        .tile_id = tile_map[row][col]
      };
      set_tile(&tile);
  */
 uint8_t valid_cards[5];
 for (uint8_t i = 0; i < 5; i++) {
   if (cards_g[i] == 52) valid_cards[i] = 0;
   else valid_cards[i] = 1;
 }
  for (uint8_t row = 0; row < CARD_TILE_ID_ROWS; row++) {
    for (uint8_t col = 0; col < CARD_TILE_ID_COLS; col++) {
      tile_map[PLAYED_ROW + row][PLAYED_1_COL + col] = valid_cards[0] == 1 ? deck[cards_g[0]][row][col] : card_slot[row][col];
      tile_map[PLAYED_ROW + row][PLAYED_2_COL + col] = valid_cards[1] == 1 ? deck[cards_g[1]][row][col] : card_slot[row][col];
      tile_map[PLAYED_ROW + row][PLAYED_3_COL + col] = valid_cards[2] == 1 ? deck[cards_g[2]][row][col] : card_slot[row][col];
      tile_map[PLAYED_ROW + row][PLAYED_4_COL + col] = valid_cards[3] == 1 ? deck[cards_g[3]][row][col] : card_slot[row][col];
      tile_map[PLAYED_ROW + row][PLAYED_5_COL + col] = valid_cards[4] == 1 ? deck[cards_g[4]][row][col] : card_slot[row][col];
    }
  }
  redraw_screen();
}

/* Clears the played card positions one by one */
void clear_table(uint8_t amount)
{

  for (uint8_t row = 0; row < CARD_TILE_ID_ROWS; row++) {
    for (uint8_t col = 0; col < CARD_TILE_ID_COLS; col++) {
        tile_map[PLAYED_ROW + row][PLAYED_1_COL + col] = card_slot[row][col];
        vga_poker_tile_coords_t tile = {
          .row     = PLAYED_ROW + row,
          .col     = PLAYED_1_COL + col,
          .tile_id = card_slot[row][col]
        };
        set_tile(&tile);
    }
  }
  sleep(1);
  if (amount >= 2) {
    for (uint8_t row = 0; row < CARD_TILE_ID_ROWS; row++) {
      for (uint8_t col = 0; col < CARD_TILE_ID_COLS; col++) {
          tile_map[PLAYED_ROW + row][PLAYED_2_COL + col] = card_slot[row][col];
          vga_poker_tile_coords_t tile = {
            .row     = PLAYED_ROW + row,
            .col     = PLAYED_2_COL + col,
            .tile_id = card_slot[row][col]
          };
          set_tile(&tile);
      }
    }
    sleep(1);
  }
  if (amount >= 3) {
    for (uint8_t row = 0; row < CARD_TILE_ID_ROWS; row++) {
      for (uint8_t col = 0; col < CARD_TILE_ID_COLS; col++) {
          tile_map[PLAYED_ROW + row][PLAYED_3_COL + col] = card_slot[row][col];
          vga_poker_tile_coords_t tile = {
            .row     = PLAYED_ROW + row,
            .col     = PLAYED_3_COL + col,
            .tile_id = card_slot[row][col]
          };
          set_tile(&tile);
      }
    }
    sleep(1);
  }

  if (amount >= 4) {
    for (uint8_t row = 0; row < CARD_TILE_ID_ROWS; row++) {
      for (uint8_t col = 0; col < CARD_TILE_ID_COLS; col++) {
          tile_map[PLAYED_ROW + row][PLAYED_4_COL + col] = card_slot[row][col];
          vga_poker_tile_coords_t tile = {
            .row     = PLAYED_ROW + row,
            .col     = PLAYED_4_COL + col,
            .tile_id = card_slot[row][col]
          };
          set_tile(&tile);
      }
    }
    sleep(1);
  }
  if (amount >= 5) {
    for (uint8_t row = 0; row < CARD_TILE_ID_ROWS; row++) {
      for (uint8_t col = 0; col < CARD_TILE_ID_COLS; col++) {
          tile_map[PLAYED_ROW + row][PLAYED_5_COL + col] = card_slot[row][col];
          vga_poker_tile_coords_t tile = {
            .row     = PLAYED_ROW + row,
            .col     = PLAYED_5_COL + col,
            .tile_id = card_slot[row][col]
          };
          set_tile(&tile);
      }
    }
    sleep(1);
  }

}

/* selected_arr: 8 integer array of either 0 or 1 where 1 denotes selected */
void draw_selected(uint8_t *selected_arr)
{
    /* green tile_id = 95, yellow_id =  91, blue_id = 94*/
    uint8_t tile_id = 94;
    for (uint8_t i = 0; i < HAND_8_COL_END - HAND_1_COL_START + 1; i++) {
      tile_map[SELECTED_ROW][SELECTED_COL + i] = tile_id + 1;
      vga_poker_tile_coords_t tile = {
        .row     = SELECTED_ROW,
        .col     = SELECTED_COL + i,
        .tile_id = tile_id + 1
      };
      set_tile(&tile);
    }
    for (uint8_t i = 0; i < 8; i++) {
      if (selected_arr[i] != 0) {
        uint8_t position = i * 5;
        tile_map[SELECTED_ROW][SELECTED_COL + 1 + position] = tile_id;
        vga_poker_tile_coords_t tile = {
          .row     = SELECTED_ROW,
          .col     = SELECTED_COL + 1 + position,
          .tile_id = tile_id
        };
        set_tile(&tile);

        tile.col = tile.col + 1;
        tile_map[SELECTED_ROW][SELECTED_COL + 2 + position] = tile_id;
        set_tile(&tile);

        tile.col = tile.col + 1;
        tile_map[SELECTED_ROW][SELECTED_COL + 3 + position] = tile_id;
        set_tile(&tile);

        if (i == 7) {
          tile.col = tile.col + 1;
          tile_map[SELECTED_ROW][SELECTED_COL + 4 + position] = tile_id;
          set_tile(&tile);

          tile.col = tile.col + 1;
          tile_map[SELECTED_ROW][SELECTED_COL + 5 + position] = tile_id;
          set_tile(&tile);
          tile.col = tile.col + 1;

          tile_map[SELECTED_ROW][SELECTED_COL + 6 + position] = tile_id;
          set_tile(&tile);

          tile.col = tile.col + 1;
          tile_map[SELECTED_ROW][SELECTED_COL + 7 + position] = tile_id;
          set_tile(&tile);

          tile.col = tile.col + 1;
          tile_map[SELECTED_ROW][SELECTED_COL + 8 + position] = tile_id;
          set_tile(&tile);
        }
      }
     
    }
}

/* Draws a yellow cursor underneath the card in hand given an index of what card position you are at (i.e index 0 = left most card)*/
void draw_cursor(uint8_t index)
{
  /* green tile_id = 95, yellow_id =  91, red_id*/
  uint8_t position = index * 5;
  uint8_t tile_id = 91;
  uint8_t range = index == 7 ? 9 : 4;
  for (uint8_t i = 1; i < range; i++) {
    tile_map[CURSOR_ROW][CURSOR_COL + i + position] = tile_id;
    vga_poker_tile_coords_t tile = {
      .row     = CURSOR_ROW,
      .col     = CURSOR_COL + i + position,
      .tile_id = tile_id
    };
    set_tile(&tile);
  }
}
/* Draws a the backgournd underneath the card in hand given an index of what card position you are at (i.e index 0 = left most card)*/
void clear_cursor(uint8_t index)
{
    /* green tile_id = 95, yellow_id =  91, red_id*/
    uint8_t position = index * 5;
    uint8_t tile_id = 95;
    uint8_t range = index == 7 ? 9 : 4;
    for (uint8_t i = 1; i < range; i++) {
      tile_map[CURSOR_ROW][CURSOR_COL + i + position] = tile_id;
      vga_poker_tile_coords_t tile = {
        .row     = CURSOR_ROW,
        .col     = CURSOR_COL + i + position,
        .tile_id = tile_id
      };
      set_tile(&tile);
    }
}

/* Draws the hand given a cards_g[8] where each index in cards_g is an index (0 - 51) to a corrseponding card.
 * Example: if cards_g[0] = 0, then 0 would represent the 2 of clubs
*/
void draw_hand(uint8_t *cards_g)
{
  uint8_t valid_cards[8];
  for (uint8_t i = 0; i < 8; i++) {
    if (cards_g[i] == 52) valid_cards[i] = 0;
    else valid_cards[i] = 1;
  }
    for (uint8_t row = 0; row < CARD_TILE_ID_ROWS; row++) {
        for (uint8_t col = 0; col < CARD_TILE_ID_COLS / 2; col++) {
            tile_map[HAND_ROW_START + row][HAND_1_COL_START + col] = valid_cards[0] == 1 ? deck[cards_g[0]][row][col] : card_slot[row][col];
            tile_map[HAND_ROW_START + row][HAND_2_COL_START + col] = valid_cards[1] == 1 ? deck[cards_g[1]][row][col] : card_slot[row][col];
            tile_map[HAND_ROW_START + row][HAND_3_COL_START + col] = valid_cards[2] == 1 ? deck[cards_g[2]][row][col] : card_slot[row][col];
            tile_map[HAND_ROW_START + row][HAND_4_COL_START + col] = valid_cards[3] == 1 ? deck[cards_g[3]][row][col] : card_slot[row][col];
            tile_map[HAND_ROW_START + row][HAND_5_COL_START + col] = valid_cards[4] == 1 ? deck[cards_g[4]][row][col] : card_slot[row][col];
            tile_map[HAND_ROW_START + row][HAND_6_COL_START + col] = valid_cards[5] == 1 ? deck[cards_g[5]][row][col] : card_slot[row][col];
            tile_map[HAND_ROW_START + row][HAND_7_COL_START + col] = valid_cards[6] == 1 ? deck[cards_g[6]][row][col] : card_slot[row][col];
        }
    }
    for (uint8_t row = 0; row < CARD_TILE_ID_ROWS; row++) {
      for (uint8_t col = 0; col < CARD_TILE_ID_COLS; col++) {
        tile_map[HAND_ROW_START + row][HAND_8_COL_START + col] = valid_cards[7] == 1 ? deck[cards_g[7]][row][col] : card_slot[row][col];
      }
    }
    redraw_screen();
}
