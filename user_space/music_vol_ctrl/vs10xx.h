/*
 * 파일: vs10xx.h
 * 이 내용으로 파일 전체를 교체하세요.
 */
#ifndef VS10XX_H
#define VS10XX_H

#include <linux/ioctl.h>

/* ioctl 매직 넘버 */
#define VS10XX_IOCTL_BASE 'v'

/*
 * 커널 드라이버에 구현된 ioctl 명령어
 * (unsigned int 타입의 값을 커널로 전달)
 */
#define VS10XX_SET_VOL _IOW(VS10XX_IOCTL_BASE, 1, unsigned int)

#endif /* VS10XX_H */
