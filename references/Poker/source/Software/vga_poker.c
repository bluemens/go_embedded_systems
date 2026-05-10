/* * Device driver for the VGA video generator
 *
 * A Platform device implemented using the misc subsystem
 *
 * Timothy Melendez + Stephen A. Edwards
 * Columbia University
 *
 * References:
 * Linux source: Documentation/driver-model/platform.txt
 *               drivers/misc/arm-charlcd.c
 * http://www.linuxforu.com/tag/linux-device-drivers/
 * http://free-electrons.com/docs/
 *
 * "make" to build
 * insmod vga_poker.ko
 *
 * Check code style with
 * checkpatch.pl --file --no-tree vga_poker.c
 */

 #include <linux/module.h>
 #include <linux/init.h>
 #include <linux/errno.h>
 #include <linux/version.h>
 #include <linux/kernel.h>
 #include <linux/platform_device.h>
 #include <linux/miscdevice.h>
 #include <linux/slab.h>
 #include <linux/io.h>
 #include <linux/of.h>
 #include <linux/of_address.h>
 #include <linux/fs.h>
 #include <linux/uaccess.h>
 #include "vga_poker.h"
 
 #define DRIVER_NAME "vga_poker"
 
 /* Device registers */
 #define TILEMAP_BASE   0x0000  /* 8 KiB: 0x0000– addressing */
 #define PALETTE_BASE   0x2000  /*  64 B:  0x2000–0x203F */
 #define TILESET_BASE   0x4000  /* 16 KiB: 0x4000–0x7FFF */

 /*
  * Information about our device
  */
 struct vga_poker_dev {
     struct resource res; /* Resource: our registers */
     void __iomem *virtbase; /* Where registers can be accessed in memory */
     vga_poker_arg_t background;
 } dev;
 

// setTile -> set tile -> 6 bits per entry 
// addr 0010 0000 0000 0000 -> 2000
// max addr    0011 1111 1111 1111 -> 3FFF
    // row, col
    // 0,0 = addr 0
    // 1,0 = addr 128  000001   0000000
    // 0,1 = addr 1   000000   0000001
    // addr = (row << 7) | col 
    // address is 13 bits worth
    // tile_id is 6 bits
    // writedata =  1 << 7 | tile_id
    // pack_addr = 1 << 14 | addr
 /* col < 80, row < 60, tile_id < 64 */
static void set_tile(vga_poker_tile_coords_t *tile_coords)
{
    int row;
    int col;
    int tile_id;
    unsigned short new_tile_addr;
    row = tile_coords->row;
    col = tile_coords->col;
    tile_id = tile_coords->tile_id;
    new_tile_addr =  TILEMAP_BASE + (row << 7 | col);
    iowrite8(tile_id, dev.virtbase + new_tile_addr);

}

//setPixels
// addr 0 -> set pixel color -> 4 bits per entry
// addr  1111 1111 1111 -> FFF
/* tile_id < 64, pixel_colors points to base of 64 pixels  */
//int *pixels = pixel_coors->pixels;
    // Pixel color is only 4 bits
    // addr is 12 bits
    // lower 6 bits of addr are for local pixel row, col (Y, X)
    // next 6 bits are tile_id
    // pixel_ram_addr =   tile_id << 6 | y << 3 | (x)
    //unsigned int pixel_ram_addr = tile_id << 6 | i;
    //iowrite8( , pixel_ram_addr)
static void set_pixels(vga_poker_pixels_t *pixel_coors)
{
    int tile_id;
    int i;
    tile_id = pixel_coors->tile_id;
    for (i = 0; i < 64; i++) {
        int color;
        int new_addr;
        color = pixel_coors->pixels[i];
        new_addr = TILESET_BASE + (tile_id << 6) + i;
        iowrite8(color, dev.virtbase + new_addr);
    }
}

static void set_palette(vga_poker_palette_t *palette)
{
    int base;

    if (palette->index >= 16) return;

    base = PALETTE_BASE + (palette->index << 2);
    /* write into creg[7:0], [15:8], [23:16] */
    iowrite8(palette->red,   dev.virtbase + base + 0);
    iowrite8(palette->green, dev.virtbase + base + 1);
    iowrite8(palette->blue,  dev.virtbase + base + 2);
    /* commit into palette[pl->index] */
    iowrite8(0, dev.virtbase + base + 3);
}



 /*
  * Handle ioctl() calls from userspace:
  * Read or write the segments on single digits.
  * Note extensive error checking of arguments
  */
 static long vga_poker_ioctl(struct file *f, unsigned int cmd, unsigned long arg)
 {
     vga_poker_arg_t vla;
 
     switch (cmd) {

    case VGA_POKER_SET_TILE:
         if (copy_from_user(&vla, (vga_poker_arg_t *) arg,
                    sizeof(vga_poker_arg_t)))
             return -EACCES;
         set_tile(&vla.tile_coords);
         break;
    case VGA_POKER_SET_PALETTE:
         if (copy_from_user(&vla, (vga_poker_arg_t *) arg,
                    sizeof(vga_poker_arg_t)))
             return -EACCES;
         set_palette(&vla.palette_t);
         break;
    case VGA_POKER_SET_PIXELS:
         if (copy_from_user(&vla, (vga_poker_arg_t *) arg,
                    sizeof(vga_poker_arg_t)))
             return -EACCES;
         set_pixels(&vla.pixel_coors);
         break;
     default:
         return -EINVAL;
     }
 
     return 0;
 }
 
 /* The operations our device knows how to do */
 static const struct file_operations vga_poker_fops = {
     .owner		= THIS_MODULE,
     .unlocked_ioctl = vga_poker_ioctl,
 };
 
 /* Information about our device for the "misc" framework -- like a char dev */
 static struct miscdevice vga_poker_misc_device = {
     .minor		= MISC_DYNAMIC_MINOR,
     .name		= DRIVER_NAME,
     .fops		= &vga_poker_fops,
 };
 
 /*
  * Initialization code: get resources (registers) and display
  * a welcome message
  */
 static int __init vga_poker_probe(struct platform_device *pdev)
 {
     int ret;
 
     /* Register ourselves as a misc device: creates /dev/vga_poker */
     ret = misc_register(&vga_poker_misc_device);
 
     /* Get the address of our registers from the device tree */
     ret = of_address_to_resource(pdev->dev.of_node, 0, &dev.res);
     if (ret) {
         ret = -ENOENT;
         goto out_deregister;
     }
 
     /* Make sure we can use these registers */
     if (request_mem_region(dev.res.start, resource_size(&dev.res),
                    DRIVER_NAME) == NULL) {
         ret = -EBUSY;
         goto out_deregister;
     }
 
     /* Arrange access to our registers */
     dev.virtbase = of_iomap(pdev->dev.of_node, 0);
     if (dev.virtbase == NULL) {
         ret = -ENOMEM;
         goto out_release_mem_region;
     }
 
     return 0;
 
 out_release_mem_region:
     release_mem_region(dev.res.start, resource_size(&dev.res));
 out_deregister:
     misc_deregister(&vga_poker_misc_device);
     return ret;
 }
 
 /* Clean-up code: release resources */
 static int vga_poker_remove(struct platform_device *pdev)
 {
     iounmap(dev.virtbase);
     release_mem_region(dev.res.start, resource_size(&dev.res));
     misc_deregister(&vga_poker_misc_device);
     return 0;
 }
 
 /* Which "compatible" string(s) to search for in the Device Tree */
 #ifdef CONFIG_OF
 static const struct of_device_id vga_poker_of_match[] = {
     { .compatible = "csee4840,vga_tiles-1.0" },
     {},
 };
 MODULE_DEVICE_TABLE(of, vga_poker_of_match);
 #endif
 
 /* Information for registering ourselves as a "platform" driver */
 static struct platform_driver vga_poker_driver = {
     .driver	= {
         .name	= DRIVER_NAME,
         .owner	= THIS_MODULE,
         .of_match_table = of_match_ptr(vga_poker_of_match),
     },
     .remove	= __exit_p(vga_poker_remove),
 };
 
 /* Called when the module is loaded: set things up */
static int __init vga_poker_init(void)
{
    pr_info(DRIVER_NAME ": init\n");
    return platform_driver_probe(&vga_poker_driver, vga_poker_probe);
}
 
 /* Calball when the module is unloaded: release resources */
static void __exit vga_poker_exit(void)
{
    platform_driver_unregister(&vga_poker_driver);
    pr_info(DRIVER_NAME ": exit\n");
}
 
 module_init(vga_poker_init);
 module_exit(vga_poker_exit);
 
 MODULE_LICENSE("GPL");
 MODULE_AUTHOR("Stephen A. Edwards, Columbia University");
 MODULE_DESCRIPTION("VGA ball driver");
 