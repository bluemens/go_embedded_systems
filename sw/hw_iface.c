#include "hw_iface.h"

#include <fcntl.h>
#include <stdio.h>
#include <sys/mman.h>
#include <unistd.h>

static int       fd_mem = -1;
static void     *lw_base = NULL;

int hw_init(void)
{
    if (lw_base) return 0;
    fd_mem = open("/dev/mem", O_RDWR | O_SYNC);
    if (fd_mem < 0) {
        perror("open /dev/mem");
        return -1;
    }
    lw_base = mmap(NULL, LW_BRIDGE_SPAN, PROT_READ | PROT_WRITE,
                   MAP_SHARED, fd_mem, LW_BRIDGE_BASE);
    if (lw_base == MAP_FAILED) {
        perror("mmap LW bridge");
        close(fd_mem);
        fd_mem = -1;
        lw_base = NULL;
        return -1;
    }
    return 0;
}

void hw_close(void)
{
    if (lw_base) {
        munmap(lw_base, LW_BRIDGE_SPAN);
        lw_base = NULL;
    }
    if (fd_mem >= 0) {
        close(fd_mem);
        fd_mem = -1;
    }
}

uint32_t hw_read(uint32_t off)
{
    return *(volatile uint32_t *)((char *)lw_base + off);
}

void hw_write(uint32_t off, uint32_t val)
{
    *(volatile uint32_t *)((char *)lw_base + off) = val;
}
