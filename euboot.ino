/**
 * @file euboot.ino
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
 * @link Potal : https://askn37.github.io/
 *       MIT License : https://askn37.github.io/LICENSE.html
 */

/*
 * This file is always empty.
 * It tells the Arduino IDE to recognize the fileset as a valid sketch.
 * It is assumed that the following SDK is used:
 *
 *   - https://github.com/askn37/multix-zinnia-sdk-modernAVR @0.3.0+
 *
 * The SDK menu options are as follows for "AVR64DU32 Curiosity Nano":
 *
 *     Board Manager - AVR DU w/o Bootloader
 *           Variant - 32pin AVR64DU32 (64KiB+8KiB)
 *         Clock(Dx) - Internal 20 MHz (recommend)      : Maximum speed available on die with Errata.
 *          FUSE PF6 - PF6 pin=GPIO (input only)        : Not change this, SW0 will not be usable.
 *         Build API - Macro API Enable without startup : REQUIRED
 *   Console and LED - UART1 TX:PD6 RX:PD7 LED=PF2 SW=PF6 (AVR64DU32 Curiosity Nano)
 *        Proggramer - Curiosity Nano (nEDBG: ATSAMD21E18)
 */

// end of code
