/*
 * vs10xx_iocomm.c
 * 이 파일 전체를 아래 내용으로 교체하세요.
 */
#include <linux/spi/spi.h>
#include <linux/delay.h>
#include "vs10xx.h"
#include "vs10xx_iocomm.h"

/*
 * devm_gpiod_get 함수가 리소스 관리를 자동으로 해주므로
 * io_init과 io_exit 함수는 더 이상 할 일이 없습니다.
 * 하지만 다른 파일에서 호출할 수 있으니 함수 형태는 남겨둡니다.
 */
int vs10xx_io_init(int id) {
    return 0; 
}

void vs10xx_io_exit(int id) {
    /* Do nothing, devm_ resource management handles cleanup */
}

int vs10xx_io_reset(int id) {
    /* gpio_set_value -> gpiod_set_value 로 변경 */
    gpiod_set_value(vs10xx_chips[id].gpio_reset, 0);
    mdelay(2);
    gpiod_set_value(vs10xx_chips[id].gpio_reset, 1);
    mdelay(2);
    return 0;
}

int vs10xx_io_wtready(int id, int timeout) {
    int i = 0;
    /* gpio_get_value -> gpiod_get_value 로 변경 */
    while (!gpiod_get_value(vs10xx_chips[id].gpio_dreq)) {
        msleep(1);
        if (i++ > timeout) return 0;
    }
    return 1;
}

int vs10xx_io_ctrl_xf(int id, const char *txbuf, unsigned txlen, char *rxbuf, unsigned rxlen) {
    int status = 0;
    struct spi_message *msg = &vs10xx_chips[id].msg;
    struct spi_transfer *xfer = vs10xx_chips[id].transfer;

    memset(xfer, 0, sizeof(vs10xx_chips[id].transfer));
    spi_message_init(msg);

    if (txbuf && txlen) {
        memcpy(vs10xx_chips[id].tx_buf, txbuf, txlen);
        xfer[0].tx_buf = vs10xx_chips[id].tx_buf;
        xfer[0].len = txlen;
        spi_message_add_tail(&xfer[0], msg);
    }
    
    if (rxbuf && rxlen) {
        xfer[1].rx_buf = vs10xx_chips[id].rx_buf;
        xfer[1].len = rxlen;
        spi_message_add_tail(&xfer[1], msg);
    }
    
    status = spi_sync(vs10xx_chips[id].spi_ctrl, msg);
    if (status < 0) {
        pr_err("vs10xx: id:%d spi_sync failed: %d\n", id, status);
        return status;
    }
    
    if (rxbuf && rxlen) {
        memcpy(rxbuf, vs10xx_chips[id].rx_buf, rxlen);
    }
    
    return status;
}

int vs10xx_io_data_tx(int id, const char *buf, int len) {
    struct spi_transfer t = {
        .tx_buf = buf,
        .len = len,
    };
    struct spi_message m;

    spi_message_init(&m);
    spi_message_add_tail(&t, &m);
    return spi_sync(vs10xx_chips[id].spi_data, &m);
}