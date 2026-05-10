/**
 * @file test_joypad_input.c
 * @brief Test file for gamepad input device driver
 *
 * This file provides testing functionality for the gamepad input device driver,
 * including tests for initialization, button detection, and resource cleanup.
 */

#include "../include/joypad_input.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>

// Global variable for interrupt handling
volatile int keep_running = 1;

/**
 * @brief Handle CTRL+C signal to gracefully end the test
 */
void handle_signal(int sig) {
    if (sig == SIGINT) {
        printf("Interrupt signal received, exiting...\n");
        keep_running = 0;
    }
}

/**
 * @brief Test gamepad connection status
 */
void test_joypad_connection() {
    printf("===== Gamepad Connection Test =====\n");
    
    for (int i = 0; i < 2; i++) {
        if (is_joypad_connected(i)) {
            printf("Player %d gamepad: Connected\n", i + 1);
        } else {
            printf("Player %d gamepad: Not connected\n", i + 1);
            printf("Default device path: %s\n", get_default_joypad_path(i));
        }
    }
    printf("\n");
}

/**
 * @brief Test gamepad button status
 */
void test_joypad_buttons() {
    printf("===== Gamepad Button Status Test =====\n");
    printf("Please press gamepad buttons, the program will display button status...\n");
    printf("Press Ctrl+C to stop the test\n\n");
    
    // Register signal handler
    signal(SIGINT, handle_signal);
    
    while (keep_running) {
        // Clear screen
        printf("\033[H\033[J");
        printf("Gamepad Button Status Test (Ctrl+C to exit)\n");
        printf("-------------------------\n");
        
        for (int player = 0; player < 2; player++) {
            if (is_joypad_connected(player)) {
                printf("Player %d gamepad status:\n", player + 1);
                printf("  D-pad: Up[%s] Down[%s] Left[%s] Right[%s]\n",
                       get_joypad_button_state(player, JOYPAD_BTN_UP) ? "Pressed" : "Released",
                       get_joypad_button_state(player, JOYPAD_BTN_DOWN) ? "Pressed" : "Released",
                       get_joypad_button_state(player, JOYPAD_BTN_LEFT) ? "Pressed" : "Released",
                       get_joypad_button_state(player, JOYPAD_BTN_RIGHT) ? "Pressed" : "Released");
                
                printf("  Buttons: A[%s] B[%s] X[%s] Y[%s]\n",
                       get_joypad_button_state(player, JOYPAD_BTN_A) ? "Pressed" : "Released",
                       get_joypad_button_state(player, JOYPAD_BTN_B) ? "Pressed" : "Released",
                       get_joypad_button_state(player, JOYPAD_BTN_X) ? "Pressed" : "Released",
                       get_joypad_button_state(player, JOYPAD_BTN_Y) ? "Pressed" : "Released");
                
                // Display current action
                game_action_t action = get_player_action(player);
                printf("  Current action: ");
                switch(action) {
                    case ACTION_NONE:
                        printf("No action\n");
                        break;
                    case ACTION_MOVE_LEFT:
                        printf("Moving left\n");
                        break;
                    case ACTION_MOVE_RIGHT:
                        printf("Moving right\n");
                        break;
                    case ACTION_JUMP:
                        printf("Jumping\n");
                        break;
                    default:
                        printf("Unknown action\n");
                }
            } else {
                printf("Player %d gamepad not connected\n", player + 1);
            }
            printf("\n");
        }
        
        // Delay 100 milliseconds
        usleep(100000);
    }
}

/**
 * @brief Test dynamic gamepad connection
 */
void test_joypad_connection_change() {
    printf("===== Dynamic Gamepad Connection Test =====\n");
    
    char device_path[256];
    int player_index;
    
    printf("Enter gamepad device path (e.g. /dev/input/event0): ");
    scanf("%255s", device_path);
    
    printf("Enter player number to connect (0 or 1): ");
    scanf("%d", &player_index);
    
    if (insert_joypad(device_path, player_index) == 0) {
        printf("Successfully connected gamepad for player %d\n", player_index + 1);
    } else {
        printf("Failed to connect gamepad for player %d\n", player_index + 1);
    }
    printf("\n");
}

/**
 * @brief Main function
 */
int main() {
    printf("=================================\n");
    printf("Gamepad Input Device Driver Test\n");
    printf("=================================\n\n");
    
    // Initialize gamepad input module
    if (input_handler_init() != 0) {
        printf("Failed to initialize gamepad input module!\n");
        return -1;
    }
    
    // Test gamepad connection status
    test_joypad_connection();
    
    // Test dynamic gamepad connection (optional)
    char choice;
    printf("Do you want to test dynamic gamepad connection? (y/n): ");
    scanf(" %c", &choice);
    if (choice == 'y' || choice == 'Y') {
        test_joypad_connection_change();
    }
    
    // Test gamepad button status
    choice = 0;
    printf("Do you want to test gamepad button status? (y/n): ");
    scanf(" %c", &choice);
    if (choice == 'y' || choice == 'Y') {
        test_joypad_buttons();
    }
    
    // Clean up gamepad input module
    input_handler_cleanup();
    printf("Test completed, gamepad input module cleaned up\n");
    
    return 0;
} 