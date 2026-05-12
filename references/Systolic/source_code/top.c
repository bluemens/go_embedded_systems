/* * Device driver for the VGA video generator
 *
 * A Platform device implemented using the misc subsystem
 *
 * Project final, csee4840
 * Columbia University
 * Hang Ye
 *
 * Stephen A. Edwards
 * Columbia University
 *
 * References:
 * Linux source: Documentation/driver-model/platform.txt
 *               drivers/misc/arm-charlcd.c
 * http://www.linuxforu.com/tag/linux-device-drivers/
 * http://free-electrons.com/docs/
 *
 * "make" to build
 * insmod vga_ball.ko
 *
 * Check code style with
 * checkpatch.pl --file --no-tree vga_ball.c
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
#include "top.h"

#define DRIVER_NAME "top"

/* Device registers */
#define IMG_SIZE(x) 	(x)
#define WEIGHT_DATA(x) 	((x)+1)
#define INPUT_DATA(x) 	((x)+2)
#define DONE(x) 		((x)+3)
#define OUTPUT_DATA(x) 	((x)+4)

/*
 * Information about our device
 */
struct top_dev {
	struct resource res; /* Resource: our registers */
	void __iomem *virtbase; /* Where registers can be accessed in memory */
		top_input_data_t input_data;
		top_weight_data_t weight_data;
		top_img_size_t img_size;
		top_output_data_t output_data;
		top_done_t done;
} dev;

/*
 * Write segments of a single digit
 * Assumes digit is in range and the device information has been set up
 */
static void write_img_size(top_img_size_t *img_size)
{
	iowrite8(img_size->img_size, IMG_SIZE(dev.virtbase) );
	dev.img_size = *img_size;
}
static void write_weight_data(top_weight_data_t *weight_data)
{
	iowrite8(weight_data->weight_data, WEIGHT_DATA(dev.virtbase) );
	dev.weight_data = *weight_data;
}
static void write_input_data(top_input_data_t *input_data)
{
	iowrite8(input_data->input_data, INPUT_DATA(dev.virtbase) );
	dev.input_data = *input_data;
}
static void read_output_data(top_output_data_t *output_data)
{
    output_data->output_data = ioread8(OUTPUT_DATA(dev.virtbase));
    dev.output_data = *output_data;
}

static void read_done(top_done_t *done)
{
    done->done = ioread8(DONE(dev.virtbase));
    dev.done = *done;
}
/*
 * Handle ioctl() calls from userspace:
 * Read or write the segments on single digits.
 * Note extensive error checking of arguments
 */
 static long top_ioctl(struct file *f, unsigned int cmd, unsigned long arg)
 {
	 top_arg_t vla;
 
	 switch (cmd) {
	 case TOP_WRITE_IMG_SIZE:
		 if (copy_from_user(&vla, (top_arg_t __user *)arg, sizeof(vla)))
			 return -EACCES;
		 write_img_size(&vla.img_size);
		 break;
 
	 case TOP_WRITE_WEIGHT_DATA:
		 if (copy_from_user(&vla, (top_arg_t __user *)arg, sizeof(vla)))
			 return -EACCES;
		 write_weight_data(&vla.weight_data);
		 break;
 
	 case TOP_WRITE_INPUT_DATA:
		 if (copy_from_user(&vla, (top_arg_t __user *)arg, sizeof(vla)))
			 return -EACCES;
		 write_input_data(&vla.input_data);
		 break;
 
	 case TOP_READ_OUTPUT_DATA:
		 read_output_data(&vla.output_data);
		 if (copy_to_user((top_arg_t __user *)arg, &vla, sizeof(vla)))
			 return -EACCES;
		 break;
 
	 case TOP_READ_DONE:
		 read_done(&vla.done);
		 if (copy_to_user((top_arg_t __user *)arg, &vla, sizeof(vla)))
			 return -EACCES;
		 break;
 
	 default:
		 return -EINVAL;
	 }
 
	 return 0;
 }

/* The operations our device knows how to do */
static const struct file_operations top_fops = {
	.owner		= THIS_MODULE,
	.unlocked_ioctl = top_ioctl,
};

/* Information about our device for the "misc" framework -- like a char dev */
static struct miscdevice top_misc_device = {
	.minor		= MISC_DYNAMIC_MINOR,
	.name		= DRIVER_NAME,
	.fops		= &top_fops,
};

/*
 * Initialization code: get resources (registers) and display
 * a welcome message
 */
static int __init top_probe(struct platform_device *pdev)
{
	int ret;

    /* Register ourselves as a misc device: creates /dev/top */
    ret = misc_register(&top_misc_device);

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
    misc_deregister(&top_misc_device);
    return ret;
}

/* Clean-up code: release resources */
static int top_remove(struct platform_device *pdev)
{
    iounmap(dev.virtbase);
    release_mem_region(dev.res.start, resource_size(&dev.res));
    misc_deregister(&top_misc_device);
    return 0;
}

/* Which "compatible" string(s) to search for in the Device Tree */
#ifdef CONFIG_OF
static const struct of_device_id top_of_match[] = {
    { .compatible = "csee4840,top-1.0" },
    {},
};
MODULE_DEVICE_TABLE(of, top_of_match);
#endif

/* Information for registering ourselves as a "platform" driver */
static struct platform_driver top_driver = {
    .driver	= {
        .name	= DRIVER_NAME,
        .owner	= THIS_MODULE,
        .of_match_table = of_match_ptr(top_of_match),
    },
    .remove	= __exit_p(top_remove),
};

/* Called when the module is loaded: set things up */
static int __init top_init(void)
{
    pr_info(DRIVER_NAME ": init\n");
    return platform_driver_probe(&top_driver, top_probe);
}

/* Called when the module is unloaded: release resources */
static void __exit top_exit(void)
{
    platform_driver_unregister(&top_driver);
    pr_info(DRIVER_NAME ": exit\n");
}

module_init(top_init);
module_exit(top_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Hang Ye, Columbia University");
MODULE_DESCRIPTION("Top driver");