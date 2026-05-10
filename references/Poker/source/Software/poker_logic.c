#define _POSIX_C_SOURCE 200809L
#define _DEFAULT_SOURCE

#include "controller.h"
#include "poker.h"
#include "vga_poker.h"
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <fcntl.h>
#include <libusb-1.0/libusb.h>

#define RANK(c)  ((c) >> 2)
#define SUIT(c)  ((c) & 0x3)
#define MAKE_CARD(rank,suit)  (((rank) << 2) | (suit))
#define REPORT_LEN 8 // CONTROLLER BYTES REPORTED
#define DEBOUNCE_DELAY 150000 // 150ms debounce time in microseconds

#define COMMON 0
#define UNCOMMON 1
#define RARE 2
#define LEGENDARY 3

#define SMILEY_FACE 0   // Uncommon: +5 mult per face card
#define EVEN_STEVEN 1   // Uncommon: +4 mult per even rank card
#define ODD_TODD 2      // Uncommon: +31 chip per odd rank card
#define SUIT_BUNDLER 3  // Rare: Spades = Clubs, Diamonds = Hearts
#define THE_ONE 4       // Legendary: Next 7 hands are royal flushes (consumed)
#define BLUE_DOT 5      // Common: +50 chip
#define RED_DOT 6       // Common: +10 mult
#define GREEN_CHECK 7   // Rare: Need only 25% to pass round (consumed)
#define BLANK 8         // Common: Does nothing (funsies)
#define STEVIE_DOTT 9   // Rare: Even sum 40 mult, odd sum 2X chip

typedef uint8_t Card;

typedef struct {
    int chips;
    int multiplier;
} HandValue;

typedef struct {
    int small_blind;
    int big_blind;
    int boss;
} Blind;

typedef struct {
	int ante_number;
    Blind blinds;
} Ante;

typedef struct {
    float joker_chance;
    float rarity_probs[4];
} drop_probs;

Card deck_l[52];
Card drawed_cards[8]; // CARDS READY TO BE SELECTED
Card joker_cards[10]; // JOKER CARDS (ON OR OFF)
Card selected_cards[5]; // SELECTED CARDS (NOT PLAYED YET)
Card played_hand[5]; // PLAYED CARDS (PLAY OR DISCARD)
Ante antes[8]; // ANTES
uint8_t index_for_tiles_cards[8];

enum HandType {
	HIGH_CARD,
    PAIR,
    TWO_PAIR,
    THREE_OF_A_KIND,
    STRAIGHT,
    FLUSH,
    FULL_HOUSE,
    FOUR_OF_A_KIND,
    STRAIGHT_FLUSH,
    ROYAL_FLUSH
};

const int JOKER_RARITIES[10] = {
    UNCOMMON,  // 0: Smiley Face
    UNCOMMON,  // 1: Even Steven
    UNCOMMON,  // 2: Odd Todd
    RARE,      // 3: Suit Bundler
    LEGENDARY, // 4: The One
    COMMON,    // 5: Blue Dot
    COMMON,    // 6: Red Dot
    RARE,      // 7: Green Check
    COMMON,    // 8: Blank
    RARE       // 9: Stevie Dott
};

int game_play = 0;
int draw_index = 0;
int draw_amount = 8;
int hands_remaining = 4;
int discards_remaining = 4;
int game_round = 1;
int ante = 1;
int current_blind = 0;
int cursor = 0;
int num_selected_cards = 0;
unsigned char last_button = 0;
struct timespec last_button_time;

/* VGA VARIABLES */
char target_score_str[7];
char round_score_str[7];
char chip_str[5];
char mult_str[5];
char hands_left_str[2];
char discards_left_str[2];
char cards_in_deck_str[3];
char ante_str[2];
char round_str[2];
uint8_t cursor_position;
uint8_t clear_cursor_position;
uint8_t selected_array[8]; // Array of selected cards (0=not selected, 1=selected)

// Antes 1-3: Early game (lowest odds of rare/legendary)
const drop_probs EARLY_GAME = {0.50f, {0.40f, 0.35f, 0.15f, 0.10f}};  
// Antes 4-6: Mid game (moderate odds of better jokers)
const drop_probs MID_GAME = {0.75f, {0.25f, 0.30f, 0.30f, 0.15f}};    
// Antes 7-8: Late game (best odds of powerful jokers)
const drop_probs LATE_GAME = {1.00f, {0.15f, 0.20f, 0.40f, 0.25f}};  


/* Hand Evaluation Functions */
HandValue get_hand_value(enum HandType hand_type);
int check_straight(int *rank_counts);
enum HandType evaluate_selected_cards();
enum HandType evaluate_hand(Card *hand, int num_cards);

/* Card and Deck Management Functions */
void init_deck();
void shuffle_deck();
Card draw_card();
int cards_remaining();
void draw_initial_pool_of_cards();
void draw_replacement_cards();
uint8_t* drawn_to_index();

/* Game State Management Functions */
void init_antes();
void init_jokers();
int play_selected_hand();
void discard_selected_cards();
int check_win_condition(int player_score);
void advance_game_state();
void round_reset();
void hard_reset();
void game_over();
void game_won();
void get_current_blind_info(char *blind_name, int *target_score);
void game_loop_vga();

/* VGA Display Functions */
const char* get_hand_name_vga(enum HandType hand);
char* get_target_score_vga(int target_score);
char* get_round_score_vga(int round_score);
char* get_chip_vga(int chip_value);
char* get_mult_vga(int multiplier);
char* get_hands_left_vga();
char* get_discards_left_vga();
char* get_cards_in_deck_vga();
char* get_ante_vga();
char* get_round_vga();
uint8_t get_cursor_position();
uint8_t get_clear_cursor_position();
void update_clear_cursor_position();
uint8_t* get_selected_cards_array();

/* Controller Input Functions */
void initialize_debounce();
int check_controller_input(unsigned char *report);
void toggle_card_selection();
void move_cursor(int direction);
void process_controller_input(unsigned char *report);

/* Score Calculation Functions */
int get_individual_card_chip(Card card);
int calculate_hand_type_specific_chips();

/* Joker Effect Functions */
int apply_smiley_face_joker(Card *hand, int num_cards, int base_multiplier);
int apply_even_steven_joker(Card *hand, int num_cards, int base_multiplier);
int apply_odd_todd_joker(Card *hand, int num_cards, int base_chips);
Card transform_card_suit_bundler(Card card);
void apply_suit_bundler_joker(Card *hand, int num_cards, Card *transformed_hand);
void activate_the_one_joker();
int check_the_one_joker();
int apply_blue_dot_joker(int base_chips);
int apply_red_dot_joker(int base_multiplier);
int apply_green_check_joker(int target_score, int current_score, int hands_left);
void apply_stevie_dott_joker(Card *hand, int num_cards, int *multiplier, int *card_chips);
void apply_all_joker_effects(Card *hand, int num_cards, enum HandType *hand_type, int *chips, int *multiplier, int target_score, int current_score, int hands_left);

/* Joker Management Functions */
void joker_drop();
int count_active_jokers(Card joker_cards[10]);

/* Score Calculation Additional Functions */
int calculate_played_hand_chips(Card *hand, int num_cards, enum HandType hand_type);

/* Main Function */
int main();

/* 
 * Given a HandType, Return The Hand Value
 */
HandValue get_hand_value(enum HandType hand_type)
{
    HandValue values[] = {
        {5, 1},    // HIGH_CARD
        {10, 2},   // PAIR
        {20, 2},   // TWO_PAIR
        {30, 3},   // THREE_OF_A_KIND
        {30, 4},   // STRAIGHT
        {35, 4},   // FLUSH
        {40, 4},   // FULL_HOUSE
        {60, 7},   // FOUR_OF_A_KIND
        {100, 10}, // STRAIGHT_FLUSH
        {200, 15}  // ROYAL_FLUSH
    };
    return values[hand_type];
}

/* 
 * Helper Function To Return String Version Of
 * Played Hand For VGA
 */
const char* get_hand_name_vga(enum HandType hand)
{
    const char* names[] = {
    	"   HIGH CARD   ",
        "     PAIR      ",
        "   TWO PAIR    ",
        "THREE OF A KIND",
        "   STRAIGHT    ",
        "     FLUSH     ",
        "   FULL HOUSE  ",
        " FOUR OF A KIND",
        " STRAIGHT FLUSH",
        "  ROYAL FLUSH  "
    };
    return names[hand];
}

/*
 * Converts target score to 6-character string
 * Pads with leading spaces
 */
char* get_target_score_vga(int target_score) {
    snprintf(target_score_str, sizeof(target_score_str), "%06d", target_score);
    return target_score_str;
}

/*
 * Converts current round score to 6-character string
 * Pads with leading spaces
 */
char* get_round_score_vga(int round_score) {
    snprintf(round_score_str, sizeof(round_score_str), "%06d", round_score);
    return round_score_str;
}

/*
 * Converts chip value to 4-character string
 * Pads with leading spaces
 */
char* get_chip_vga(int chip_value) {
    snprintf(chip_str, sizeof(chip_str), "%04d", chip_value);
    return chip_str;
}

/*
 * Converts multiplier to 4-character string
 * Pads with leading spaces
 */
char* get_mult_vga(int multiplier) {
    snprintf(mult_str, sizeof(mult_str), "%04d", multiplier);
    return mult_str;
}

/*
 * Converts hands left to 1-character string
 */
char* get_hands_left_vga() {
    snprintf(hands_left_str, sizeof(hands_left_str), "%d", hands_remaining);
    return hands_left_str;
}

/*
 * Converts discards left to 1-character string
 */
char* get_discards_left_vga() {
    snprintf(discards_left_str, sizeof(discards_left_str), "%d", discards_remaining);
    return discards_left_str;
}

/*
 * Converts cards in deck to 2-character string
 */
char* get_cards_in_deck_vga() {
    int cards = cards_remaining();
    if (cards < 10) {
        snprintf(cards_in_deck_str, sizeof(cards_in_deck_str), "0%d", cards);
    } else {
        snprintf(cards_in_deck_str, sizeof(cards_in_deck_str), "%d", cards);
    }
    
    return cards_in_deck_str;
}

/*
 * Converts ante to 1-character string
 */
char* get_ante_vga() {
    snprintf(ante_str, sizeof(ante_str), "%d", ante);
    return ante_str;
}

/*
 * Converts round to 1-character string
 */
char* get_round_vga() {
    snprintf(round_str, sizeof(round_str), "%d", game_round);
    return round_str;
}

/*
 * Gets current cursor position (0-7)
 */
uint8_t get_cursor_position() {
    return (uint8_t)cursor;
}

/*
 * Gets previous cursor position for clearing
 * Should be called before updating cursor
 */
uint8_t get_clear_cursor_position() {
    return clear_cursor_position;
}

/*
 * Updates the clear cursor position to current cursor
 * Call this whenever cursor is about to change
 */
void update_clear_cursor_position() {
    clear_cursor_position = cursor;
}

/*
 * Builds an array of selected card indicators
 * Returns an array of 8 uint8_t values:
 * 0 = not selected, 1 = selected
 */
uint8_t* get_selected_cards_array() {
    // Initialize all to not selected
    for (int i = 0; i < 8; i++) {
        selected_array[i] = 0;
    }
    
    // Mark selected cards
    for (int i = 0; i < num_selected_cards; i++) {
        for (int j = 0; j < draw_amount; j++) {
            if (drawed_cards[j] == selected_cards[i]) {
                selected_array[j] = 1;
                break;
            }
        }
    }
    
    return selected_array;
}


/* 
 * Initialize Deck of Cards
 * Suit[0, 1, 2, 3] = [Clove, Diamond, Spade, Heart]
 * Rank[0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12] = [2, 3, 4, 5, 6, 7, 8, 9, 10, J, Q, K, A]
 */
void init_deck()
{
	int idx = 0;
	for (int suit = 0; suit < 4; suit++) {
		for (int rank = 0; rank < 13; rank++) {
			deck_l[idx] = MAKE_CARD(rank, suit);
			idx++;
		}
	}
}

/*
 * Initialize The Antes And Blinds
 * Small Blinds = Base Values
 * Big Blinds = Base Values * 1.5
 * Boss Blinds = Base Values * 2
 */
void init_antes()
{
    int base_values[] = {300, 800, 2000, 5000, 11000, 20000, 35000, 50000};
    
    for (int i = 0; i < 8; i++) {
        antes[i].ante_number = i + 1;
        antes[i].blinds.small_blind = base_values[i];
        antes[i].blinds.big_blind = (int)(base_values[i] * 1.5);
        antes[i].blinds.boss = base_values[i] * 2;
    }
}

/*
 * Initialize Joker Cards
 * Initialized to 0.
 */
void init_jokers()
{
	for (int i = 0; i < 10; i++) {
		joker_cards[i] = 0;
	}
}

/*
 * Shuffles the Deck of Cards
 * Uses Fisher-Yates (Knuth) Shuffle Algorithm
 */
void shuffle_deck()
{
	/* NEED THIS FOR RANDOM SEEDING */
	srand(time(NULL));
	for (int i = 51; i > 0; i--) {
		// Generate random index between 0 and i (inclusive)
		int j = rand() % (i + 1);
		// Swap deck[i] with deck[j]
		Card temp = deck_l[i];
		deck_l[i] = deck_l[j];
		deck_l[j] = temp;
	}
}

/* 
 * Draws A Card From The Deck
 * 
 * On Success: Returns The Topmost Card
 * On Error: Returns 0xFF
 */
Card draw_card()
{
	if (draw_index < 52 && cards_remaining() > 0) {
		Card card = deck_l[draw_index];
		draw_index++;
		return card;
	}
	return 0xFF;
}

/*
 * Checks The Amount Of Cards Remaining In The Deck
 *
 * Returns The Amount Of Cards Remaining In The Deck
 */
int cards_remaining()
{
	return 52 - draw_index;
}

/*
 * Draw the initial pool of cards. Initially 8 (draw_amount).
 * Can be expanded to 10.
 */
void draw_initial_pool_of_cards()
{
	for (int i = 0; i < draw_amount; i++) {
		drawed_cards[i] = draw_card();
	}
}

/*
 * Takes An Array Of The Selected Card Ranks
 * Sees If The Selected Cards Form a Straight
 *
 * Returns 1 If A Straight Is Formed
 * Returns 0 If No Straight If Formed
 */
int check_straight(int *rank_counts)
{
    // First, count how many different ranks we have
    int unique_ranks = 0;
    for (int i = 0; i < 13; i++) {
        if (rank_counts[i] > 0) {
            unique_ranks++;
        }
    }
    
    // A straight must have exactly 5 different ranks
    if (unique_ranks != 5) {
        return 0;
    }
    
    // Check for regular straight (5 consecutive ranks)
    int consecutive = 0;
    for (int i = 0; i < 13; i++) {
        if (rank_counts[i] > 0) {
            consecutive++;
            if (consecutive == 5) {
                return 1;
            }
        } else {
            consecutive = 0;  // Reset counter when we find a gap
        }
    }
    
    // Check for Ace-low straight (A-2-3-4-5)
    // Must have exactly these 5 ranks and no others
    if (rank_counts[12] > 0 && rank_counts[0] > 0 && rank_counts[1] > 0 && 
        rank_counts[2] > 0 && rank_counts[3] > 0) {
        // Verify we don't have any other ranks
        for (int i = 4; i < 12; i++) {
            if (rank_counts[i] > 0) {
                return 0;
            }
        }
        return 1;
    }
    
    return 0;
}

/*
 * Evaluate Selected Cards
 * Returns The Hand Type (enum HandType) for any number of cards (1-5)
 */
enum HandType evaluate_selected_cards()
{
    // Return HIGH_CARD if no cards selected
    if (num_selected_cards == 0) {
        return HIGH_CARD;
    }
    
    // Count ranks and suits
    int rank_counts[13] = {0};  // Index = rank (0-12)
    int suit_counts[4] = {0};   // Index = suit (0-3)
    
    for (int i = 0; i < num_selected_cards; i++) {
        rank_counts[RANK(selected_cards[i])]++;
        suit_counts[SUIT(selected_cards[i])]++;
    }
    
	// For flush, we need 5 cards
    int is_flush = 0;
    if (num_selected_cards == 5) {
        for (int i = 0; i < 4; i++) {
            if (suit_counts[i] == 5) {
                is_flush = 1;
                break;
            }
        }
    }
    
    // For straight, we need 5 cards
    int is_straight = (num_selected_cards == 5) ? check_straight(rank_counts) : 0;
    
    // Count pairs, trips, quads
    int pairs = 0, trips = 0, quads = 0;
    
    for (int i = 0; i < 13; i++) {
        if (rank_counts[i] == 2) pairs++;
        if (rank_counts[i] == 3) trips++;
        if (rank_counts[i] == 4) quads++;
    }
    
    // Royal flush and straight flush require 5 cards
    if (num_selected_cards == 5 && is_flush && is_straight) {
        // Check if it's a royal (10-J-Q-K-A)
        if (rank_counts[8] > 0 && rank_counts[9] > 0 && rank_counts[10] > 0 && 
            rank_counts[11] > 0 && rank_counts[12] > 0) {
            return ROYAL_FLUSH;
        }
        return STRAIGHT_FLUSH;
    }
    
    // Evaluate best possible hand based on cards played
    if (quads > 0) return FOUR_OF_A_KIND;
    if (trips > 0 && pairs > 0) return FULL_HOUSE;
    if (is_flush) return FLUSH;
    if (is_straight) return STRAIGHT;
    if (trips > 0) return THREE_OF_A_KIND;
    if (pairs == 2) return TWO_PAIR;
    if (pairs == 1) return PAIR;
    
    return HIGH_CARD;
}

/*
 * Given a Hand And Num Cards
 * Evaluate The Type Of Hand
 */
enum HandType evaluate_hand(Card *hand, int num_cards)
{
    // Handle empty hand or more than 5 cards
    if (num_cards <= 0 || num_cards > 5) {
        return HIGH_CARD;
    }
    
    // Count ranks and suits
    int rank_counts[13] = {0};  // Index = rank (0-12)
    int suit_counts[4] = {0};   // Index = suit (0-3)
    
    for (int i = 0; i < num_cards; i++) {
        if (hand[i] == 0) continue; // Skip empty slots
        
        rank_counts[RANK(hand[i])]++;
        suit_counts[SUIT(hand[i])]++;
    }
    
    // Check for flush - requires exactly 5 cards of same suit
    int is_flush = 0;
    if (num_cards == 5) {  // Flush requires exactly 5 cards
        for (int i = 0; i < 4; i++) {
            if (suit_counts[i] == 5) {
                is_flush = 1;
                break;
            }
        }
    }
    
    // For straight, we need 5 cards
    int is_straight = (num_cards == 5) ? check_straight(rank_counts) : 0;
    
    // Count pairs, trips, quads
    int pairs = 0, trips = 0, quads = 0;
    
    for (int i = 0; i < 13; i++) {
        if (rank_counts[i] == 2) pairs++;
        if (rank_counts[i] == 3) trips++;
        if (rank_counts[i] == 4) quads++;
    }
    
    // Royal flush and straight flush require 5 cards
    if (num_cards == 5 && is_flush && is_straight) {
        // Check if it's a royal (10-J-Q-K-A)
        if (rank_counts[8] > 0 && rank_counts[9] > 0 && rank_counts[10] > 0 && 
            rank_counts[11] > 0 && rank_counts[12] > 0) {
            return ROYAL_FLUSH;
        }
        return STRAIGHT_FLUSH;
    }
    
    // Evaluate best possible hand based on cards played
    if (quads > 0) return FOUR_OF_A_KIND;
    if (trips > 0 && pairs > 0) return FULL_HOUSE;
    if (is_flush) return FLUSH;  // Only possible with 5 cards
    if (is_straight) return STRAIGHT;  // Only possible with 5 cards
    if (trips > 0) return THREE_OF_A_KIND;
    if (pairs == 2) return TWO_PAIR;
    if (pairs == 1) return PAIR;
    
    return HIGH_CARD;
}

/*
 * Get Individual Card Chip Value
 * 2-9: Face value (2 chips, 3 chips, etc.)
 * 10, J, Q, K: 10 chips each
 * A: 11 chips
 */
int get_individual_card_chip(Card card)
{
    int rank = RANK(card);
    
    if (rank >= 0 && rank <= 7) {        // 2-9 (rank 0-7)
        return rank + 2;                 // rank 0 = 2, rank 1 = 3, etc.
    } else if (rank >= 8 && rank <= 11) { // 10, J, Q, K (rank 8-11)
        return 10;
    } else if (rank == 12) {             // A (rank 12)
        return 11;
    }
    return 0;
}

/*
 * Calculate Chip Values for Cards that Make Up the Hand Type
 * Works with any number of cards (1-5)
 */
int calculate_hand_type_specific_chips()
{
    // Handle case of no cards
    if (num_selected_cards == 0) {
        return 0;
    }
    
    // Count ranks
    int rank_counts[13] = {0};
    Card cards_by_rank[13][4]; // Store which cards have each rank
    int count_by_rank[13] = {0};
    
    for (int i = 0; i < num_selected_cards; i++) {
        int rank = RANK(selected_cards[i]);
        rank_counts[rank]++;
        cards_by_rank[rank][count_by_rank[rank]] = selected_cards[i];
        count_by_rank[rank]++;
    }
    
    enum HandType hand_type = evaluate_selected_cards();
    int total_chips = 0;
    
    switch(hand_type) {
        case ROYAL_FLUSH:
        case STRAIGHT_FLUSH:
		case FULL_HOUSE:
        case FLUSH:
        case STRAIGHT:
            // All cards make up these hands
            for (int i = 0; i < num_selected_cards; i++) {
                total_chips += get_individual_card_chip(selected_cards[i]);
            }
            break;
            
        case FOUR_OF_A_KIND:
            // Only the matching cards count
            for (int i = 0; i < 13; i++) {
                if (rank_counts[i] == 4) {
                    for (int j = 0; j < 4; j++) {
                        total_chips += get_individual_card_chip(cards_by_rank[i][j]);
                    }
                    break;
                } else if (rank_counts[i] == 3) {
                    // If we only have 3 cards that match
                    for (int j = 0; j < 3; j++) {
                        total_chips += get_individual_card_chip(cards_by_rank[i][j]);
                    }
                    break;
                } else if (rank_counts[i] == 2) {
                    // If we only have 2 cards that match
                    for (int j = 0; j < 2; j++) {
                        total_chips += get_individual_card_chip(cards_by_rank[i][j]);
                    }
                    break;
                }
            }
            break;
            
        case THREE_OF_A_KIND:
            // Only the matching cards count
            for (int i = 0; i < 13; i++) {
                if (rank_counts[i] == 3) {
                    for (int j = 0; j < 3; j++) {
                        total_chips += get_individual_card_chip(cards_by_rank[i][j]);
                    }
                    break;
                } else if (rank_counts[i] == 2) {
                    // If we only have 2 cards of a kind
                    for (int j = 0; j < 2; j++) {
                        total_chips += get_individual_card_chip(cards_by_rank[i][j]);
                    }
                    break;
                }
            }
            break;
            
        case TWO_PAIR:
            // Only the cards that form the pairs count
            for (int i = 0; i < 13; i++) {
                if (rank_counts[i] == 2) {
                    for (int j = 0; j < 2; j++) {
                        total_chips += get_individual_card_chip(cards_by_rank[i][j]);
                    }
                }
            }
            break;
            
        case PAIR:
            // Only the matching cards count
            for (int i = 0; i < 13; i++) {
                if (rank_counts[i] == 2) {
                    for (int j = 0; j < 2; j++) {
                        total_chips += get_individual_card_chip(cards_by_rank[i][j]);
                    }
                    break;
                }
            }
            break;
            
        case HIGH_CARD: {
            // Only the highest card counts
            int highest_rank = -1;
            Card highest_card = 0;
            for (int i = 0; i < num_selected_cards; i++) {
                int rank = RANK(selected_cards[i]);
                if (rank > highest_rank) {
                    highest_rank = rank;
                    highest_card = selected_cards[i];
                }
            }
            total_chips = get_individual_card_chip(highest_card);
            break;
        }
    }
    
    return total_chips;
}

/* 
 * Play Selected Cards
 */
int play_selected_hand()
{   
    // Copy selected cards to played_hand
    for (int i = 0; i < num_selected_cards; i++) {
        played_hand[i] = selected_cards[i];
    }
    
    // Clear remaining slots in played_hand
    for (int i = num_selected_cards; i < 5; i++) {
        played_hand[i] = 0;
    }
    
    // Evaluate hand and calculate score
    enum HandType hand_type = evaluate_selected_cards();
    HandValue hand_value = get_hand_value(hand_type);
    int hand_specific_chips = calculate_hand_type_specific_chips();
    int total_chip_value = hand_value.chips + hand_specific_chips;
    int score = total_chip_value * hand_value.multiplier;
    
    // Mark played cards as invalid in drawed_cards
    for (int i = 0; i < num_selected_cards; i++) {
        for (int j = 0; j < draw_amount; j++) {
            if (drawed_cards[j] == selected_cards[i]) {
                drawed_cards[j] = 0xFF; // Mark as played
                break;
            }
        }
    }
    
    // Clear selected cards
    for (int i = 0; i < num_selected_cards; i++) {
        selected_cards[i] = 0;
    }
    
    num_selected_cards = 0; // Reset selection count
    hands_remaining--;
    
    // Reset cursor to a valid position
    cursor = 0;
    
    return score;
}

/*
 * Discard Selected Cards
 * Fixed to properly discard any number of cards
 */
void discard_selected_cards()
{
    // Copy selected cards to played_hand for tracking
    for (int i = 0; i < num_selected_cards; i++) {
        played_hand[i] = selected_cards[i];
    }
    
    // Clear remaining slots in played_hand
    for (int i = num_selected_cards; i < 5; i++) {
        played_hand[i] = 0;
    }

    // Mark selected cards as invalid in drawed_cards
    for (int i = 0; i < num_selected_cards; i++) {
        for (int j = 0; j < draw_amount; j++) {
            if (drawed_cards[j] == played_hand[i]) {
                drawed_cards[j] = 0xFF; // Mark as discarded
                break;
            }
        }
    }
    
    // Clear selected cards
    for (int i = 0; i < num_selected_cards; i++) {
        selected_cards[i] = 0;
    }
    
    num_selected_cards = 0; // Reset selection count
    discards_remaining--;
}

/*
 * Draws Replacement Cards After Play/Dicarded
 */
void draw_replacement_cards()
{
    for (int i = 0; i < draw_amount; i++) {
        if (drawed_cards[i] == 0xFF) {
            Card new_card = draw_card();
            if (new_card != 0xFF) {  // Valid card drawn
                drawed_cards[i] = new_card;
            }
        }
    }
}

/*
 * Function That Checks If The Player Has Reached The Target
 * Score For The Ante/Blind/Round They Are On
 */
int check_win_condition(int player_score)
{   
    Ante current_ante = antes[ante - 1];
    int target_score;

    if (current_blind == 0) {
        target_score = current_ante.blinds.small_blind;
    } else if (current_blind == 1) {
        target_score = current_ante.blinds.big_blind;
    } else {
        target_score = current_ante.blinds.boss;
    }

    return player_score >= target_score;
}

/*
 * Advance Game State Function
 *
 * Called After User Beats A Blind
 */
void advance_game_state()
{
    current_blind++;
    game_round++;
    if (game_round > 3) {
        game_round = 1;
    }

    if (current_blind > 2) {
        current_blind = 0;
        ante++;

        if (ante > 8) {
            game_won();
        }
    }
    round_reset();
}

/*
 * Gets The Current Blind Name and Target Score
 * 
 * Ie. Small Blind, Big Blind, Boss Blind
 * Retrieves The Target Score Needed To Beat Blind
 */
void get_current_blind_info(char *blind_name, int *target_score)
{   
    Ante current_ante = antes[ante - 1];
    
    if (current_blind == 0) {
        strcpy(blind_name, "Small Blind");
        *target_score = current_ante.blinds.small_blind;
    } else if (current_blind == 1) {
        strcpy(blind_name, "Big Blind");
        *target_score = current_ante.blinds.big_blind;
    } else {
        strcpy(blind_name, "Boss");
        *target_score = current_ante.blinds.boss;
    }
}

/*
 * Initialize the debounce timer
 */
void initialize_debounce() {
    clock_gettime(CLOCK_MONOTONIC, &last_button_time);
}

/*
 * Check If Controller Report Matches Expected Pattern
 */
int check_controller_input(unsigned char *report)
{
    unsigned char patterns[][8] = {
        {0x01, 0x7f, 0x7f, 0x7f, 0x7f, 0x0f, 0x00, 0x00}, // NONE
        {0x01, 0x7f, 0x7f, 0x00, 0x7f, 0x0f, 0x00, 0x00}, // LEFT
        {0x01, 0x7f, 0x7f, 0xff, 0x7f, 0x0f, 0x00, 0x00}, // RIGHT
        {0x01, 0x7f, 0x7f, 0x7f, 0x00, 0x0f, 0x00, 0x00}, // UP
        {0x01, 0x7f, 0x7f, 0x7f, 0xff, 0x0f, 0x00, 0x00}, // DOWN
        {0x01, 0x7f, 0x7f, 0x7f, 0x7f, 0x2f, 0x00, 0x00}, // A
        {0x01, 0x7f, 0x7f, 0x7f, 0x7f, 0x4f, 0x00, 0x00}, // B
        {0x01, 0x7f, 0x7f, 0x7f, 0x7f, 0x0f, 0x10, 0x00}, // SELECT
        {0x01, 0x7f, 0x7f, 0x7f, 0x7f, 0x0f, 0x20, 0x00}  // START
    };
    
    enum {NONE = 0, LEFT, RIGHT, UP, DOWN, A, B, SELECT, START};
    
    for (int i = 0; i < 9; i++) {
        int match = 1;
        for (int j = 0; j < 8; j++) {
            if (report[j] != patterns[i][j]) {
                match = 0;
                break;
            }
        }
        if (match) return i;
    }

    return NONE;
}

/*
 * Toggle Card Selection Based on Cursor Position
 */
void toggle_card_selection()
{
    // Check if card is already selected
    int is_selected = 0;
    int selected_index = -1;
    
    for (int i = 0; i < num_selected_cards; i++) {
        if (selected_cards[i] == drawed_cards[cursor]) {
            is_selected = 1;
            selected_index = i;
            break;
        }
    }
    
    if (is_selected) {
        // Remove from selection
        for (int i = selected_index; i < num_selected_cards - 1; i++) {
            selected_cards[i] = selected_cards[i + 1];
        }
        num_selected_cards--;
        selected_cards[num_selected_cards] = 0; // Clear the slot
    } else {
        // Add to selection (if not full)
        if (num_selected_cards < 5) {
            selected_cards[num_selected_cards] = drawed_cards[cursor];
            num_selected_cards++;
        }
    }
}

/*
 * Move Cursor Left or Right
 */
void move_cursor(int direction)
{
    int original_cursor = cursor;
    int valid_position_found = 0;
    int attempts = 0;
    
    // Try to find a valid position up to draw_amount attempts
    while (!valid_position_found && attempts < draw_amount) {
        if (direction > 0) { // Move right
            cursor = (cursor + 1) % draw_amount;
        } else { // Move left
            cursor = (cursor - 1 + draw_amount) % draw_amount;
        }
        
        // Check if this position has a valid card
        if (drawed_cards[cursor] != 0xFF) {
            valid_position_found = 1;
        }
        
        attempts++;
    }
    
    // If no valid position found, reset to original position (should never happen if at least one card exists)
    if (!valid_position_found) {
        cursor = original_cursor;
    }
}


/*
 * Process Controller Input
 * Updated to handle debouncing and support playing 1-5 cards
 */
void process_controller_input(unsigned char *report)
{
    int button = check_controller_input(report);

    if (button == 0) {
        last_button = 0;
		return;
    } else if ((button == last_button)) {
        return;
    }

	clock_gettime(CLOCK_MONOTONIC, &last_button_time);
	last_button = button;
    if (!game_play) {
		if (button == 8) {
			load_tilemap("tilemap.hex");
			game_play = 1;
		}
	} else if (game_play) {
	
		switch(button) {
			case 2: // RIGHT
				move_cursor(1);
				break;
				
			case 1: // LEFT
				move_cursor(-1);
				break;
				
			case 5: // A (Select/Deselect)
				toggle_card_selection();
				break;
				
			case 6: // B (Remove from selection)
				// Remove the card at cursor from selection if it's selected
				for (int i = 0; i < num_selected_cards; i++) {
					if (selected_cards[i] == drawed_cards[cursor]) {
						for (int j = i; j < num_selected_cards - 1; j++) {
							selected_cards[j] = selected_cards[j + 1];
						}
						num_selected_cards--;
						selected_cards[num_selected_cards] = 0;
						break;
					}
				}
				break;
				
			case 8: // START (Play selected cards - 1 to 5 cards allowed)
				if (game_play && num_selected_cards > 0 && num_selected_cards <= 5) {
					play_selected_hand();
				}
				break;
				
			case 7: // SELECT (Discard selected cards)
				if (num_selected_cards > 0 && discards_remaining > 0) {
					discard_selected_cards();
					draw_replacement_cards();
				}
				break;
		}
	}
}

/*
 * Map drawn cards to tile indices for display
 */
uint8_t *drawn_to_index()
{
	for (int i = 0; i < draw_amount; i++) {
		if (SUIT(drawed_cards[i]) == 0) {
			index_for_tiles_cards[i] = RANK(drawed_cards[i]);
		} else if (SUIT(drawed_cards[i]) == 1) {
			index_for_tiles_cards[i] = (13) + RANK(drawed_cards[i]);
		} else if (SUIT(drawed_cards[i]) == 2) {
			index_for_tiles_cards[i] = (13 * 2) + RANK(drawed_cards[i]);
		} else if (SUIT(drawed_cards[i]) == 3) {
			index_for_tiles_cards[i] = (13 * 3) + RANK(drawed_cards[i]);
		}
	}
	return index_for_tiles_cards;
}

/* 
 * Reset Function
 * 
 * Called When User Wins a Game Round
 */
void round_reset()
{
    draw_index = 0;
    draw_amount = 8;
    hands_remaining = 4;
    discards_remaining = 4;
	cursor = 0;
	init_deck();
	init_antes();
	shuffle_deck();
	draw_initial_pool_of_cards();
	drawn_to_index();
}

/* 
 * HARD Reset Function
 * 
 * Called When Restarting Game
 */
void hard_reset()
{
	ante = 1;
    current_blind = 0;
    game_round = 1;
    draw_index = 0;
    draw_amount = 8;
    hands_remaining = 4;
    discards_remaining = 4;
	init_deck();
	init_antes();
	init_jokers();
	shuffle_deck();
	draw_initial_pool_of_cards();
	drawn_to_index();
}

/* 
 * Game Over Function
 * Called When User Loses
 */
void game_over()
{
	load_tilemap("256-Game-Over.hex");
	sleep(3);
	load_tilemap("256-title.hex");
	game_play = 0;
}

/*
 * Game Won Function. 
 *
 * Called After User Beats Ante 8
 */
void game_won()
{
	// DRAW SOMETHING
	load_tilemap("256-title.hex");
	game_play = 0;
}

/*
 * Calculate Chip Values for Cards that Make Up a Played Hand
 * Works with any number of cards (1-5)
 * Takes the hand array, number of cards, and hand type as parameters
 */
int calculate_played_hand_chips(Card *hand, int num_cards, enum HandType hand_type)
{
    if (num_cards == 0) {
        return 0;
    }
    
    // Count ranks
    int rank_counts[13] = {0};
    Card cards_by_rank[13][4]; // Store which cards have each rank
    int count_by_rank[13] = {0};
    
    for (int i = 0; i < num_cards; i++) {
        if (hand[i] == 0) continue; // Skip empty slots
        
        int rank = RANK(hand[i]);
        rank_counts[rank]++;
        cards_by_rank[rank][count_by_rank[rank]] = hand[i];
        count_by_rank[rank]++;
    }
    
    int total_chips = 0;
    
    switch(hand_type) {
        case ROYAL_FLUSH:
        case STRAIGHT_FLUSH:
        case FULL_HOUSE:
        case FLUSH:
        case STRAIGHT:
            // All cards make up these hands
            for (int i = 0; i < num_cards; i++) {
                if (hand[i] != 0) { // Skip empty slots
                    total_chips += get_individual_card_chip(hand[i]);
                }
            }
            break;
            
        case FOUR_OF_A_KIND:
            // Only the matching cards count
            for (int i = 0; i < 13; i++) {
                if (rank_counts[i] == 4) {
                    for (int j = 0; j < 4; j++) {
                        total_chips += get_individual_card_chip(cards_by_rank[i][j]);
                    }
                    break;
                } else if (rank_counts[i] == 3) {
                    // If we only have 3 cards that match
                    for (int j = 0; j < 3; j++) {
                        total_chips += get_individual_card_chip(cards_by_rank[i][j]);
                    }
                    break;
                } else if (rank_counts[i] == 2) {
                    // If we only have 2 cards that match
                    for (int j = 0; j < 2; j++) {
                        total_chips += get_individual_card_chip(cards_by_rank[i][j]);
                    }
                    break;
                }
            }
            break;
            
        case THREE_OF_A_KIND:
            // Only the matching cards count
            for (int i = 0; i < 13; i++) {
                if (rank_counts[i] == 3) {
                    for (int j = 0; j < 3; j++) {
                        total_chips += get_individual_card_chip(cards_by_rank[i][j]);
                    }
                    break;
                } else if (rank_counts[i] == 2) {
                    // If we only have 2 cards of a kind
                    for (int j = 0; j < 2; j++) {
                        total_chips += get_individual_card_chip(cards_by_rank[i][j]);
                    }
                    break;
                }
            }
            break;
            
        case TWO_PAIR:
            // Only the cards that form the pairs count
            for (int i = 0; i < 13; i++) {
                if (rank_counts[i] == 2) {
                    for (int j = 0; j < 2; j++) {
                        total_chips += get_individual_card_chip(cards_by_rank[i][j]);
                    }
                }
            }
            break;
            
        case PAIR:
            // Only the matching cards count
            for (int i = 0; i < 13; i++) {
                if (rank_counts[i] == 2) {
                    for (int j = 0; j < 2; j++) {
                        total_chips += get_individual_card_chip(cards_by_rank[i][j]);
                    }
                    break;
                }
            }
            break;
            
        case HIGH_CARD: {
            // Only the highest card counts
            int highest_rank = -1;
            Card highest_card = 0;
            for (int i = 0; i < num_cards; i++) {
                if (hand[i] == 0) continue; // Skip empty slots
                
                int rank = RANK(hand[i]);
                if (rank > highest_rank) {
                    highest_rank = rank;
                    highest_card = hand[i];
                }
            }
            total_chips = get_individual_card_chip(highest_card);
            break;
        }
    }
    
    return total_chips;
}

/*
 * Counts how many active jokers the player currently has
 * Returns a count of all joker_cards[] elements with value 1
 */
int count_active_jokers(Card joker_cards[10])
{
    int count = 0;
    for (int i = 0; i < 10; i++) {
        if (joker_cards[i] == 1) {
            count++;
        }
    }
    return count;
}

/*
 * Joker Probability Drawer
 * Determines if a joker drops after beating a blind/round
 * Selects which joker to give based on rarity tables
 */
void joker_drop()
{
    // Get current ante to determine game stage
    int current_ante = ante;
    
    // Check if player already has maximum jokers
    if (count_active_jokers(joker_cards) >= 5) {
        return; // No more jokers can be added
    }
    
    // Select probability table based on game stage
    drop_probs probs;
    if (current_ante <= 3) {
        probs = EARLY_GAME;
    } else if (current_ante <= 6) {
        probs = MID_GAME;
    } else {
        probs = LATE_GAME;
    }
    
    // Determine if a joker drops at all
    float roll = (float)rand() / RAND_MAX;
    if (roll > probs.joker_chance) {
        return; // No joker drops
    }
    
    // Determine the rarity of the joker
    roll = (float)rand() / RAND_MAX;
    int rarity;
    float cumulative = 0.0f;
    for (rarity = 0; rarity < 4; rarity++) {
        cumulative += probs.rarity_probs[rarity];
        if (roll <= cumulative) break;
    }
    
    // Find available jokers of the chosen rarity
    int available_jokers[10];
    int count = 0;
    
    for (int i = 0; i < 10; i++) {
        if (JOKER_RARITIES[i] == rarity && joker_cards[i] == 0) {
            available_jokers[count++] = i;
        }
    }
    
    // If no jokers of chosen rarity are available, try any rarity
    if (count == 0) {
        for (int i = 0; i < 10; i++) {
            if (joker_cards[i] == 0) {
                available_jokers[count++] = i;
            }
        }
        
        // If all jokers are already active, just return
        if (count == 0) {
            return;
        }
    }
    
    // Select a random joker from the available pool
    int selected_joker = available_jokers[rand() % count];
    
    // Give the joker to the player
    joker_cards[selected_joker] = 1;
}

/*
 * Smiley Face (Uncommon) ~ Plus 5 Mult Per Face Card Played
 * Adds 5 to multiplier for each face card (J, Q, K) in the played hand
 */
int apply_smiley_face_joker(Card *hand, int num_cards, int base_multiplier)
{
    if (joker_cards[0] == 0) { // Check if this joker is not active
        return base_multiplier;
    }
    
    int additional_mult = 0;
    
    for (int i = 0; i < num_cards; i++) {
        int rank = RANK(hand[i]);
        // J=9, Q=10, K=11 (ranks 0-12)
        if (rank >= 9 && rank <= 11) {
            additional_mult += 5;
        }
    }
    
    return base_multiplier + additional_mult;
}

/*
 * Even Steven (Uncommon) ~ Plus 4 Mult Per Even Rank Card Played
 * Adds 4 to multiplier for each even-ranked card (2,4,6,8,10,Q) in the played hand
 */
int apply_even_steven_joker(Card *hand, int num_cards, int base_multiplier)
{
    if (joker_cards[1] == 0) { // Check if this joker is not active
        return base_multiplier;
    }
    
    int additional_mult = 0;
    
    for (int i = 0; i < num_cards; i++) {
        int rank = RANK(hand[i]);
        // Even ranks: 0(2), 2(4), 4(6), 6(8), 8(10)
        if (rank % 2 == 0 && rank < 9) {
            additional_mult += 4;
        }
    }
    
    return base_multiplier + additional_mult;
}

/*
 * Odd Todd (Uncommon) ~ Plus 31 Chip per Odd Rank Card Played
 * Adds 31 chips for each odd-ranked card (A,3,5,7,9) in the played hand
 */
int apply_odd_todd_joker(Card *hand, int num_cards, int base_chips)
{
    if (joker_cards[2] == 0) { // Check if this joker is not active
        return base_chips;
    }
    
    int additional_chips = 0;
    
    for (int i = 0; i < num_cards; i++) {
        int rank = RANK(hand[i]);
        // Odd ranks: 1(3), 3(5), 5(7), 7(9), 12(A is odd in value)
        if ((rank % 2 == 1 || rank == 12) && rank != 9 && rank != 11) {
            additional_chips += 31;
        }
    }
    
    return base_chips + additional_chips;
}

/*
 * Suit Bundler (Rare) - Spade Equals Clubs And Diamonds Equal Hearts
 * Makes spades equal to clubs and diamonds equal to hearts for hand evaluation
 * This changes the effective suit for evaluation, especially for flushes
 */
Card transform_card_suit_bundler(Card card)
{
    if (joker_cards[3] == 0) { // Check if this joker is not active
        return card;
    }
    
    if (card == 0 || card == 0xFF) return card; // Skip invalid cards
    
    int rank = RANK(card);
    int suit = SUIT(card);
    
    // Transform suits: Spades(2) -> Clubs(0), Diamonds(1) -> Hearts(3)
    if (suit == 2) { // Spades -> Clubs
        return MAKE_CARD(rank, 0);
    } else if (suit == 1) { // Diamonds -> Hearts
        return MAKE_CARD(rank, 3);
    }
    
    return card; // No change for clubs and hearts
}

/*
 * Apply Suit Bundler to an entire hand 
 * Used before hand evaluation to transform all cards
 */
void apply_suit_bundler_joker(Card *hand, int num_cards, Card *transformed_hand)
{
    for (int i = 0; i < num_cards; i++) {
        transformed_hand[i] = transform_card_suit_bundler(hand[i]);
    }
}

/*
 * The One (Legendary) ~ Next 7 Hands are Royal Flushes
 * Sets a counter to transform the next 7 hands into royal flushes
 */
int the_one_hands_remaining = 0;

void activate_the_one_joker()
{
    if (joker_cards[4] == 0) { // Check if this joker is not active
        return;
    }
    
    the_one_hands_remaining = 7;
    joker_cards[4] = 0; // Consume the joker once activated
}

/*
 * Check if The One should transform the current hand
 * Returns 1 if the hand should be treated as a royal flush
 */
int check_the_one_joker()
{
    if (the_one_hands_remaining > 0) {
        the_one_hands_remaining--;
        return 1; // Treat as royal flush
    }
    return 0;
}

/*
 * Blue Dot (Common) ~ Plus 50 Chip
 * Adds a flat 50 chips to any hand
 */
int apply_blue_dot_joker(int base_chips)
{
    if (joker_cards[5] == 0) { // Check if this joker is not active
        return base_chips;
    }
    
    return base_chips + 50;
}

/*
 * Red Dot (Common) ~ Plus 10 Mult
 * Adds a flat 10 to the multiplier of any hand
 */
int apply_red_dot_joker(int base_multiplier)
{
    if (joker_cards[6] == 0) { // Check if this joker is not active
        return base_multiplier;
    }
    
    return base_multiplier + 10;
}

/*
 * Green Check (Rare) ~ Need Only 25 Percent to Pass Round (One Time Use but will 
 * activate when no hands left)
 * Reduces target score to 25% when no hands are left and the target wasn't reached
 */
int green_check_used = 0;
int apply_green_check_joker(int target_score, int current_score, int hands_left)
{
    if (joker_cards[7] == 0 || green_check_used) { // Check if joker inactive or already used
        return 0; // Not applicable
    }
    
    // Activate when no hands left and score is below target
    if (hands_left == 0 && current_score < target_score) {
        int reduced_target = target_score / 4; // 25% of original target
        
        // If player has reached the reduced target
        if (current_score >= reduced_target) {
            green_check_used = 1; // Mark as used
            joker_cards[7] = 0;   // Deactivate the joker
            return 1; // Indicate success
        }
    }
    
    return 0; // Not activated yet
}

/*
 * Stevie Dott (Rare) ~ Even Sum Get 40 Mult Odd Sum Get 2X On Played Card Chip
 * Changes multiplier or doubles card chips based on sum of card ranks
 */
void apply_stevie_dott_joker(Card *hand, int num_cards, int *multiplier, int *card_chips)
{
    if (joker_cards[8] == 0) { // Check if this joker is not active
        return;
    }
    
    // Calculate sum of card ranks
    int rank_sum = 0;
    for (int i = 0; i < num_cards; i++) {
        rank_sum += RANK(hand[i]);
    }
    
    if (rank_sum % 2 == 0) { // Even sum
        *multiplier = 40; // Set multiplier to 40
    } else { // Odd sum
        *card_chips *= 2; // Double the card chips
    }
}

/*
 * Helper function to apply all active joker effects to a hand's score
 * This would be called after normal hand evaluation
 */
void apply_all_joker_effects(Card *hand, int num_cards, enum HandType *hand_type, 
                            int *chips, int *multiplier, int target_score, 
                            int current_score, int hands_left)
{
    // Make a copy of the hand for potential transformations
    Card transformed_hand[5] = {0};
    memcpy(transformed_hand, hand, num_cards * sizeof(Card));
    
    // Apply suit bundler transformation
    if (joker_cards[3]) {
        apply_suit_bundler_joker(hand, num_cards, transformed_hand);
        // Re-evaluate hand with transformed suits
        *hand_type = evaluate_hand(transformed_hand, num_cards);
    }
    
    // Apply The One (royal flush override)
    if (check_the_one_joker()) {
        *hand_type = ROYAL_FLUSH;
    }
    
    // Get base hand value after possible transformations
    HandValue hand_value = get_hand_value(*hand_type);
    *chips = hand_value.chips;
    *multiplier = hand_value.multiplier;
    
    // Apply joker effects that modify chips
    *chips = apply_odd_todd_joker(hand, num_cards, *chips);
    *chips = apply_blue_dot_joker(*chips);
    
    // Apply joker effects that modify multiplier
    *multiplier = apply_smiley_face_joker(hand, num_cards, *multiplier);
    *multiplier = apply_even_steven_joker(hand, num_cards, *multiplier);
    *multiplier = apply_red_dot_joker(*multiplier);
    
    // Calculate card-specific chips
    int hand_specific_chips = calculate_hand_type_specific_chips();
    *chips += hand_specific_chips;
    
    // Apply Stevie Dott effect (may modify multiplier or hand_specific_chips)
    apply_stevie_dott_joker(hand, num_cards, multiplier, &hand_specific_chips);
    
    // Check if Green Check should be applied
	apply_green_check_joker(target_score, current_score, hands_left);
}

/*
 * GAME LOOP
 */
void game_loop_vga() {
    struct libusb_device_handle *controller;
    uint8_t ep;
    unsigned char report[REPORT_LEN];
    int transferred;
    
    // Initialize controller
    controller = opencontroller(&ep);
    if (!controller) {
        printf("Failed to open controller\n");
        return;
    }

	while (!game_play) {
		// Input processing
        int r = libusb_interrupt_transfer(controller, ep,
			report, REPORT_LEN,
			&transferred, 100); // 100ms timeout
		
		if (r == 0 && transferred > 0) {
			process_controller_input(report);
		}

	}

    // Initialize debounce
    initialize_debounce();
    
    // Set up initial game state
    hard_reset();

    // Initialize joker display (empty)
    uint8_t empty_jokers[5] = {10, 10, 10, 10, 10}; // 10 is the code for empty
    draw_jokers(empty_jokers);
    
    draw_hand(index_for_tiles_cards);
    
    // Get initial game info
    char blind_name[20];
    int target_score;
    int cumulative_score = 0;
    get_current_blind_info(blind_name, &target_score);
    
    // Initialize VGA display with game state
    draw_target_score(get_target_score_vga(target_score));
    draw_round_score(get_round_score_vga(cumulative_score));
    draw_hand_type("POKER HAND TYPE");
    draw_chip(get_chip_vga(0));
    draw_mult(get_mult_vga(0));
    draw_hands_left(get_hands_left_vga()[0]);
    draw_discards(get_discards_left_vga()[0]);
    draw_cards_in_deck(get_cards_in_deck_vga());
    draw_ante(get_ante_vga()[0]);
    draw_round(get_round_vga()[0]);
    draw_selected(get_selected_cards_array());
    
    // Draw initial cursor
    cursor_position = get_cursor_position();
    draw_cursor(cursor_position);
    
	while (game_play) {

		// Input processing
		int r = libusb_interrupt_transfer(controller, ep,
									report, REPORT_LEN,
									&transferred, 100); // 100ms timeout
		
		if (r == 0 && transferred > 0) {
			// Save previous game state for change detection
			update_clear_cursor_position(); // Save current cursor for clearing
			int previous_hands = hands_remaining;
			int previous_discards = discards_remaining;
			int previous_num_selected = num_selected_cards;
			
			// Process controller input
			int old_button = last_button;
			process_controller_input(report);
			
			// Get current cursor position and update cursor display if it moved
			cursor_position = get_cursor_position();
			clear_cursor_position = get_clear_cursor_position();
			
			if (cursor_position != clear_cursor_position) {
				clear_cursor(clear_cursor_position);
				draw_cursor(cursor_position);
			}
			
			// If selections changed, update hand evaluation display
			if (num_selected_cards != previous_num_selected) {
				// Get current hand evaluation
				enum HandType hand_type = evaluate_selected_cards();
				HandValue hand_value = get_hand_value(hand_type);
				
				// Update the display
				draw_hand_type((char*)get_hand_name_vga(hand_type));
				draw_chip(get_chip_vga(hand_value.chips));
				draw_mult(get_mult_vga(hand_value.multiplier));
				
				// Update selected cards display
				draw_selected(get_selected_cards_array());
			}
			if (num_selected_cards == 0) {
				draw_hand_type("POKER HAND TYPE");
				draw_chip(get_chip_vga(0));
				draw_mult(get_mult_vga(0));
			}
			
			// Check if a hand was just played (START button pressed)
			if (last_button == 8 && old_button != 8 && previous_num_selected > 0 && 
				hands_remaining < previous_hands) {
				
				// Calculate the score for the hand that was just played
				enum HandType hand_type = evaluate_hand(played_hand, previous_num_selected);
				HandValue hand_value = get_hand_value(hand_type);
				
				// Calculate card-specific chips using the played hand instead of selected_cards
				int hand_specific_chips = calculate_played_hand_chips(played_hand, previous_num_selected, hand_type);

				// Apply joker effects to the hand
				int modified_chips = hand_value.chips;
				int modified_multiplier = hand_value.multiplier;

				// Apply all active joker effects to the hand
				apply_all_joker_effects(
					played_hand, 
					previous_num_selected, 
					&hand_type,  // Hand type might be upgraded by jokers
					&modified_chips, 
					&modified_multiplier,
					target_score,
					cumulative_score,
					hands_remaining
				);

				// Use the modified values for scoring
				int total_chip_value = modified_chips + hand_specific_chips;
				int score = total_chip_value * modified_multiplier;
				
				// Add score to running total
				cumulative_score += score;
				
				// Display played hand on the VGA
				uint8_t played_card_indices[5] = {52, 52, 52, 52, 52}; // Initialize all to "empty"
				for (int i = 0; i < previous_num_selected; i++) {
					// Convert each card to its deck index
					int rank = RANK(played_hand[i]);
					int suit = SUIT(played_hand[i]);
					played_card_indices[i] = suit * 13 + rank;
				}

				draw_played_cards(played_card_indices);

				// Create a temporary array to modify for display
				uint8_t updated_hand_indices[8];
				for (int i = 0; i < draw_amount; i++) {
					updated_hand_indices[i] = index_for_tiles_cards[i];
				}

				// Mark played cards as empty (52) in the display array
				for (int i = 0; i < 5; i++) {
					for (int j = 0; j < 8; j++) {
						if (played_card_indices[i] == index_for_tiles_cards[j]) {
							updated_hand_indices[j] = 52; // Set to empty
							break;
						}
					}
				}
				// Update the hand display with empty slots for played cards
				draw_hand(updated_hand_indices);
				
				// Update the VGA display with new score and game state
				draw_round_score(get_round_score_vga(cumulative_score));
				draw_hands_left(get_hands_left_vga()[0]);
				draw_cards_in_deck(get_cards_in_deck_vga());
				
				// Also show the hand type that was just played
				draw_hand_type((char*)get_hand_name_vga(hand_type));
				draw_chip(get_chip_vga(total_chip_value));
				draw_mult(get_mult_vga(modified_multiplier));
				
				// Pause briefly to show the played hand
				sleep(2);
				clear_table((uint8_t) previous_num_selected);
				
				// Draw replacement cards
				draw_replacement_cards();
				drawn_to_index();
				draw_hand(index_for_tiles_cards);
				
				// Reset cursor position
				clear_cursor(cursor_position);
				cursor = 0;
				cursor_position = get_cursor_position();
				draw_cursor(cursor_position);
				
				// Reset selected cards display
				draw_selected(get_selected_cards_array());
				
				// Check win condition
				if (check_win_condition(cumulative_score)) {
					// Display win message
					draw_hand_type("  YOU WIN    ");
					sleep(2);
					// Award a New Possible Joker
					joker_drop();

					// Refresh joker display
					uint8_t joker_display[5] = {10, 10, 10, 10, 10}; // Initialize empty
					int joker_count = 0;
					for (int i = 0; i < 10 && joker_count < 5; i++) {
						if (joker_cards[i] == 1) {
							joker_display[joker_count++] = i;
						}
					}
					draw_jokers(joker_display);
					
					// Advance to next blind/ante
					advance_game_state();
					if (game_play) {
						cumulative_score = 0;
						
						// Update display for new game state
						get_current_blind_info(blind_name, &target_score);
						draw_target_score(get_target_score_vga(target_score));
						draw_round_score(get_round_score_vga(cumulative_score));
						draw_hands_left(get_hands_left_vga()[0]);
						draw_discards(get_discards_left_vga()[0]);
						draw_cards_in_deck(get_cards_in_deck_vga());
						draw_ante(get_ante_vga()[0]);
						draw_round(get_round_vga()[0]);
						
						// Reset hand evaluation display
						draw_hand_type("POKER HAND TYPE");
						draw_chip(get_chip_vga(0));
						draw_mult(get_mult_vga(0));
						
						// Draw new hand
						drawn_to_index();
						draw_hand(index_for_tiles_cards);
						
						// Reset cursor and selection display
						clear_cursor(cursor_position);
						cursor = 0;
						cursor_position = get_cursor_position();
						draw_cursor(cursor_position);
						draw_selected(get_selected_cards_array());
					}
				}
			}
			
			// Check if cards were discarded (SELECT button pressed)
			if (last_button == 7 && old_button != 7 && previous_discards > discards_remaining) {
				// Draw replacement cards
				draw_replacement_cards();
				drawn_to_index();
				draw_hand(index_for_tiles_cards);
				
				// Update display
				draw_discards(get_discards_left_vga()[0]);
				draw_cards_in_deck(get_cards_in_deck_vga());
				
				// Reset hand evaluation display
				draw_hand_type("POKER HAND TYPE");
				draw_chip(get_chip_vga(0));
				draw_mult(get_mult_vga(0));
				
				// Reset cursor and selection display
				clear_cursor(cursor_position);
				cursor = 0;
				cursor_position = get_cursor_position();
				draw_cursor(cursor_position);
				draw_selected(get_selected_cards_array());
			}
		}
		
		// Check if game is over (out of hands and didn't win)
		if (hands_remaining == 0) {
			// Check if Green Check would save us (reduces target to 25%)
			if (joker_cards[7] == 1 && !green_check_used) {
				// Get current target score
				get_current_blind_info(blind_name, &target_score);
				int reduced_target = target_score / 4;
				
				if (cumulative_score >= reduced_target) {
					// Green Check activates!
					joker_cards[7] = 0; // Consume the joker
					green_check_used = 1;
					
					// Display Green Check activation message
					draw_hand_type("GREEN CHECK WIN!");
					sleep(2);
					
					// Update joker display to remove green check
					uint8_t joker_display[5] = {10, 10, 10, 10, 10}; // Initialize empty
					int joker_count = 0;
					for (int i = 0; i < 10 && joker_count < 5; i++) {
						if (joker_cards[i] == 1) {
							joker_display[joker_count++] = i;
						}
					}
					draw_jokers(joker_display);
					
					// Award a possible new joker
					joker_drop();
					
					// Refresh joker display
					joker_count = 0;
					for (int i = 0; i < 10 && joker_count < 5; i++) {
						if (joker_cards[i] == 1) {
							joker_display[joker_count++] = i;
						}
					}
					draw_jokers(joker_display);
					
					// Handle win as if target was reached
					advance_game_state();
					if (game_play) {
						cumulative_score = 0;
						
						// Update display for new game state
						get_current_blind_info(blind_name, &target_score);
						draw_target_score(get_target_score_vga(target_score));
						draw_round_score(get_round_score_vga(cumulative_score));
						draw_hands_left(get_hands_left_vga()[0]);
						draw_discards(get_discards_left_vga()[0]);
						draw_cards_in_deck(get_cards_in_deck_vga());
						draw_ante(get_ante_vga()[0]);
						draw_round(get_round_vga()[0]);

						// Reset hand evaluation display
						draw_hand_type("POKER HAND TYPE");
						draw_chip(get_chip_vga(0));
						draw_mult(get_mult_vga(0));

						// Draw new hand
						drawn_to_index();
						draw_hand(index_for_tiles_cards);
					}
				}
				else if (!check_win_condition(cumulative_score)) {
					// Game over logic
					draw_hand_type("  GAME OVER   ");
					sleep(1);
					// CALL GAME OVER
					game_over();
				}
			}
			else if (!check_win_condition(cumulative_score)) {
				// Normal game over without Green Check
				draw_hand_type("  GAME OVER   ");
				sleep(1);
				// CALL GAME OVER
				game_over();
			}
		}

		// Small delay to prevent maxing out the CPU
		usleep(10000); // 10ms delay
	}

    // Clean up
    libusb_close(controller);
    libusb_exit(NULL);
}

/* 
 * Main Method
 */
int main()
{
    static const char filename[] = "/dev/vga_poker";
    printf("VGA tiles Userspace program started\n");
 
    if ( (vga_poker_fd = open(filename, O_RDWR)) == -1) {
      fprintf(stderr, "could not open %s\n", filename);
      return -1;
    }

    /* 1) load all the on‐disk graphics into FPGA memory */
    load_palette("palette.hex");
    load_tileset("tileset.hex");
    load_tilemap("256-title.hex");
    load_deck("256-cards.hex");
    load_jokers("256-Jokers.hex");

	while(1) {
    	game_loop_vga();
		load_tilemap("256-title.hex");
	}

    close(vga_poker_fd);
	return 0;
}
