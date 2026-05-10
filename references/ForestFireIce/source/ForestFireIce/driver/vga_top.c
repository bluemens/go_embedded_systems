/*
 * vga_top – driver for FPGA VGA core
 *
 * Registers (byte offsets, 32-bit wide)
 *   0x00  CTRL_REG            W
 *   0x04  STATUS_REG          R
 *   0x80..0xFC  SPRITE[n]     R/W  (n = 0-31)
 *
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
#include "vga_top.h"

#define DRIVER_NAME "vga_top"

/* ---------- register helpers ---------- */
#define CTRL_REG(base)     ((base) + 0x00)
#define STATUS_REG(base)   ((base) + 0x04)
#define SPRITE_REG(base,n) ((base) + 0x80 + ((n) * 4))

/*
 * Information about our device
 */
struct vga_top_dev {
	struct resource res; /* Resource: our registers */
	void __iomem *virtbase; /* Where registers can be accessed in memory */
    u32 cached_ctrl;
} dev;

/*
 * Handle ioctl() calls from userspace:
 * Read or write the segments on single digits.
 * Note extensive error checking of arguments
 */
static long vga_top_ioctl(struct file *f, unsigned int cmd, unsigned long arg)
{
	vga_top_ctrl_arg_t   c_arg;
	vga_top_status_arg_t s_arg;
	vga_top_sprite_arg_t sp_arg;

	switch (cmd) {
	case VGA_TOP_WRITE_CTRL:
		if (copy_from_user(&c_arg, (vga_top_ctrl_arg_t *) arg, sizeof(vga_top_ctrl_arg_t)))
			return -EACCES;
			iowrite32(c_arg.value, CTRL_REG(dev.virtbase));
			dev.cached_ctrl = c_arg.value;
		break;

	case VGA_TOP_READ_STATUS:
		s_arg.value = ioread32(STATUS_REG(dev.virtbase));
		if (copy_to_user((vga_top_status_arg_t *) arg, &s_arg, sizeof(vga_top_status_arg_t)))
			return -EACCES;
		break;
	
	case VGA_TOP_WRITE_SPRITE:
        if (copy_from_user(&sp_arg, (vga_top_sprite_arg_t *) arg, sizeof(vga_top_sprite_arg_t)))
            return -EACCES;
		if (sp_arg.index > 31)
			return -EINVAL;
		iowrite32(sp_arg.attr_word, SPRITE_REG(dev.virtbase, sp_arg.index));
        break;

	default:
		return -EINVAL;
	}

	return 0;
}

/* The operations our device knows how to do */
static const struct file_operations vga_top_fops = {
	.owner		= THIS_MODULE,
	.unlocked_ioctl = vga_top_ioctl,
};

/* Information about our device for the "misc" framework -- like a char dev */
static struct miscdevice vga_top_misc_device = {
	.minor		= MISC_DYNAMIC_MINOR,
	.name		= DRIVER_NAME,
	.fops		= &vga_top_fops,
};

/*
 * Initialization code: get resources (registers) and display
 * a welcome message
 */
static int __init vga_top_probe(struct platform_device *pdev)
{
	int ret;

	/* Register ourselves as a misc device: creates /dev/vga_top */
	ret = misc_register(&vga_top_misc_device);

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
	misc_deregister(&vga_top_misc_device);
	return ret;
}

/* Clean-up code: release resources */
static int vga_top_remove(struct platform_device *pdev)
{
	iounmap(dev.virtbase);
	release_mem_region(dev.res.start, resource_size(&dev.res));
	misc_deregister(&vga_top_misc_device);
	return 0;
}

/* Which "compatible" string(s) to search for in the Device Tree */
#ifdef CONFIG_OF
static const struct of_device_id vga_top_of_match[] = {
	{ .compatible = "csee4840,vga_top-1.0" },
	{},
};
MODULE_DEVICE_TABLE(of, vga_top_of_match);
#endif

/* Information for registering ourselves as a "platform" driver */
static struct platform_driver vga_top_driver = {
	.driver	= {
		.name	= DRIVER_NAME,
		.owner	= THIS_MODULE,
		.of_match_table = of_match_ptr(vga_top_of_match),
	},
	.remove	= __exit_p(vga_top_remove),
};

/* Called when the module is loaded: set things up */
static int __init vga_top_init(void)
{
	pr_info(DRIVER_NAME ": init\n");
	return platform_driver_probe(&vga_top_driver, vga_top_probe);
}

/* Calball when the module is unloaded: release resources */
static void __exit vga_top_exit(void)
{
	platform_driver_unregister(&vga_top_driver);
	pr_info(DRIVER_NAME ": exit\n");
}

module_init(vga_top_init);
module_exit(vga_top_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("ForestFireIce CSEE 4840");
MODULE_DESCRIPTION("VGA_TOP misc driver");
