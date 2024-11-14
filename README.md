# euboot : EDBG USB bootloader for AVR-DU series

*Switching document languages* : [日本語](README_jp.md), __English__

## Features

- USB bootloader for AVR-DU series.
- Uses standard USB-HID/CMSIS-DAP/EDBG protocols and is recognized as a `jtag3updi` device by AVRDUDE<=8.0.
- Fast memory read/write speeds, close to the limits of USB Full-Speed.
- Footprint is less than 2.5KiB.

## Reasons for development

The AVR-DU series is the only current AVR generation with built-in USB peripherals, but unlike previous similar products (ATMEL generations), it does not ship with a DFU bootloader and the bare metal chip flash is always empty.

This is partly because ATMEL was a regular member of the USB-IF DFU standard development, whereas Microchip was not. Therefore, it is unlikely that DFU support will be provided in the future.

Currently, AVRDUDE has two alternative bootloader approaches: USB-CDC and USB-HID.

CDC (VCP, VCOM) is popular in many implementations, but it has many drawbacks in exchange for a few advantages.
The USB protocol and lower layers are hidden, and the stream is byte-oriented, so it is difficult to read.
There is no protection against packet loss, so there is no protection against intrusion of invalid data.
To make matters worse, new patches are required for AVRDUDE to properly support the new chip's memory.
It is hardly an attractive choice for new development.

On the other hand, bulk transfer communication using USB-HID is not used much because it requires USB expertise.
But now we have [UPDI4AVR-USB](https://github.com/askn37/UPDI4AVR-USB/), which supports CMSIS-DAP and EDBG protocols and can also handle `jtag3updi`. It is easy to implement only the necessary parts based on this and add NVM control.
The result was good, and I was able to create a USB bootloader that supports `jtag3updi` with a footprint of 2.5KiB.
It's not as small as DFU, but it's compact enough for practical use.
Also, the stream is block-oriented, so there is almost no slowdown in either read or write compared to the native speed of the USB protocol.

## What you need to create the Bootloader Firmware

To do this, you need the following environment:

#### [MultiX Zinnia Product SDK [modernAVR] @0.3.0+](https://github.com/askn37/multix-zinnia-sdk-modernAVR)

Bare metal development SDK that can be easily installed with the Arduino IDE/CLI board manager. It has various macros that make AVR-LIBC easier to use, and allows you to write low-level code with a similar feel to Arduino-API. AVRDUDE 8.0+ is also installed at the same time.

#### [Arduino-CLI @1.0.3+](https://arduino.github.io/arduino-cli/1.0/installation/)

This is required if you want to build the firmware using the included Makefile.
It is possible to output binary file using only the modernAVR SDK without using this, but the menu settings will become more complicated.

> [!TIP]
> In a Windows environment, you cannot use `make` as is. You need to use WSL or some other method.

#### UPDI compatible programmer for AVR-DU series

This mainly applies to `PICKit4`. However, the most readily available and cheapest one in the world is ["AVR64DU32 Curiosity Nano : EV59F82A"](https://www.microchip.com/en-us/development-tool/ev59f82a). Get this and install [UPDI4AVR-USB](https://github.com/askn37/UPDI4AVR-USB/). From AVRDUDE, you can use it as a UPDI programmer like `PICKit4`. For details, please refer to each link.

## Creating and Installing the Bootloader Firmware

Install the modernAVR SDK, add Arduino-CLI and AVRDUDE 8.0 to your path, and when you're ready, go to the `euboot` directory and run `make all`. The generated files will be saved in the `hex` directory.

```sh
euboot $ make all
```

> [!TIP]
> If you have a `Perl5` executable, the hex and bin files will have an embedded CRC32 for use with the `CRCSCAN` peripheral.
> By modifying FUSE to use this, it is possible to stop normal operation of the MCU if the bootloader reserved area is tampered with.

Upload the resulting file to the target. In this example, the target is "CURIOSITY NANO". This is easy because `pkobn_updi` is built in.

```sh
euboot $ avrdude -cpkobn_updi -pavr64du32 -Uflash:w:hex/euboot_LF2_SF6.hex:i -Ufuses:w:hex/euboot_LF2_SF6.fuse:i
```

After the bootloader is uploaded successfully, the user application area is still empty, so the device will repeatedly reset itself.
If you press `SW0 (PF6)` once, the `LED (PF2)` will start flashing in the following pattern.
This means that USB enumeration with the host PC is not yet complete.

- LED(PF2): 🟠⚫️⚫️⚫️ (Waiting for enumeration)

When you connect a second USB cable to the target USB port or switch the USB cable from the debugger port, the LED changes to the following blinking pattern, indicating that it is ready for EDBG protocol communication to begin.

- LED(PF2): 🟠🟠⚫️⚫️ (Standby mode)

Ready to go? Now let's check if the USB bootloader responds correctly. Note the `-P` option.

```sh
avrdude -Pusb:04d8:0b12 -cjtag3updi -pavr64du32 -v -Usib:r:-:r
```

```console
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
Reading sib memory ...
Writing 32 bytes to output file <stdout>
AVR     P:4D:1-3M2 (EDBG.Boot.)
Avrdude done.  Thank you.
```

Firmware specific information is listed under `ICE HW Version`. The item names and contents do not match by design, because AVRDUDE does not have the ability to display specific information, so unused items are used instead.

- __ICE HW version__: It is always 52. This is originally a character code for `4` and indicates the version of the NVM controller.
- __ICE FW version__: Shows the version and update number of the USB bootloader.
- __Serial number__: This represents the USB device product string. The USB serial number string is not used and both are the same string.
- __Vtarget__: Displays the operating voltage supplied to VDD.
- __PDI/UPDI clk__: This is not the actual operating speed, but the program start address of the user application calculated from the `FUSE_BOOTSIZE` setting. For example, `2560` indicates that the user application will run from program start address `0x0A00`. If it is `0`, the required FUSEs are not configured correctly.

A user application written to run from a specific address can be written to the standby USB bootloader as follows. Using the `-D` option is recommended. The LED blinking during memory read/write will change as follows:

- LED(PF2): 🟠🟠🟠🟠 (Communication in progress)

```sh
# `USERAPP.ino` must be built with 'build.text_section_start=.text=0xA00'
$ avrdude -Pusb:04d8:0b12 -cjtag3updi -pavr64du32 -v -D -Uflash:w:USERAPP.ino.hex:i
```

Similarly, the `-U` option can be used to write and read `eeprom`, `userrow`, and `bootrow`.
`fuse(s)` and `lock` are read-only and cannot be modified.

> [!TIP]
> Once the `LED(PF2)` is lit, if you unplug the USB cable, a self-reset will occur and the user application will start running.
> The bootloader itself does not have a timeout, so the normal way to stop the bootloader is to try writing a sketch or to unplug the USB cable.

## Activating the USB bootloader

To activate the USB bootloader, you must hold down `SW0 (PF6)` while powering on the AVR-DU.
If successful, the LED will flash in standby mode.

With the default `FUSES` settings, `SW0 (PF6)` does not function as a hard reset switch, but is a regular GPIO input.
To use it as a reset switch, the user application must implement the necessary code.

The user application can implement two reset methods: WDT operation and SWRST operation.
The WDT reset always activates the user application and ignores the bootloader activation switch.
The SWRST reset activates the USB bootloader if the bootloader activation switch is LOW.

> [!TIP]
> If you are testing with "CURIOSITY NANO", `SW0 (PF6)` does not function as a hard reset switch, but the debugger is running, so you can perform a remote `UPDI` reset.
> So just execute the following command. If you hold down `SW0 (PF6)` before that, the bootloader will be activated.
> This is a convenient way to avoid having to unplug and replug the USB cable. \
> `avrdude -cpkobn_updi -pavr64du32`

## SPM snippets

The first address of the USB bootloader starts at `PROGMEM_START` in the `PROGMEM` area. It contains a special magic number and the SPM snippet code.

|Series|Address|Magic number: uint32_t (LE)|
|-|-|-|
|AVR_DA/DB/DD/DU/EA/EB|PROGMEM_START + 2 bytes|0x95089361|

> [!TIP] 
> `(pgm_read_dword(PROGMEM_START + 2) == 0x95089361L`

|Offset|HWV=52|OP code|
|-|-|-|
|$02|nvm_stz|ST Z+, R22 \n RET
|$06|nvm_ldz|LD R24, Z+ \n RET
|$0A|nvm_spm|SPM Z+     \n RET
|$0E|nvm_cmd|(function)

These can be used to erase/rewrite the FLASH in the CODE/APPEND and BOOTROW areas using the BOOT area protection privilege.

> For actual usage examples, see [[FlashNVM Tool Reference]](https://github.com/askn37/askn37.github.io/wiki/FlashNVM).

## Related link and documentation

- [UPDI4AVR-USB](https://github.com/askn37/UPDI4AVR-USB) : OSS/OSHW Programmer for UPDI/TPI/PDI
- [AVRDUDE](https://github.com/avrdudes/avrdude) @8.0+ (AVR-DU series is officially supported from 8.0 onwards)

## Copyright and Contact

Twitter(X): [@askn37](https://twitter.com/askn37) \
BlueSky Social: [@multix.jp](https://bsky.app/profile/multix.jp) \
GitHub: [https://github.com/askn37/](https://github.com/askn37/) \
Product: [https://askn37.github.io/](https://askn37.github.io/)

Copyright (c) askn (K.Sato) multix.jp \
Released under the MIT license \
[https://opensource.org/licenses/mit-license.php](https://opensource.org/licenses/mit-license.php) \
[https://www.oshwa.org/](https://www.oshwa.org/)
