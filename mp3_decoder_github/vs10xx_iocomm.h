#ifndef __VS10XX_IOCOMM_H__
#define __VS10XX_IOCOMM_H__

int vs10xx_io_init(int id);
void vs10xx_io_exit(int id);
int vs10xx_io_reset(int id);
int vs10xx_io_data_tx(int id, const char *buf, int len);
int vs10xx_io_ctrl_xf(int id, const char *txbuf, unsigned txlen, char *rxbuf, unsigned rxlen);
int vs10xx_io_wtready(int id, int timeout);

#endif /* __VS10XX_IOCOMM_H__ */