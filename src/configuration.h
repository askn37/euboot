/**
 * @file configuration.h
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

#pragma once
#include <avr/io.h>
#include "api/macro_api.h"  /* interrupts, initVariant */
#include "variant.h"

/************************
 * Global configuration *
 ***********************/

/*
 * When the DEBUG symbol is enabled, DBG-COM is enabled.
 *
 *  If enabled, the application start address will be set to a different value than normal.
 *
 *    NDEBUG  0x0A00  (2560)  BOOTSIZE= 5 sectors
 *    DEBUG1  0x2000  (8192)  BOOTSIZE=16 sectors
 *
 *  The DEBUG=0 output is not normally used,
 *  but can be used to filter only user-defined output.
 */

// #define DEBUG 2

/*** CONFIG_SYS ***/

/*
 * USB VID:PID pair value
 *
 *  The default value is MCHP:HIDC (04D8:0B12)
 */

#define CONFIG_USB_VIDPID 0xD8, 0x04, 0x12, 0x0B

/*
 * JTAGICE3 FW versions:
 *
 *  The version notifications shown here are compatible
 *  with bootloaders that support <FlashNVM.h>.
 *
 *  Columns: HW_VER, FW_MAJOR, FW_MINOR, FW_RELL, FW_RELH (all 1-byte decimal)
 */

#define CONFIG_SYS_FWVER { 52, 3, 72, 48, 0 }

/*
 * Bootloader enable switch
 *
 *  WDT reset and software reset always wake up the user application,
 *  other resets check the short status of the specified ports.
 *
 *  If you use PF6, make sure you disable the FUSE_RSTPINCFG_bm in SYSCFG0.
 */

#ifndef PIN_SYS_SW0
  #ifdef SW_BUILTIN
    #define PIN_SYS_SW0 SW_BUILTIN
  #else
    #define PIN_SYS_SW0 PIN_PF6
  #endif
#endif

/*
 * Bootloader Status LED
 *
 *  For PF2, the sign is reversed to correspond to CURIOSITY NANO.
 */

#ifndef PIN_SYS_LED0
  #ifdef LED_BUILTIN
    #define PIN_SYS_LED0 LED_BUILTIN
  #else
    #define PIN_SYS_LED0 PIN_PC3
  #endif
#endif

// end of header
