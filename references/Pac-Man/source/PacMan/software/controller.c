#include "controller.h"
#include <libusb-1.0/libusb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct controller_list open_controller() {
    printf("Searching for USB controller...\n");

    struct controller_list device;
    libusb_device **devs;
    struct libusb_device_descriptor desc;
    struct libusb_device_handle *controller = NULL;
    ssize_t num_devs;

    if (libusb_init(NULL) != 0) {
        printf("\nERROR: libusb failed to boot");
        exit(1);
    }

    if ((num_devs = libusb_get_device_list(NULL, &devs)) < 0) {
        printf("\nERROR: no USB devices found");
        exit(1);
    }

    for (int i = 0; i < num_devs; i++) {
        libusb_device *dev = devs[i];

        if (libusb_get_device_descriptor(dev, &desc) < 0)
            continue;

        if (desc.idProduct == 0x11) {
            struct libusb_config_descriptor *config;
            if ((libusb_get_config_descriptor(dev, 0, &config)) < 0)
                continue;

            const struct libusb_interface_descriptor *inter = config->interface[0].altsetting;
            if (libusb_open(dev, &controller) != 0)
                continue;

            if (libusb_kernel_driver_active(controller, 0))
                libusb_detach_kernel_driver(controller, 0);
            libusb_set_auto_detach_kernel_driver(controller, 0);

            if (libusb_claim_interface(controller, 0) != 0)
                continue;

            device.device1 = controller;
            device.device1_addr = inter->endpoint[0].bEndpointAddress;
            libusb_free_device_list(devs, 1);

            printf("Controller connected.\n");
            return device;
        }
    }

    printf("ERROR: couldn't find a controller.\n");
    exit(1);
}

void detect_presses(struct controller_pkt pkt, char *buttons, int mode) {
    char vals[] = "LRUDAXY";
    if (mode == 1) {
        strcpy(buttons, "11111");
    } else {
        strcpy(buttons, "_____");
    }

    if (pkt.dir_x == 0x00) buttons[0] = vals[0];  // Left
    if (pkt.dir_x == 0xff) buttons[1] = vals[1];  // Right
    if (pkt.dir_y == 0x00) buttons[2] = vals[2];  // Up
    if (pkt.dir_y == 0xff) buttons[3] = vals[3];  // Down

    if ((pkt.ab & 0x20)) {
        buttons[4] = vals[4]; // A pressed
    }
    if ((pkt.ab & 0x10)) {
        buttons[5] = vals[5]; // X pressed
    }
//     if ((pkt.ab & 0x08)) {
//         buttons[6] = vals[6]; // L pressed
//     }
        // if ((pkt.rl & 0x20)) {
        //         buttons[6] = vals[6]; // L pressed
        // }
}

void *listen_controller(void *arg) {
    struct args_list *args_p = arg;
    struct args_list args = *args_p;
    struct controller_pkt pkt;
    int transferred;
    int size = sizeof(pkt);
    char buttons[6] = "_____";

    while (1) {
        libusb_interrupt_transfer(
            args.devices.device1,
            args.devices.device1_addr,
            (unsigned char *)&pkt,
            size,
            &transferred,
            0
        );

        if (transferred == 7) {
            detect_presses(pkt, buttons, args.mode);
            strcpy(args.buttons, buttons);
            if (args.print) {
                printf("%s\n", args.buttons);
            }
        }
    }

    return NULL;
}