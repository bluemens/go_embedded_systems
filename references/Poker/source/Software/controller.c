#include "controller.h"
#include <libusb-1.0/libusb.h>
#include <stdio.h>
#include <stdlib.h>

/* Vendor / product IDs for the DragonRise Inc. USB Gamepad          */
#define DRAGONRISE_VID  0x0079
#define DRAGONRISE_PID  0x0011

struct libusb_device_handle *opencontroller(uint8_t *endpoint_address)
{
    libusb_device       **devs;
    struct libusb_device_handle *handle = NULL;
    struct libusb_device_descriptor desc;
    ssize_t               ndevs;

    if (libusb_init(NULL) < 0) {
        perror("libusb_init");
        return NULL;
    }

    ndevs = libusb_get_device_list(NULL, &devs);
    if (ndevs < 0) {
        perror("libusb_get_device_list");
        return NULL;
    }

    /* enumerate every device ----------------------------------------- */
    for (ssize_t d = 0; d < ndevs; ++d) {
        libusb_device *dev = devs[d];

        if (libusb_get_device_descriptor(dev, &desc) != 0)
            continue;

        if (desc.idVendor  != DRAGONRISE_VID ||
            desc.idProduct != DRAGONRISE_PID)
            continue;                                   /* not our pad   */

        /* walk its interfaces ---------------------------------------- */
        struct libusb_config_descriptor *cfg;
        libusb_get_config_descriptor(dev, 0, &cfg);

        for (uint8_t i = 0; i < cfg->bNumInterfaces; ++i)
            for (uint8_t a = 0; a < cfg->interface[i].num_altsetting; ++a) {

                const struct libusb_interface_descriptor *ifd =
                    &cfg->interface[i].altsetting[a];

                if (ifd->bInterfaceClass    == LIBUSB_CLASS_HID &&
                    ifd->bInterfaceSubClass == 0x00 &&          /* generic */
                    ifd->bInterfaceProtocol == 0x00) {          /* none    */

                    if (libusb_open(dev, &handle) != 0)
                        break;

                    if (libusb_kernel_driver_active(handle, i))
                        libusb_detach_kernel_driver(handle, i);

                    libusb_claim_interface(handle, i);

                    *endpoint_address = ifd->endpoint[0].bEndpointAddress;
                    libusb_free_config_descriptor(cfg);
                    libusb_free_device_list(devs, 1);
                    return handle;                   /* success! */
                }
            }

        libusb_free_config_descriptor(cfg);
    }

    libusb_free_device_list(devs, 1);
    libusb_exit(NULL);
    return NULL;                                       /* not found */
}
