/**
 * @file main.cpp
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

#ifndef F_CPU
  #define F_CPU 20000000L
#endif
#include <avr/io.h>
#include <stddef.h>
#include "api/macro_api.h"  /* interrupts, initVariant */
#include "peripheral.h"     /* import Serial (Debug) */
#include "configuration.h"
#include "prototype.h"

// MARK: Work space

namespace /* NAMELESS */ {

  /* USB */
  alignas(2) NOINIT EP_TABLE_t EP_TABLE;
  alignas(2) NOINIT EP_DATA_t EP_MEM;
  NOINIT Device_Desc_t Device_Descriptor;

  /* JTAG packet payload */
  NOINIT JTAG_Packet_t packet;
  NOINIT size_t  _packet_length;
  NOINIT uint8_t _packet_fragment;
  NOINIT uint8_t _packet_chunks;
  NOINIT uint8_t _packet_endfrag;

  /* JTAG parameter */
  NOINIT uint8_t _jtag_arch;
  NOINIT uint8_t _jtag_conn;
  NOINIT uint32_t _before_page;

  /* SYSTEM */
  NOINIT uint16_t _bootsize;
  NOINIT uint8_t _set_config;
  NOINIT uint8_t _led_bits;
  NOINIT uint8_t _led_next;
  NOINIT uint8_t _led_mask;

} /* NAMELESS */;

// MARK: Startup and Vectors Overload

/* This section will be placed at the beginning of  */
/* the output binary if built with `-nostartfiles`. */
/* Provides an SPM snippet compatible with bootloaders 3.71. */

__attribute__((used))
int main (void);

__attribute__((used))
__attribute__((section (".vectors")))
void nvm_cmd (uint8_t /* _nvm_cmd */) {
  /* R24 <- _nvm_cmd */
  __asm__ __volatile__ (
    R"#ASM#(
      1:  LDS   R25, %0
          ANDI  R25, 3
          BRNE  1b
          LDI   R25, 0x9D
          OUT   0x34, R25
          STS   %1, __zero_reg__
          OUT   0x34, R25
          STS   %1, R24
    )#ASM#"
    :: "p" (_SFR_MEM_ADDR(NVMCTRL_STATUS))
    ,  "p" (_SFR_MEM_ADDR(NVMCTRL_CTRLA))
    : "r25"
  );
}

__attribute__((used))
__attribute__((naked))
__attribute__((noinline))
__attribute__((noreturn))
__attribute__((section (".vectors")))
void vectors (void) {
  __asm__ __volatile__ (
    R"#ASM#(
      RJMP  main      ; $0000
      ST    Z+, R22   ; $0002 nvm_stz
      RET
      LD    R24, Z+   ; $0006 nvm_ldz
      RET
      SPM   Z+        ; $000A nvm_spm
      RET
    )#ASM#"
  );
  /* next is nvm_cmd */
}

// MARK: main function

int main (void) {

  /*** Startup Section ***/

  /* This is the first code that is executed.
     According to modernAVR specifications,
     interrupts are disabled and SP points to RAMEND. */

  /* Known-zero required by avr-libc. */
  __asm__ __volatile__ ( "CLR __zero_reg__" );

  GPR_GPR0 = RSTCTRL_RSTFR; /* get reset cause */
  RSTCTRL_RSTFR = GPR_GPR0; /* clear flags */

  pinControlRegister(PIN_SYS_SW0) = PORT_PULLUPEN_bm;

  /* If register is zero, perform software reset */
  if (GPR_GPR0 == 0) _PROTECTED_WRITE(RSTCTRL_SWRR, 1);

  _bootsize = FUSE_BOOTSIZE << 9;   /* x PROGMEM_PAGE_SIZE(512) */

  /* WDT restart causes user code to execute */
  if (bit_is_set(GPR_GPR0, RSTCTRL_WDRF_bp) || digitalReadMacro(PIN_SYS_SW0)) {
    pinControlRegister(PIN_SYS_SW0) = 0;
    __asm__ __volatile__ ( "IJMP" :: "z" (_bootsize / 2) );
  }

  /*** This is where the actual processing begins. ***/

  /* setting F_CPU == 20MHz */
  _PROTECTED_WRITE(CLKCTRL_OSCHFCTRLA, CLKCTRL_FRQSEL_20M_gc);

  pinMode(PIN_SYS_LED0, OUTPUT);
#if PIN_SYS_LED0 == PIN_PF2
  digitalWriteMacro(PIN_SYS_LED0, TOGGLE);
#endif
  digitalWriteMacro(PIN_SYS_LED0, TOGGLE);

#if defined(DEBUG)
  Serial.begin(CONSOLE_BAUD);
  delay_millis(600);
  D1PRINTF("\n<startup>\r\n");
  D1PRINTF("F_CPU = %d\r\n", F_CPU);
  D1PRINTF("_AVR_IOXXX_H_ = " _AVR_IOXXX_H_ "\r\n");
  D1PRINTF("__AVR_ARCH__ = %d\r\n", __AVR_ARCH__);
  D1PRINTF("BOOTSIZE = %d, 0x%04X\r\n", FUSE_BOOTSIZE, _bootsize);
  DFLUSH();
#endif

  _led_next = 0b11000000;
  _led_mask = 0;

  TCA0_SINGLE_PER = F_CPU / 1024 / 12;
  TCA0_SINGLE_CTRLA = TCA_SINGLE_ENABLE_bm | TCA_SINGLE_CLKSEL_DIV1024_gc;

  loop_until_bit_is_clear(WDT_STATUS, WDT_SYNCBUSY_bp);
  _PROTECTED_WRITE(WDT_CTRLA, WDT_PERIOD_1KCLK_gc);

  SYSCFG_VUSBCTRL = SYSCFG_USBVREG_bm;

  SYS::delay_125ms();
  SYS::delay_125ms();
  USB::setup_device(true);

  digitalWriteMacro(PIN_SYS_LED0, TOGGLE);
  D1PRINTF("<WAITING>\r\n");
  DFLUSH();
  while (true) {
    DFLUSH();
    if (bit_is_clear(GPCONF, GPCONF_FAIL_bp)) wdt_reset();

    if (bit_is_set(TCA0_SINGLE_INTFLAGS, TCA_SINGLE_CMP0_bp)) {
      bit_set(TCA0_SINGLE_INTFLAGS, TCA_SINGLE_CMP0_bp);
      if (_led_mask) _led_mask >>= 1;
      else {
        _led_bits = _led_next;
        _led_mask = 0x80;
      }
      if (_led_bits & _led_mask) digitalWriteMacro(PIN_SYS_LED0, TOGGLE);
    }

    USB::handling_bus_events();
    if (USB::is_ep_setup()) USB::handling_control_transactions();

    if (bit_is_clear(GPCONF, GPCONF_USB_bp)) continue;

    if (USB::is_not_dap()) continue;

    if (JTAG::dap_command_check()) JTAG::jtag_scope_branch();
  }
}

// end of code
