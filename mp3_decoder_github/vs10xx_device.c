#include "vs10xx.h"
#include "vs10xx_iocomm.h"
#include "vs10xx_device.h"

// SCI Registers
#define SCI_MODE        0x00
#define SCI_STATUS      0x01
#define SCI_CLOCKF      0x03
#define SCI_AUDATA      0x05
#define SCI_VOL         0x0B

int vs10xx_device_init(int id) {
    unsigned char msb, lsb;
    
    vs10xx_io_reset(id);
    
    // Read version
    vs10xx_device_r_sci_reg(id, SCI_STATUS, &msb, &lsb);
    printk(KERN_INFO "vs10xx: VS10xx Version: %d\n", (lsb >> 4) & 0x0F);
    
    // Set clock, e.g., XTALI * 4.5
    vs10xx_device_w_sci_reg(id, SCI_CLOCKF, 0xB8, 0x00);
    
    // Set volume
    vs10xx_device_w_sci_reg(id, SCI_VOL, 0xFE, 0xFE); // Min volume
    
    // Set sample rate
    vs10xx_device_w_sci_reg(id, SCI_AUDATA, 0xAC, 0x45); // 44100Hz stereo

    return 0;
}

int vs10xx_device_w_sci_reg(int id, unsigned char reg, unsigned char msb, unsigned char lsb) {
    int status;
    unsigned char cmd[] = {0x02, reg, msb, lsb};

    if (!vs10xx_io_wtready(id, 100)) {
        PERR("id:%d timeout before write (reg=%x)", id, reg);
        return -1;
    }
    
    status = vs10xx_io_ctrl_xf(id, cmd, sizeof(cmd), NULL, 0);
    
    if (!vs10xx_io_wtready(id, 100)) {
        PERR("id:%d timeout after write (reg=%x)", id, reg);
        return -1;
    }

    return status;
}

int vs10xx_device_r_sci_reg(int id, unsigned char reg, unsigned char* msb, unsigned char* lsb) {
    int status;
    unsigned char cmd[] = {0x03, reg};
    unsigned char res[2] = {0, 0};

    if (!vs10xx_io_wtready(id, 100)) {
        PERR("id:%d timeout before read (reg=%x)", id, reg);
        return -1;
    }
    
    status = vs10xx_io_ctrl_xf(id, cmd, sizeof(cmd), res, sizeof(res));
    *msb = res[0];
    *lsb = res[1];
    
    if (!vs10xx_io_wtready(id, 100)) {
        PERR("id:%d timeout after read (reg=%x)", id, reg);
        return -1;
    }

    return status;
}