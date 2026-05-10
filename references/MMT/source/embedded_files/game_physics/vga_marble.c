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
#include "vga_marble.h"

#define DRIVER_NAME "vga_gpu"

//redefine when register map is finalized
#define TILES(x) (x)
#define TILEMAP(x) (x + 0x8000)
#define SPRITE(x) (x + 0xF000)
#define COLOR(x) (x + 0xF400)

/*
 * Information about our device
 */
struct vga_marble_dev {
	struct resource res; /* Resource: our registers */
	void __iomem *virtbase; /* Where registers can be accessed in memory */
	vga_marble_pos_t pos;
} dev;

/* Write the color palette to hw */
void write_palette(vga_palette_arg *pal) {
	pr_info(DRIVER_NAME "write_palette()\n");
	iowrite32(pal+sizeof(u8), COLOR(dev.virtbase) + pal->index);
}

/* Write individual tiles to hw */
void write_tile(vga_tile_arg *vta) {
	pr_info(DRIVER_NAME "write_tile()\n");
	u16 tile = 0;
    tile |= (vta->texture_index & 0x3FF);    
    tile |= (vta->palette_index & 0x7) << 10;          
    tile |= (vta->tile_priority & 0x1) << 13;               
    tile |= (vta->horizontal_flip & 0x1) << 14;                 
    tile |= (vta->vertical_flip & 0x1) << 15;
	
	iowrite16(&tile, TILEMAP(dev.virtbase) + vta->offset);
	
	/*int offset = sizeof(Tile) * num_offset;
    char *tile_data = tile->palette_indicies;
    iowrite32(tile_data, COLOR(virtbase) + offset);
    iowrite32(tile_data+32, COLOR(virtbase) + offset + 32);*/
}

void write_tile_texture(vga_tile_texture_arg *vtta) {
	pr_info(DRIVER_NAME "write_tile_texture()\n");
	int i = 0;
	for(i = 0; i < 8; i++)
		iowrite32(vtta->data+32*i, TILES(dev.virtbase) + vtta->offset + 32*i);
}

/* Write each tilemap tile to hw */
void write_tilemap_tile(char tile_index, int num_offset) {
    int offset = num_offset * 2;
    u16 tile = 0;
    tile |= (tile_index & 0x3FF);    
    tile |= (0 & 0x7) << 10;          
    tile |= (0 & 0x1) << 13;               
    tile |= (0 & 0x1) << 14;                 
    tile |= (0 & 0x1) << 15;      

    iowrite16(tile, TILEMAP(dev.virtbase) + offset);
}


/* NEEDS TO BE UPDATED */
/*
static void write_marble_position(vga_marble_pos_t *pos)
{
	iowrite16(pos->x, BALL_X(dev.virtbase));
	iowrite16(pos->y, BALL_Y(dev.virtbase));
	dev.pos = *pos;
}
*/

/* REST OF THE FILE NEEDS TO BE EXPLAINED TO JAKE */

/*
 * Handle ioctl() calls from userspace:
 * Read or write the segments on single digits.
 * Note extensive error checking of arguments
 */
static long vga_marble_ioctl(struct file *f, unsigned int cmd, unsigned long arg)
{
	vga_marble_pos_arg_t vmpa;
	vga_palette_arg vpa;
	vga_tile_arg vta;
	vga_tile_texture_arg vtta;

	switch (cmd) {
	/* TILE MAP CASES */
	case VGA_MARBLE_WRITE_PALETTE:
		if (copy_from_user(&vpa, (void __user *)arg, sizeof(vga_palette_arg)))
			return -EFAULT;
		write_palette(&vpa); 
		break;
	case VGA_MARBLE_WRITE_TILE:
		if (copy_from_user(&vta, (void __user *)arg, sizeof(vga_tile_arg)))
			return -EFAULT;
		write_tile(&vta); 
		break;
	case VGA_MARBLE_WRITE_TILE_TEXTURE:
		if (copy_from_user(&vtta, (void __user *)arg, sizeof(vga_tile_texture_arg)))
			return -EFAULT;
		write_tile_texture(&vtta);
		break;
	/* MARBLE CASES? */
	/*
	case VGA_MARBLE_WRITE_POSITION:
		if (copy_from_user(&vmpa, (vga_marble_pos_arg_t *) arg, sizeof(vga_marble_pos_arg_t)))
			return -EACCES;
		write_marble_position(&vmpa.pos);
		break;
		*/
	default:
		return -EINVAL;
	}

	return 0;
}

/* The operations our device knows how to do */
static const struct file_operations vga_marble_fops = {
	.owner= THIS_MODULE,
	.unlocked_ioctl = vga_marble_ioctl,
};

/* Information about our device for the "misc" framework -- like a char dev */
static struct miscdevice vga_marble_misc_device = {
	.minor	= MISC_DYNAMIC_MINOR,
	.name	= DRIVER_NAME,
	.fops	= &vga_marble_fops,
};

/*
 * Initialization code: get resources (registers) and display
 * a welcome message
 */
static int __init vga_marble_probe(struct platform_device *pdev)
{
	pr_info(DRIVER_NAME ": vga_marble_probe\n");
	int ret;

	/* Register ourselves as a misc device: creates /dev/vga_marble */
	ret = misc_register(&vga_marble_misc_device);
	if (ret) {
		pr_err(DRIVER_NAME ": failed to register misc device %d\n", ret);
	}

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
	misc_deregister(&vga_marble_misc_device);
	return ret;
}

/* Clean-up code: release resources */
static int vga_marble_remove(struct platform_device *pdev)
{
	pr_info(DRIVER_NAME ": vga_marble_remove\n");
	iounmap(dev.virtbase);
	release_mem_region(dev.res.start, resource_size(&dev.res));
	misc_deregister(&vga_marble_misc_device);
	return 0;
}

/* Which "compatible" string(s) to search for in the Device Tree */
#ifdef CONFIG_OF
static const struct of_device_id vga_marble_of_match[] = {
	{ .compatible = "monkey_madness,vga_gpu-1.0" },
	{},
};
MODULE_DEVICE_TABLE(of, vga_marble_of_match);
#endif

/* Information for registering ourselves as a "platform" driver */
static struct platform_driver vga_marble_driver = {
	.driver = {
		.name = DRIVER_NAME,
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(vga_marble_of_match),
	},
	.remove = __exit_p(vga_marble_remove),
};

/* Called when the module is loaded: set things up */
static int __init vga_marble_init(void)
{
	pr_info(DRIVER_NAME ": init\n");
	return platform_driver_probe(&vga_marble_driver, vga_marble_probe);
}

/* Calball when the module is unloaded: release resources */
static void __exit vga_marble_exit(void)
{
	platform_driver_unregister(&vga_marble_driver);
	pr_info(DRIVER_NAME ": exit\n");
}

module_init(vga_marble_init);
module_exit(vga_marble_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Sadie & Madeline, Columbia University");
MODULE_DESCRIPTION("VGA marble driver");
