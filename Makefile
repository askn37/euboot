###
### make all
###

TARGET = euboot

MF := $(MAKEFILE_LIST)

### You can change the built-in LEDs and switches here. ###

BUILDOPT = --build-property "build.buildopt=-DNDEBUG=1" 
BUILTIN_LA7_SF6 = $(BUILDOPT) --build-property "build.console_select=-DSerial=Serial0A -DLED_BUILTIN=PIN_PA7 -DSW_BUILTIN=PIN_PF6"
BUILTIN_LC3_SF6 = $(BUILDOPT) --build-property "build.console_select=-DSerial=Serial1C -DLED_BUILTIN=PIN_PC3 -DSW_BUILTIN=PIN_PF6"
BUILTIN_LF2_SF6 = $(BUILDOPT) --build-property "build.console_select=-DSerial=Serial1C -DLED_BUILTIN=PIN_PF2 -DSW_BUILTIN=PIN_PF6"

### arduino-cli @1.0.x is required. ###

ACLIPATH =
SDKURL = --additional-urls https://askn37.github.io/package_multix_zinnia_index.json

# FQBN : You only need to specify the menu items that differ from the defaults.

FQBN = --fqbn "MultiX-Zinnia:modernAVR:AVRDU_noloader:\
  01_variant=22_AVR64DU32,\
	02_clock=11_20MHz,\
	21_resetpin=02_gpio,\
	27_fusefile=03_upload,\
	52_macroapi=02_Withoutboot"

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
ifneq ($(PERL),)
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