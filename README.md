# euboot -- EDBG USB bootloader for AVR-DU series

*Switching document languages* : [日本語](README_jp.md), __English__

## Features

- USB bootloader for AVR-DU series.
- Uses standard USB-HID/CMSIS-DAP/EDBG protocols and is recognized as a `jtag3updi` device by AVRDUDE<=8.0.
- Fast memory read/write speeds, close to the limits of USB Full-Speed.
- Footprint is less than 2.5KiB.

## Reasons for development

The AVR-DU series is the only current AVR generation with built-in USB peripherals, but unlike previous similar products (ATMEL generations), it does not ship with a DFU bootloader and the bare metal chip flash is always empty.

This is partly because ATMEL was a regular member of the USB-IF DFU standard development, whereas Microchip was not. Therefore, it is unlikely that DFU support will be provided in the future.

Currently, AVRDUDE has two alternative bootloader approaches: USB-CDC and USB-HID. CDC (VCP) is very popular but has many drawbacks and few advantages: it requires communication to be split into bytes, is complex and slow, requires a lot of effort in SRAM management, and even worse, requires a new patch to AVRDUDE to properly support it.

On the other hand, bulk transfer communication using USB-HID is not very popular because it requires USB expertise. However, recently, [UPDI4AVR-USB](https://github.com/askn37/UPDI4AVR-USB/) has appeared, which supports CMSIS-DAP and EDBG protocols and can also handle `jtag3updi`. Using this as a base, you can implement only the necessary parts and add NVM control. The results were good, and we were able to develop a USB bootloader that supports `jtag3updi` with a footprint of 2.5KiB. Although it is not as small as DFU, it is compact enough for practical use. Additionally, the lack of internal overhead makes it easy to achieve memory read speeds approaching the upper limit of USB Full-Speed.

## What you need to create the Bootloader Firmware

To do this, you need the following environment:

#### [MultiX Zinnia Product SDK [modernAVR] @0.3.0+](https://github.com/askn37/multix-zinnia-sdk-modernAVR)

It is a bare metal SDK that can be easily installed with Arduino IDE/CLI board manager. It has various macros that make it easier to use AVR-LIBC, and allows you to write low level code in a similar way to Arduino-API. It also installs AVRDUDE 8.0+ at the same time.

#### [Arduino-CLI @1.0.3+](https://arduino.github.io/arduino-cli/1.0/installation/)

This is required if you want to build the firmware using the included Makefile.
It is possible to output binary file using only the modernAVR SDK without using this, but the menu settings will become more complicated.

> [!HINT]
> In a Windows environment, you cannot use `make` as is. You need to use WSL or some other method.

#### UPDI compatible programmer for AVR-DU series

This mainly applies to `PICKit4+`. However, the most readily available and cheapest one in the world is ["AVR64DU32 Curiosity Nano : EV59F82A"](https://www.microchip.com/en-us/development-tool/ev59f82a). Get this and install [UPDI4AVR-USB](https://github.com/askn37/UPDI4AVR-USB/). From AVRDUDE, you can use it as a UPDI programmer like `PICKit4`. For details, please refer to each link.

## Creating and Installing the Bootloader Firmware

Install the modernAVR SDK, add Arduino-CLI and AVRDUDE 8.0 to your path, and when you're ready, go to the `euboot` directory and run `make all`. The generated files will be saved in the `hex` directory.

```sh
euboot $ make all
```

> [!HINT]
> If you have a `Perl5` executable, the hex/bin file contains a CRC32 for use with the `CRCSCAN` peripheral.

Upload the resulting file to the target. In this example, the target is CURIOSITY NANO. This is easy because `pkobn_updi` is built in.

```sh
euboot $ avrdude -cpkobn_updi -pavr64du32 -Uflash:w:hex/euboot_LF2_SF6.hex:i -Ufuses:w:hex/euboot_LF2_SF6.fuse:i
```

Once the upload is successful, the firmware will start working immediately. If there is nothing plugged into the target USB port of the CNANO, the LED (that is PF2) will keep flashing in the following pattern, which means the USB enumeration with the host PC is not completed.

- LED(PF2): 🟠⚫️⚫️⚫️ (Waiting for enumeration)

When you connect a second USB cable to the target USB port or switch the USB cable from the debugger port, the LED changes to the following blinking pattern, indicating that it is ready for EDBG protocol communication to begin.

- LED(PF2): 🟠🟠⚫️⚫️ (Standby mode)

Ready to go? Now let's check if the USB bootloader responds correctly. Note the `-P` option.

```sh
$ avrdude -Pusb:04d8:0b12 -cjtag3updi -pavr64du32 -v
```

```text
Avrdude version 8.0-20241010 (0b92721a)
Copyright see https://github.com/avrdudes/avrdude/blob/main/AUTHORS

System wide configuration file is /usr/local/etc/avrdude.conf
User configuration file is /Users/user/.avrduderc

Using port            : usb:04d8:0b12
Using programmer      : jtag3updi
AVR part              : AVR64DU32
Programming modes     : SPM, UPDI
Programmer type       : JTAGICE3_UPDI
Description           : Atmel AVR JTAGICE3 in UPDI mode
ICE HW version        : 52
ICE FW version        : 3.72 (rel. 48)
Serial number         : euboot:CMSIS-DAP:EDBG
Vtarget               : 3.30 V
PDI/UPDI clk          : 2560 kHz

Partial Family_ID returned: "AVR "
Silicon revision: 1.3

AVR device initialized and ready to accept instructions
Device signature = 1E 96 21 (AVR64DU32)

Avrdude done.  Thank you.
```

Firmware specific information is listed under `ICE HW Version`. The item names and contents do not match by design, because AVRDUDE does not have the ability to display specific information, so unused items are used instead.

- __ICE HW version__: It is always 52. This is originally a character code for `4` and indicates the version of the NVM controller.
- __ICE FW version__: Shows the version and update number of the USB bootloader.
- __Serial number__: This shows the product string of the USB device. The USB serial number string is not used, so both will show the same string.
- __Vtarget__: Displays the operating voltage supplied to VDD.
- __PDI/UPDI clk__: This is not the actual operating speed. It is the starting address of the user application program calculated from the `FUSE_BOOTSIZE` setting. For example, `2560` indicates that the user application program runs from address `0x0A00`. If it is `0`, the FUSE configuration is insufficient to write the user application.

A user application written to run from a specific address can be written to the standby USB bootloader as follows. Using the `-D` option is recommended. The LED blinking during memory read/write will change as follows:

- LED(PF2): 🟠🟠🟠🟠 (Communication in progress)

```sh
$ avrdude -Pusb:04d8:0b12 -cjtag3updi -pavr64du32 -v -D -Uflash:w:USERAPP.hex:i
```

Similarly, the `-U` option can be used to write and read `eeprom`, `userrow`, and `bootrow`.
`fuse(s)` and `lock` are read-only and cannot be modified.

## Activating the USB bootloader

If a user application has not yet been written and the device is empty, the USB bootloader takes priority, but once a user application has been written, the USB bootloader is skipped. To disable this and activate the USB bootloader, press and hold `SW0(PF6)` and power-on the CNANO.
If successful, the LED will flash to indicate standby mode.

With the default `FUSES` settings, `SW0(PF6)` does not function as a hard reset switch, it is a normal GPIO input, to use it as a reset switch the user application must implement the necessary code.

The user application can choose between two reset methods: WDT operation and SWRST operation. The WDT reset always activates the user application, ignoring the active switch. The SWRST reset activates the USB bootloader if the active switch is LOW.

## SPM snippets

The first address of the USB bootloader starts at `PROGMEM_START` in the `PROGMEM` area. It contains a special magic number and the SPM snippet code.

|Series|Address|Magic number: uint32_t (LE)|
|-|-|-|
|AVR_DA/DB/DD/DU/EA/EB|PROGMEM_START + 2 bytes|0x95089361|

> [!TIPS] 
> `(pgm_read_dword(PROGMEM_START + 2) == 0x95089361L`

|Offset|HWV=52|OP code|
|-|-|-|
|$02|nvm_stz|ST Z+, R22 \n RET
|$06|nvm_ldz|LD R24, Z+ \n RET
|$0A|nvm_spm|SPM Z+     \n RET
|$0E|nvm_cmd|(function)

These can be used to erase/rewrite the FLASH in the CODE/APPEND and BOOTROW areas using the BOOT area protection privilege.

> For actual usage examples, see [[FlashNVM Tool Reference]](https://github.com/askn37/askn37.github.io/wiki/FlashNVM).

## Copyright and Contact

Twitter(X): [@askn37](https://twitter.com/askn37) \
BlueSky Social: [@multix.jp](https://bsky.app/profile/multix.jp) \
GitHub: [https://github.com/askn37/](https://github.com/askn37/) \
Product: [https://askn37.github.io/](https://askn37.github.io/)

Copyright (c) askn (K.Sato) multix.jp \
Released under the MIT license \
[https://opensource.org/licenses/mit-license.php](https://opensource.org/licenses/mit-license.php) \
[https://www.oshwa.org/](https://www.oshwa.org/)
