#include "vga_gpu.h"
#include "vga_marble.h"
#include <asm-generic/errno-base.h>
#include <linux/errno.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/miscdevice.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/version.h>

#define DRIVER_NAME "vga_gpu"

// redefine when register map is finalized
#define TEXTURE(x) (x)
#define TILEMAP(x) (x + 0x8000)
#define SPRITE(x) (x + 0xF000)
#define PALETTE(x) (x + 0xF400)
#define CONTROL(x) (x + 0xFF00)
#define SCROLL_X(x) (x + 0xFF04)
#define SCROLL_Y(x) (x + 0xFF06)
#define BG_COLOR(x) (x + 0xFF08)
#define H_COUNT(x) (x + 0xFF0C)
#define V_COUNT(x) (x + 0xFF0E)

/*
 * Information about our device
 */
struct VGAGPUDevice {
  struct resource res;    /* Resource: our registers */
  void __iomem *virtbase; /* Where registers can be accessed in memory */
} dev;

/* Write a texture */
static void write_texture(GPUTextureArgs *args) {
  u32 addr, data;
  u32 sliver, off;

  addr = (u32)(args->ind & 0b1111111111) << 5;

  for (sliver = 0; sliver < 8; sliver++) {
    data = 0;
    for (off = 0; off < 8; off++) {
      data <<= 4;
      data |= (u32)(args->palette_offs[sliver][7-off] & 0b1111);
    }

    if(args->ind == 0) 
    	pr_info(DRIVER_NAME ": WRITING TEXTURE DATA %d at %x\n", data, addr + (sliver << 2) );

    iowrite32(data, TEXTURE(dev.virtbase) + addr + (sliver << 2));
  }
}

/* Write a tile */
static void write_tile(GPUTileArgs *args) {
  u32 addr;
  u16 data;

  addr = ((u32)(args->layer & 0b1) << 13) | ((u32)(args->y & 0b111111) << 7) |
         ((u32)(args->x & 0b111111) << 1);
  data = ((u16)(args->v_flip & 0b1) << 15) | ((u16)(args->h_flip & 0b1) << 14) |
         ((u16)(args->priority & 0b1) << 13) |
         ((u16)(args->palette_ind & 0b111) << 10) |
         (u16)(args->texture_ind & 0b1111111111);

  //pr_info(DRIVER_NAME ": WRITING TILE DATA %hd AT ADDR %d", data, addr);

  iowrite16(data, TILEMAP(dev.virtbase) + addr);
}

/* Write a sprite */
static void write_sprite(GPUSpriteArgs *args) {
  u32 data;
  u32 addr;

  data = ((u32)(args->v_flip & 0b1) << 31) | ((u32)(args->h_flip & 0b1) << 30) |
         ((u32)(args->palette_ind & 0b111) << 27) |
         ((u32)(args->texture_ind & 0b1111111111) << 17) |
         ((u32)(args->y) << 9) | ((u32)(args->x & 0b111111111));

  addr = (u32)(args->ind) << 2;

  iowrite32(data, SPRITE(dev.virtbase) + addr);
}

/* Write a palette color */
static void write_palette(GPUPaletteArgs *args) {
  u32 data;
  u32 addr;

  data = (((u32)args->r) << 24) | ((u32)args->g << 16) | ((u32)args->b << 8) |
         ((u32)args->a);
  addr = ((u32)(args->ind & 0b111) << 6) | ((u32)(args->off & 0b1111) << 2);

  pr_info(DRIVER_NAME ": WRITING PALETTE DATA %d AT ADDR %d", data, addr);

  iowrite32(data, PALETTE(dev.virtbase) + addr);
}

/* Write to ctrl register */
static void write_ctrl(GPUCtrlArgs *args) {
  u32 data;

  data = ((u32)(args->force_blank & 0b1));

  iowrite32(data, CONTROL(dev.virtbase));
}

/* Write to scroll registers */
static void write_scroll_x(u16 *arg) {
  iowrite16(*arg & 0b111111111, SCROLL_X(dev.virtbase));
}

/* Write to scroll registers */
static void write_scroll_y(u16 *arg) {
  iowrite16(*arg & 0b111111111, SCROLL_Y(dev.virtbase));
}

/* Read horizontal count */
static u16 read_h_count(void) { return ioread16(H_COUNT(dev.virtbase)); }

/* Read vertical count */
static u16 read_v_count(void) { return ioread16(V_COUNT(dev.virtbase)); }

/*
 * Handle ioctl() calls from userspace:
 * Read or write the segments on single digits.
 * Note extensive error checking of arguments
 */
static long vga_gpu_ioctl(struct file *f, unsigned int cmd, unsigned long arg) {
  GPUArgs gpu_args;
  u16 arg_as_u16;
  // GPUTextureArgs texture_args;
  // GPUTileArgs tile_args;
  // GPUSpriteArgs sprite_args;
  // GPUPaletteArgs palette_args;
  // GPUCtrlArgs ctrl_args;

  switch (cmd) {
  case VGA_GPU_WRITE_TEXTURE:
    if (copy_from_user(&gpu_args.texture, (void __user *)arg,
                       sizeof(GPUTextureArgs)))
      return -EACCES;
    write_texture(&gpu_args.texture);
    break;
  case VGA_GPU_WRITE_TILE:
    if (copy_from_user(&gpu_args.tile, (void __user *)arg, sizeof(GPUTileArgs)))
      return -EACCES;
    write_tile(&gpu_args.tile);
    break;
  case VGA_GPU_WRITE_SPRITE:
    if (copy_from_user(&gpu_args.sprite, (void __user *)arg,
                       sizeof(GPUSpriteArgs)))
      return -EACCES;
    write_sprite(&gpu_args.sprite);
    break;
  case VGA_GPU_WRITE_PALETTE:
    if (copy_from_user(&gpu_args.palette, (void __user *)arg,
                       sizeof(GPUPaletteArgs)))
      return -EACCES;
    write_palette(&gpu_args.palette);
    break;
  case VGA_GPU_WRITE_CTRL:
    if (copy_from_user(&gpu_args.ctrl, (void __user *)arg, sizeof(GPUCtrlArgs)))
      return -EACCES;
    write_ctrl(&gpu_args.ctrl);
    break;
  case VGA_GPU_WRITE_SCROLL_X:
    if (copy_from_user(&arg_as_u16, (void __user *)arg, sizeof(u16)))
      return -EACCES;
    write_scroll_x(&arg_as_u16);
    break;
  case VGA_GPU_WRITE_SCROLL_Y:
    if (copy_from_user(&arg_as_u16, (void __user *)arg, sizeof(u16)))
      return -EACCES;
    write_scroll_y(&arg_as_u16);
    break;
  case VGA_GPU_READ_H_COUNT:
    arg_as_u16 = read_h_count();
    if (copy_to_user((void __user *)arg, &arg_as_u16, sizeof(u16)))
      return -EACCES;
    break;
  case VGA_GPU_READ_V_COUNT:
    arg_as_u16 = read_v_count();
    if (copy_to_user((void __user *)arg, &arg_as_u16, sizeof(u16)))
      return -EACCES;
    break;
  default:
    return -EINVAL;
  }

  return 0;
}

/* The operations our device knows how to do */
static const struct file_operations vga_gpu_fops = {
    .owner = THIS_MODULE,
    .unlocked_ioctl = vga_gpu_ioctl,
};

/* Information about our device for the "misc" framework -- like a char dev */
static struct miscdevice vga_gpu_misc_device = {
    .minor = MISC_DYNAMIC_MINOR,
    .name = DRIVER_NAME,
    .fops = &vga_gpu_fops,
};

/*
 * Initialization code: get resources (registers) and display
 * a welcome message
 */
static int __init vga_gpu_probe(struct platform_device *pdev) {
  int ret;

  /* Register ourselves as a misc device: creates /dev/vga_gpu */
  ret = misc_register(&vga_gpu_misc_device);

  /* Get the address of our registers from the device tree */
  ret = of_address_to_resource(pdev->dev.of_node, 0, &dev.res);
  if (ret) {
    ret = -ENOENT;
    goto out_deregister;
  }

  /* Make sure we can use these registers */
  if (request_mem_region(dev.res.start, resource_size(&dev.res), DRIVER_NAME) ==
      NULL) {
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
  misc_deregister(&vga_gpu_misc_device);
  return ret;
}

/* Clean-up code: release resources */
static int vga_gpu_remove(struct platform_device *pdev) {
  iounmap(dev.virtbase);
  release_mem_region(dev.res.start, resource_size(&dev.res));
  misc_deregister(&vga_gpu_misc_device);
  return 0;
}

/* Which "compatible" string(s) to search for in the Device Tree */
#ifdef CONFIG_OF
static const struct of_device_id vga_gpu_of_match[] = {
    {.compatible = "monkey_madness,vga_gpu-1.0"},
    {},
};
MODULE_DEVICE_TABLE(of, vga_gpu_of_match);
#endif

/* Information for registering ourselves as a "platform" driver */
static struct platform_driver vga_gpu_driver = {
    .driver =
        {
            .name = DRIVER_NAME,
            .owner = THIS_MODULE,
            .of_match_table = of_match_ptr(vga_gpu_of_match),
        },
    .remove = __exit_p(vga_gpu_remove),
};

/* Called when the module is loaded: set things up */
static int __init vga_gpu_init(void) {
  pr_info(DRIVER_NAME ": init\n");
  return platform_driver_probe(&vga_gpu_driver, vga_gpu_probe);
}

/* Calball when the module is unloaded: release resources */
static void __exit vga_gpu_exit(void) {
  platform_driver_unregister(&vga_gpu_driver);
  pr_info(DRIVER_NAME ": exit\n");
}

module_init(vga_gpu_init);
module_exit(vga_gpu_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Monkey Madness team, Columbia University");
MODULE_DESCRIPTION("VGA GPU driver");
