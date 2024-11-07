/**
 * @file fuse.c
 * @author askn (K.Sato) multix.jp
 * @brief `euboot` (EDBG USB Bootloader) is a USB bootloader for AVR-DU series that runs
 *        at full USB 2.0 speed. It is recognized as a HID/CMSIS-DAP/EDBG device in
 *        AVRDUDE>=8.0 and can read all memory areas and write to FLASH, EEPROM, USERROW
 *        and BOOTROW areas. It cannot change FUSE and LOCKBIT or erase the chip.
 *        To start `euboot`, connect it to a USB port while holding down the configured
 *        button (or shorting it to GND) and power it on. If successful, the configured
 *        LED will blink in a specific pattern and wait for a connection from AVRDUDE.
 *        No automatic reset from Atduino IDE/CLI is possible.
 * @version 3.72.48+
 * @date 2024-11-01
 * @copyright Copyright (c) 2024 askn37 at github.com
 * @link Product Potal : https://askn37.github.io/
 *         MIT License : https://askn37.github.io/LICENSE.html
 */

#include <avr/io.h>
#include <avr/fuse.h>
#include "configuration.h"

/*
 * Note:
 *
 * These are the recommended FUSE arrays.
 *
 * - SYSCFG0->FUSE_UPDIPINCFG_bm is True by default
 * - SYSCFG0->FUSE_RSTPINCFG_bm varies depending on SW0 usage
 * - SYSCFG0->FUSE_EESAVE_bm is True to preserve information
 * - PDICFG should not be changed from the default
 */

#if PIN_SYS_SW0 != PIN_PF6
  #define ENABLE_SYS_RESET FUSE_RSTPINCFG_bm
#else
  #define ENABLE_SYS_RESET 0
#endif

#if defined(DEBUG) && !defined(NDEBUG)
  #define APPSTART 16
#else
  #define APPSTART 5
#endif

FUSES = {
    .WDTCFG   = FUSE0_DEFAULT,
    .BODCFG   = FUSE1_DEFAULT,
    .OSCCFG   = FUSE2_DEFAULT,
    .SYSCFG0  = FUSE5_DEFAULT | FUSE_EESAVE_bm | ENABLE_SYS_RESET,
    .SYSCFG1  = FUSE6_DEFAULT,
    .CODESIZE = FUSE7_DEFAULT,  /* 0=All application code */
    .BOOTSIZE = APPSTART,
    .PDICFG   = FUSE10_DEFAULT  /* Never change it */
};

// end of code
