/**
 * @file joypad_input.h
 * @brief Interface definition for the Joypad input handling module
 *
 * This header file defines the interface for using Joypad (game controller)
 * as an input device, including functionality for initialization, cleanup,
 * and connecting new controller devices.
 * Specifically supports the classic 8-button joypad layout.
 */

#ifndef JOYPAD_INPUT_H
#define JOYPAD_INPUT_H

/* Classic joypad button definitions */
/* Direction buttons on the left side (D-pad) */
#define JOYPAD_BTN_UP 0    // Up direction button
#define JOYPAD_BTN_DOWN 1  // Down direction button
#define JOYPAD_BTN_LEFT 2  // Left direction button
#define JOYPAD_BTN_RIGHT 3 // Right direction button

/* Function buttons on the right side */
#define JOYPAD_BTN_A 4 // A button
#define JOYPAD_BTN_B 5 // B button
#define JOYPAD_BTN_X 6 // X button
#define JOYPAD_BTN_Y 7 // Y button

/* Game action constant definition */
typedef enum
{
    ACTION_NONE = 0,       // No action
    ACTION_MOVE_LEFT = 1,  // Move left
    ACTION_MOVE_RIGHT = 2, // Move right
    ACTION_JUMP = 3        // Jump
} game_action_t;

/**
 * @brief Connect a new player joypad device
 * Attempts to open and connect a joypad device at the specified path
 *
 * @param device_path The joypad device path, e.g., "/dev/input/js0"
 * @param player_index The player index to bind to (0 for first player, 1 for second player)
 * @return 0 on success, -1 on failure
 */
int insert_joypad(const char *device_path, int player_index);

/**
 * @brief Get the default joypad device path for a player
 *
 * @param player_index The player index (0 for first player, 1 for second player)
 * @return The default joypad device path for the specified player
 */
const char *get_default_joypad_path(int player_index);

/**
 * @brief Check if a player's joypad is connected
 *
 * @param player_index The player index (0 for first player, 1 for second player)
 * @return 1 if connected, 0 if not connected
 */
int is_joypad_connected(int player_index);

/**
 * @brief Get the state of a specific button for a player
 *
 * @param player_index The player index (0 for first player, 1 for second player)
 * @param button_id The button ID (see JOYPAD_BTN_* constants)
 * @return 1 if pressed, 0 if not pressed
 */
int get_joypad_button_state(int player_index, int button_id);

game_action_t get_player_action(int player_index);

void input_handler_cleanup();
int input_handler_init();

#endif /* JOYPAD_INPUT_H */