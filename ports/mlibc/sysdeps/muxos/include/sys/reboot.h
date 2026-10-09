#ifndef _SYS_REBOOT_H
#define _SYS_REBOOT_H

#define RB_AUTOBOOT 0x01234567
#define RB_HALT_SYSTEM 0xcdef0123
#define RB_POWER_OFF 0x4321fedc

#ifdef __cplusplus
extern "C" {
#endif

int reboot(int command);

#ifdef __cplusplus
}
#endif

#endif