/**
 * @file joypad_input.c
 * @brief Implementation of the Joypad controller input device driver
 *
 * This file provides the implementation for using game controllers (Joypads)
 * as input devices, supporting the connection of two controllers
 * to control two game characters.
 * Specifically supports the classic 8-button joypad layout.
 */

#include "../include/joypad_input.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/joystick.h>
#include <linux/input.h>
#include <sys/time.h>

/* Constants */
#define JOYPAD_1_DEVICE "/dev/input/event0" // First joypad device
#define JOYPAD_2_DEVICE "/dev/input/event1" // Second joypad device

/* Classic joypad button mapping - Direction buttons on the left side (D-pad) */
#define JOYPAD_BTN_UP 0    // Up direction button
#define JOYPAD_BTN_DOWN 1  // Down direction button
#define JOYPAD_BTN_LEFT 2  // Left direction button
#define JOYPAD_BTN_RIGHT 3 // Right direction button

/* Classic joypad button mapping - Function buttons on the right side */
#define JOYPAD_BTN_A 4 // A button
#define JOYPAD_BTN_B 5 // B button
#define JOYPAD_BTN_X 6 // X button
#define JOYPAD_BTN_Y 7 // Y button

/* Joypad state structure */
typedef struct
{
    int fd;         // Device file descriptor
    bool connected; // Connection status

    // Direction button states (left side four buttons)
    bool btn_up;
    bool btn_down;
    bool btn_left;
    bool btn_right;

    // Function button states (right side four buttons)
    bool btn_a;
    bool btn_b;
    bool btn_x;
    bool btn_y;

} joypad_state_t;

/* Global variables */
static joypad_state_t joypads[2]; // Support for up to two joypads

/**
 * @brief Initialize the Joypad input module
 * Attempts to connect to joypad devices and set them to non-blocking mode
 *
 * @return 0 on success, -1 on failure
 */
int input_handler_init()
{
    printf("Initializing Joypad Input Handler...\n");

    // Initialize joypad states
    for (int i = 0; i < 2; i++)
    {
        joypads[i].connected = false;
        joypads[i].btn_up = false;
        joypads[i].btn_down = false;
        joypads[i].btn_left = false;
        joypads[i].btn_right = false;
        joypads[i].btn_a = false;
        joypads[i].btn_b = false;
        joypads[i].btn_x = false;
        joypads[i].btn_y = false;
        joypads[i].fd = -1;
    }

    // Try to open the first joypad
    joypads[0].fd = open(JOYPAD_1_DEVICE, O_RDONLY | O_NONBLOCK);
    if (joypads[0].fd != -1)
    {
        joypads[0].connected = true;
        printf("Successfully connected first joypad (Player 1)\n");
    }
    else
    {
        printf("Could not connect first joypad, keyboard will be used as fallback\n");
    }

    // Try to open the second joypad
    joypads[1].fd = open(JOYPAD_2_DEVICE, O_RDONLY | O_NONBLOCK);
    if (joypads[1].fd != -1)
    {
        joypads[1].connected = true;
        printf("Successfully connected second joypad (Player 2)\n");
    }
    else
    {
        printf("Could not connect second joypad, keyboard will be used as fallback\n");
    }

    return 0;
}

/**
 * @brief Clean up the Joypad input module
 * Close joypad device files
 */
void input_handler_cleanup()
{
    printf("Cleaning up Joypad Input Handler...\n");

    // Close joypad devices
    for (int i = 0; i < 2; i++)
    {
        if (joypads[i].connected && joypads[i].fd != -1)
        {
            close(joypads[i].fd);
            joypads[i].connected = false;
        }
    }
}

/**
 * @brief Update Joypad state
 * Read joypad events and update status
 *
 * @param player_index Player index (0 or 1)
 */
static void update_joypad_state(int player_index)
{
    if (player_index < 0 || player_index > 1 || !joypads[player_index].connected)
    {
        return;
    }

    // Using input_event structure to read events
    struct input_event event;

    // Read all pending events
    while (read(joypads[player_index].fd, &event, sizeof(event)) > 0)
    {
        // Handle button events (type=1)
        if (event.type == 1)
        {
            switch (event.code)
            {
            case 288: // X button
                joypads[player_index].btn_x = (event.value != 0);
                break;
            case 289: // A button
                joypads[player_index].btn_a = (event.value != 0);
                break;
            case 290: // B button
                joypads[player_index].btn_b = (event.value != 0);
                break;
            case 291: // Y button
                joypads[player_index].btn_y = (event.value != 0);
                break;
            case 296: // Select button
                // Can add Select button handling here
                break;
            case 297: // Start button
                // Can add Start button handling here
                break;
            }
        }
        // Handle directional events (type=3)
        else if (event.type == 3)
        { // EV_ABS
            if (event.code == 0)
            { // X axis
                if (event.value == 0)
                { // Left button pressed
                    joypads[player_index].btn_left = true;
                    joypads[player_index].btn_right = false;
                }
                else if (event.value == 255)
                { // Right button pressed
                    joypads[player_index].btn_left = false;
                    joypads[player_index].btn_right = true;
                }
                else if (event.value == 127)
                { // Left and right buttons released
                    joypads[player_index].btn_left = false;
                    joypads[player_index].btn_right = false;
                }
            }
            else if (event.code == 1)
            { // Y axis
                if (event.value == 0)
                { // Up button pressed
                    joypads[player_index].btn_up = true;
                    joypads[player_index].btn_down = false;
                }
                else if (event.value == 255)
                { // Down button pressed
                    joypads[player_index].btn_up = false;
                    joypads[player_index].btn_down = true;
                }
                else if (event.value == 127 || event.value == 126)
                { // Up and down buttons released
                    joypads[player_index].btn_up = false;
                    joypads[player_index].btn_down = false;
                }
            }
        }
    }
}

/**
 * @brief Get the current game action for a player
 * Determine the current game action based on joypad state
 *
 * @param player_index Player index (0 for Fireboy, 1 for Watergirl)
 * @return The game action corresponding to the current input
 */
game_action_t get_player_action(int player_index)
{
    // Ensure valid player index
    if (player_index < 0 || player_index > 1)
    {
        return ACTION_NONE;
    }

    // If joypad is connected, update joypad state
    if (joypads[player_index].connected)
    {
        update_joypad_state(player_index);

        // Determine action based on joypad state
        // Jump has highest priority (using X button and up direction)
        if (joypads[player_index].btn_x || joypads[player_index].btn_up)
        {
            return ACTION_JUMP;
        }
        // Left/right movement (using direction buttons)
        else if (joypads[player_index].btn_left)
        {
            return ACTION_MOVE_LEFT;
        }
        else if (joypads[player_index].btn_right)
        {
            return ACTION_MOVE_RIGHT;
        }
    }

    // Default no action
    return ACTION_NONE;
}

/**
 * @brief Connect a new player joypad device
 * Attempts to open and connect a joypad device at the specified path
 *
 * @param device_path The joypad device path
 * @param player_index The player index to bind to
 * @return 0 on success, -1 on failure
 */
int insert_joypad(const char *device_path, int player_index)
{
    if (player_index < 0 || player_index > 1)
    {
        printf("Error: Invalid player index\n");
        return -1;
    }

    // If there's already a connected joypad, close it first
    if (joypads[player_index].connected && joypads[player_index].fd != -1)
    {
        close(joypads[player_index].fd);
        joypads[player_index].connected = false;
    }

    // Try to open the new joypad device
    joypads[player_index].fd = open(device_path, O_RDONLY | O_NONBLOCK);
    if (joypads[player_index].fd != -1)
    {
        joypads[player_index].connected = true;
        // Reset all button states
        joypads[player_index].btn_up = false;
        joypads[player_index].btn_down = false;
        joypads[player_index].btn_left = false;
        joypads[player_index].btn_right = false;
        joypads[player_index].btn_a = false;
        joypads[player_index].btn_b = false;
        joypads[player_index].btn_x = false;
        joypads[player_index].btn_y = false;

        printf("Successfully connected Player %d joypad\n", player_index + 1);
        return 0;
    }
    else
    {
        printf("Failed to connect Player %d joypad\n", player_index + 1);
        return -1;
    }
}

/**
 * @brief Get the default joypad device path for a player
 *
 * @param player_index Player index (0 for first player, 1 for second player)
 * @return The default joypad device path
 */
const char *get_default_joypad_path(int player_index)
{
    if (player_index == 0)
    {
        return JOYPAD_1_DEVICE;
    }
    else if (player_index == 1)
    {
        return JOYPAD_2_DEVICE;
    }
    else
    {
        return NULL; // Invalid player index
    }
}

/**
 * @brief Check if a player's joypad is connected
 *
 * @param player_index Player index (0 for first player, 1 for second player)
 * @return 1 if connected, 0 if not connected
 */
int is_joypad_connected(int player_index)
{
    if (player_index < 0 || player_index > 1)
    {
        return 0; // Invalid player index
    }

    return joypads[player_index].connected ? 1 : 0;
}

/**
 * @brief Get the state of a specific button for a player
 *
 * @param player_index Player index
 * @param button_id Button ID (see JOYPAD_BTN_* constants)
 * @return 1 if pressed, 0 if not pressed
 */
int get_joypad_button_state(int player_index, int button_id)
{
    if (player_index < 0 || player_index > 1 || !joypads[player_index].connected)
    {
        return 0;
    }

    // Update joypad state
    update_joypad_state(player_index);

    // Return state based on button ID
    switch (button_id)
    {
    case JOYPAD_BTN_UP:
        return joypads[player_index].btn_up ? 1 : 0;
    case JOYPAD_BTN_DOWN:
        return joypads[player_index].btn_down ? 1 : 0;
    case JOYPAD_BTN_LEFT:
        return joypads[player_index].btn_left ? 1 : 0;
    case JOYPAD_BTN_RIGHT:
        return joypads[player_index].btn_right ? 1 : 0;
    case JOYPAD_BTN_A:
        return joypads[player_index].btn_a ? 1 : 0;
    case JOYPAD_BTN_B:
        return joypads[player_index].btn_b ? 1 : 0;
    case JOYPAD_BTN_X:
        return joypads[player_index].btn_x ? 1 : 0;
    case JOYPAD_BTN_Y:
        return joypads[player_index].btn_y ? 1 : 0;
    default:
        return 0;
    }
}