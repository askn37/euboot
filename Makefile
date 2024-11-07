TARGET = euboot

MF := $(MAKEFILE_LIST)

### You can change the built-in LEDs and switches here. ###

BUILTIN_LA7_SF6 = --build-property "build.led_pin=-DLED_BUILTIN=PIN_PA7 -DSW_BUILTIN=PIN_PF6"
BUILTIN_LC3_SF6 = --build-property "build.led_pin=-DLED_BUILTIN=PIN_PC3 -DSW_BUILTIN=PIN_PF6"
BUILTIN_LF2_SF6 = --build-property "build.led_pin=-DLED_BUILTIN=PIN_PF2 -DSW_BUILTIN=PIN_PF6"

### arduino-cli @1.0.x is required. ###

ACLIPATH =
SDKURL = --additional-urls https://askn37.github.io/package_multix_zinnia_index.json
FQBN = --fqbn MultiX-Zinnia:modernAVR:AVRDU_noloader:01_variant=22_AVR64DU32,02_clock=11_20MHz,11_BODMODE=01_disabled,12_BODLVL=BODLEVEL0,21_resetpin=02_gpio,22_updipin=01_updi,24_eeprom=01_keep,25_bootrow=01_erase,26_userrow=02_keep,27_fusefile=03_upload,51_buildopt=01_Release,52_macroapi=02_Withoutboot,53_printf=01_default,90_console_baud=14_500000bps,95_bootloader=00_woBootloader,54_console_select=03_UART1_D6_LC3

# If you have MPLAB installed, the executable path will already be there.
# If not, you should specify the path to the Arduino tools.

AVRROOT =
OBJCOPY = $(AVRROOT)avr-objcopy
OBJDUMP = $(AVRROOT)avr-objdump
LISTING = $(OBJDUMP) -S

JOINING  = -j .text -j .data -j --set-section-flags

### If Perl is on the execution path, CRC32 padding will be performed. ###

PERL := $(shell which perl)
GENCRC = gencrc.pl
GENCRCOPT = -u -c6

### Make rule ###

hex/$(TARGET)%.hex: build/$(TARGET)%.ino.elf
ifneq ($(PERL),'')
	@$(OBJCOPY) $(JOINING) -O binary $< build/$(TARGET)$*.tmp
	@$(PERL) $(GENCRC) $(GENCRCOPT) -i build/$(TARGET)$*.tmp -o $@
else
	@$(OBJCOPY) $(JOINING) -O ihex $< $@
endif
	@# @$(OBJCOPY) -I ihex -O binary $@ hex/$(TARGET)$*.bin
	@# $(LISTING) $< > hex/$(TARGET)$*.lst
	@cp -f build/$(TARGET).ino.fuse hex/$(TARGET)$*.fuse
	ls -la hex/$(TARGET)$*.*

build/$(TARGET)_LA7_SF6.ino.elf: src/$(SRCS:.cpp=.o) src/$(SRCS:.c=.o)
	$(ACLIPATH)arduino-cli compile $(FQBN) $(BUILTIN_LA7_SF6) $(SDKURL) --build-path build --no-color
	@mv -f build/$(TARGET).ino.elf $@

build/$(TARGET)_LC3_SF6.ino.elf: src/$(SRCS:.cpp=.o) src/$(SRCS:.c=.o)
	$(ACLIPATH)arduino-cli compile $(FQBN) $(BUILTIN_LC3_SF6) $(SDKURL) --build-path build --no-color
	@mv -f build/$(TARGET).ino.elf $@

build/$(TARGET)_LF2_SF6.ino.elf: src/$(SRCS:.cpp=.o) src/$(SRCS:.c=.o)
	$(ACLIPATH)arduino-cli compile $(FQBN) $(BUILTIN_LF2_SF6) $(SDKURL) --build-path build --no-color
	@mv -f build/$(TARGET).ino.elf $@

clean:
	@touch ./build/__temp
	rm -rf ./build/*

all: hex/$(TARGET)_LA7_SF6.hex hex/$(TARGET)_LC3_SF6.hex hex/$(TARGET)_LF2_SF6.hex clean

# end of script