/**
 * @file sys.cpp
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
#include "api/macro_api.h"  /* interrupts, initVariant */
#include "peripheral.h"     /* import Serial (Debug) */
#include "prototype.h"

#define pinLogicPush(PIN) openDrainWriteMacro(PIN, LOW)
#define pinLogicOpen(PIN) openDrainWriteMacro(PIN, HIGH)

namespace SYS {

  /*
   * System reboot
   *
   * Always run it after the USB has stopped.
   */
  void reboot (void) {
    D0PRINTF("<REBOOT>\r\n");
    DFLUSH();
    _PROTECTED_WRITE(RSTCTRL_SWRR, 1);
  }

  /*
   * Measure self operating voltage.
   *
   * Vdd/10 goes into MUXPOS and is divided by the internal reference voltage of 1.024V.
   * A delay of 1250us is required for the voltage to stabilize.
   * The result is 10-bit, so multiply by 10.0 to convert to 1V * 0.0001.
   * The ADC0 peripheral is operational only during voltage measurements.
   */
  uint16_t get_vdd (void) {
    CLKCTRL_MCLKTIMEBASE = F_CPU / 1000000.0;
    ADC0_INTFLAGS = ~0;
    ADC0_SAMPLE = 0;
    ADC0_CTRLA = ADC_ENABLE_bm;
    ADC0_CTRLB = ADC_PRESC_DIV4_gc;
    ADC0_CTRLC = ADC_REFSEL_1V024_gc;
    ADC0_CTRLE = 250; /* (SAMPDUR + 0.5) * fCLK_ADC sample duration */
    ADC0_MUXPOS = ADC_MUXPOS_VDDDIV10_gc; /* ADC channel VDD * 0.1 */
    loop_until_bit_is_clear(ADC0_STATUS, ADC_ADCBUSY_bp);
    ADC0_COMMAND = ADC_MODE_SINGLE_10BIT_gc | ADC_START_IMMEDIATE_gc;
    loop_until_bit_is_set(ADC0_INTFLAGS, ADC_SAMPRDY_bp);
    uint16_t _adc_reading = ADC0_SAMPLE;
    _adc_reading += (_adc_reading << 3) + _adc_reading;
    ADC0_CTRLA = 0;
    return _adc_reading;
  }

  void delay_55us (void) {
    delay_micros(55);
  }

  void delay_100us (void) {
    delay_micros(100);
  }

  void delay_800us (void) {
    delay_micros(800);
  }

  void delay_2500us (void) {
    delay_micros(2500);
  }

  void delay_125ms (void) {
    delay_millis(125);
  }

};

// end of code
