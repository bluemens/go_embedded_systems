#ifndef _CONTROLLER_H
#define _CONTROLLER_H

#include <libusb-1.0/libusb.h>

#define USB_HID_KEYBOARD_PROTOCOL 1

struct usb_keyboard_packet {
  uint8_t modifiers;
  uint8_t reserved;
  uint8_t keycode[6];
};

/* Find and open a USB controller device.  Argument should point to
   space to store an endpoint address.  Returns NULL if no controller
   device was found. */
extern struct libusb_device_handle *opencontroller(uint8_t *);

#endif
