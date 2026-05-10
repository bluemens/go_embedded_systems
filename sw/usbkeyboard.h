/*
 * USB HID keyboard via libusb-1.0
 *
 * Adapted from references/FlappyBird/source/4840-Flappy-Bird/FB_sw/usbkeyboard.h
 * (CSEE 4840 Spring 2025). Used verbatim except for the include guard rename.
 *
 * The keyboard report format is the standard USB HID Boot Keyboard Report:
 *   uint8_t modifiers;     (Ctrl/Shift/Alt/GUI bitmask)
 *   uint8_t reserved;
 *   uint8_t keycode[6];    (up to 6 simultaneously held keys, scan codes
 *                           per USB HID Usage Tables 1.5, Keyboard/Keypad
 *                           page 0x07; 0 = no key in this slot)
 */

#ifndef _USBKEYBOARD_H
#define _USBKEYBOARD_H

#include <stdint.h>
#include <libusb-1.0/libusb.h>

#define USB_HID_KEYBOARD_PROTOCOL 1

/* Modifier bits (packet.modifiers) */
#define USB_LCTRL  (1 << 0)
#define USB_LSHIFT (1 << 1)
#define USB_LALT   (1 << 2)
#define USB_LGUI   (1 << 3)
#define USB_RCTRL  (1 << 4)
#define USB_RSHIFT (1 << 5)
#define USB_RALT   (1 << 6)
#define USB_RGUI   (1 << 7)

struct usb_keyboard_packet {
    uint8_t modifiers;
    uint8_t reserved;
    uint8_t keycode[6];
};

/*
 * Find and open a USB keyboard device.
 *
 * Argument: pointer to space where the function will store the endpoint
 * address used for libusb_interrupt_transfer.
 *
 * Returns: opened libusb_device_handle on success, NULL on failure (no
 * keyboard found / open failed).
 */
extern struct libusb_device_handle *openkeyboard(uint8_t *endpoint_address);

#endif /* _USBKEYBOARD_H */
