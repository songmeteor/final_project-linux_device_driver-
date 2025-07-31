#ifndef __VS10XX_DEVICE_H__
#define __VS10XX_DEVICE_H__

int vs10xx_device_init(int id);
int vs10xx_device_w_sci_reg(int id, unsigned char reg, unsigned char msb, unsigned char lsb);
int vs10xx_device_r_sci_reg(int id, unsigned char reg, unsigned char* msb, unsigned char* lsb);

#endif /* __VS10XX_DEVICE_H__ */