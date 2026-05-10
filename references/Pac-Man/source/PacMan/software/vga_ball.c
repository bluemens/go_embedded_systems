#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/miscdevice.h>
#include <linux/io.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include "vga_ball.h"

#define DRIVER_NAME "vga_ball"

#define SPRITE_DESC_OFFSET(i)  (dev.virtbase + (i) * 8)
#define SCORE_REG_OFFSET       (dev.virtbase + 0x28)
#define CONTROL_REG_OFFSET     (dev.virtbase + 0x2A)
#define PELLETE_EAT_REG_OFFSET (dev.virtbase + 0x2C)

#define NUM_SPRITES 5

struct vga_ball_dev {
    struct resource res;
    void __iomem *virtbase;
} dev;

static void write_all_state(vga_all_state_t *state) {

    int i = 0;
    for (i = 0; i < NUM_SPRITES; i++) {
        void __iomem *base = SPRITE_DESC_OFFSET(i);
        iowrite8(state->sprites[i].x,        base);
        iowrite8(state->sprites[i].y,        base + 1);
        iowrite8(state->sprites[i].frame,    base + 2);
        iowrite8(state->sprites[i].visible,  base + 3);
        iowrite8(state->sprites[i].direction,base + 4);
        iowrite8(state->sprites[i].type_id,  base + 5);
        iowrite8(state->sprites[i].reserved1,     base + 6);
        iowrite8(state->sprites[i].reserved2,     base + 7);
    }
    iowrite16(state->score, SCORE_REG_OFFSET);
    iowrite8(state->control, CONTROL_REG_OFFSET);
    iowrite8(0, CONTROL_REG_OFFSET + 1); // Clear the control flag
    iowrite16(state->pellet_to_eat, PELLETE_EAT_REG_OFFSET);
}

static void read_all_state(vga_all_state_t *state) {
    int i = 0;
    for (i = 0; i < NUM_SPRITES; i++) {
        void __iomem *base = SPRITE_DESC_OFFSET(i);
        state->sprites[i].x         = ioread8(base);
        state->sprites[i].y         = ioread8(base + 1);
        state->sprites[i].frame     = ioread8(base + 2);
        state->sprites[i].visible   = ioread8(base + 3);
        state->sprites[i].direction = ioread8(base + 4);
        state->sprites[i].type_id   = ioread8(base + 5);
        state->sprites[i].reserved1      = ioread8(base + 6);
        state->sprites[i].reserved2      = ioread8(base + 7);
    }
    state->score = ioread16(SCORE_REG_OFFSET);
    state->control = ioread8(CONTROL_REG_OFFSET);
    state->pellet_to_eat = ioread16(PELLETE_EAT_REG_OFFSET);
}

static long vga_ball_ioctl(struct file *f, unsigned int cmd, unsigned long arg) {
    vga_all_state_t user_state;

    switch (cmd) {
        case VGA_BALL_WRITE_ALL:
            if (copy_from_user(&user_state, (void __user *)arg, sizeof(user_state)))
                return -EFAULT;
            write_all_state(&user_state);
            break;

        case VGA_BALL_READ_ALL:
            read_all_state(&user_state);
            if (copy_to_user((void __user *)arg, &user_state, sizeof(user_state)))
                return -EFAULT;
            break;

        default:
            return -EINVAL;
    }

    return 0;
}

static const struct file_operations vga_ball_fops = {
    .owner = THIS_MODULE,
    .unlocked_ioctl = vga_ball_ioctl,
};

static struct miscdevice vga_ball_misc_device = {
    .minor = MISC_DYNAMIC_MINOR,
    .name = DRIVER_NAME,
    .fops = &vga_ball_fops,
};

static int __init vga_ball_probe(struct platform_device *pdev) {
    int ret;

    ret = misc_register(&vga_ball_misc_device);
    if (ret)
        return ret;

    ret = of_address_to_resource(pdev->dev.of_node, 0, &dev.res);
    if (ret)
        goto out_deregister;

    if (request_mem_region(dev.res.start, resource_size(&dev.res), DRIVER_NAME) == NULL) {
        ret = -EBUSY;
        goto out_deregister;
    }

    dev.virtbase = of_iomap(pdev->dev.of_node, 0);
    if (dev.virtbase == NULL) {
        ret = -ENOMEM;
        goto out_release_mem;
    }

    return 0;

out_release_mem:
    release_mem_region(dev.res.start, resource_size(&dev.res));
out_deregister:
    misc_deregister(&vga_ball_misc_device);
    return ret;
}

static int vga_ball_remove(struct platform_device *pdev) {
    iounmap(dev.virtbase);
    release_mem_region(dev.res.start, resource_size(&dev.res));
    misc_deregister(&vga_ball_misc_device);
    return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id vga_ball_of_match[] = {
    { .compatible = "csee4840,vga_ball-1.0" },
    {},
};
MODULE_DEVICE_TABLE(of, vga_ball_of_match);
#endif

static struct platform_driver vga_ball_driver = {
    .driver = {
        .name = DRIVER_NAME,
        .owner = THIS_MODULE,
        .of_match_table = of_match_ptr(vga_ball_of_match),
    },
    .remove = __exit_p(vga_ball_remove),
};

static int __init vga_ball_init(void) {
    pr_info(DRIVER_NAME ": init\n");
    return platform_driver_probe(&vga_ball_driver, vga_ball_probe);
}

static void __exit vga_ball_exit(void) {
    platform_driver_unregister(&vga_ball_driver);
    pr_info(DRIVER_NAME ": exit\n");
}

module_init(vga_ball_init);
module_exit(vga_ball_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Your Name");
MODULE_DESCRIPTION("Pac-Man VGA Controller - Sprite and Score Management");
