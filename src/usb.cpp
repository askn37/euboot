/**
 * @file usb.cpp
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
#include <avr/pgmspace.h>   /* PROGMEM memcpy_P */
#include <string.h>         /* memcpy */
#include "api/macro_api.h"  /* ATOMIC_BLOCK */
#include "api/capsule.h"    /* _CAPS macro */
#include "peripheral.h"     /* import Serial (Debug) */
#include "configuration.h"
#include "prototype.h"

/*
 * NOTE:
 *
 * USB VID:PID pair value
 *
 *  The default value is MCHP:TEST (04D8:002F)
 */

namespace USB {

  // MARK: Descroptor

  const wchar_t PROGMEM mstring[] = L"euboot:CMSIS-DAP:EDBG";

  const uint8_t PROGMEM device_descriptor[] = {
    /* This device descriptor contains. */
    0x12, 0x01, 0x00, 0x02, 0x00, 0x00, 0x00, 0x40,
    CONFIG_USB_VIDPID,      0x00, 0x01, 0x00, 0x02, 0x03, 0x01
  };
  const uint8_t PROGMEM qualifier_descriptor[] = {
    /* This descriptor selects Full-Speed (USB 2.0) ​​for USB 3.0. */
    0x0A, 0x06, 0x00, 0x02, 0xEF, 0x02, 0x01, 0x40, 0x00, 0x00
  };
  const uint8_t PROGMEM current_descriptor[] = {
    /* This descriptor is almost identical to the Xplained Mini series. */
    /* It does not allow for an dWire gateway. */
    0x09, 0x02, 0x29, 0x00, 0x01, 0x01, 0x00, 0x00, 0x32, /* Information Set#1 */
    0x09, 0x04, 0x00, 0x00, 0x02, 0x03, 0x00, 0x00, 0x00, /* Interface #0 HID  */
    0x09, 0x21, 0x10, 0x01, 0x00, 0x01, 0x22, 0x23, 0x00, /*   HID using       */
    0x07, 0x05, 0x02, 0x03, 0x40, 0x00, 0x01,             /*   EP_DPO_OUT 0x02 */
    0x07, 0x05, 0x81, 0x03, 0x40, 0x00, 0x01,             /*   EP_DPI_IN  0x81 */
  };
  const uint8_t PROGMEM report_descriptor[] = {
    /* This descriptor defines a HID report. */
    /* The maximum buffer size allowed in Full-Speed (USB 2.0) mode is 64 bytes. */
    0x06, 0x00, 0xFF, 0x09, 0x01, 0xA1, 0x01, 0x15,
    0x00, 0x26, 0xFF, 0x00, 0x75, 0x08, 0x96, 0x40,
    0x00, 0x09, 0x01, 0x81, 0x02, 0x96, 0x40, 0x00,
    0x09, 0x01, 0x91, 0x02, 0x95, 0x04, 0x09, 0x01,
    0xB1, 0x02, 0xC0
  };

  const EP_TABLE_t PROGMEM ep_init = {
    { /* EP */
      { /* EP_REQ */
        { 0,
          USB_TYPE_CONTROL_gc                                 | USB_TCDSBL_bm | USB_BUFSIZE_DEFAULT_BUF64_gc,
          0, (uint16_t)&EP_MEM.req_data, 0 },
        /* EP_RES */
        { 0,
          USB_TYPE_CONTROL_gc | USB_MULTIPKT_bm | USB_AZLP_bm | USB_TCDSBL_bm | USB_BUFSIZE_DEFAULT_BUF64_gc,
          0, (uint16_t)&EP_MEM.res_data, 0 },
      },
      { /* EP_DPI */
        { /* not used */ },
        { 0,
          USB_TYPE_BULKINT_gc | USB_MULTIPKT_bm | USB_AZLP_bm | USB_TCDSBL_bm | USB_BUFSIZE_DEFAULT_BUF64_gc,
          64, (uint16_t)&EP_MEM.dap_data, 0 },
      },
      { /* EP_DPO */
        { 0,
          USB_TYPE_BULKINT_gc                                 | USB_TCDSBL_bm | USB_BUFSIZE_DEFAULT_BUF64_gc,
          0, (uint16_t)&EP_MEM.dap_data, 64 },
        { /* not used */ },
      },
    },
  };

  size_t get_descriptor (uint8_t* _buffer, uint16_t _index) {
    uint8_t* _pgmem = 0;
    size_t   _size = 0;
    uint8_t  _type = _index >> 8;
    if (_type == 0x01) {          /* DEVICE */
      _pgmem = (uint8_t*)&device_descriptor;
      _size = sizeof(device_descriptor);
      memcpy_P(_buffer, _pgmem, _size);
      D1PRINTF(" VID:PID=%04X:%04X\r\n", _CAPS16(_buffer[8])->word, _CAPS16(_buffer[10])->word);
      return _size;
    }
    else if (_type == 0x02) {     /* CONFIGURATION */
      _pgmem = (uint8_t*)&current_descriptor;
      _size = sizeof(current_descriptor);
    }
    else if (_type == 0x06) {     /* QUALIFIER */
      _pgmem = (uint8_t*)&qualifier_descriptor;
      _size = sizeof(qualifier_descriptor);
    }
    else if (_type == 0x21) {     /* HID */
      _pgmem = (uint8_t*)&current_descriptor + 18;
      _size = 9;
    }
    else if (_type == 0x22) {     /* REPORT */
      _pgmem = (uint8_t*)&report_descriptor;
      _size = sizeof(report_descriptor);
    }
    else if (_index == 0x0300) {  /* LANGUAGE */
      _size = 4;
      *_buffer++ = 4;
      *_buffer++ = 3;
      *_buffer++ = 0x09;
      *_buffer++ = 0x04;
      return _size;
    }
    else {
      switch (_index) {
        case 0x0302:
        case 0x0303:
          _pgmem = (uint8_t*)&mstring;
          _size = sizeof(mstring);
          break;
      }
      *_buffer++ = (uint8_t)_size;
      *_buffer++ = 3;
      if (_size) memcpy_P(_buffer, _pgmem, _size - 2);
      return _size;
    }
    if (_size) memcpy_P(_buffer, _pgmem, _size);
    return _size;
  }

  void setup_device (bool _force) {
    USB0_ADDR = 0;
    if (USB0_CTRLA || _force) {
      USB0_CTRLA = 0;
      USB0_FIFOWP = 0;
      USB0_EPPTR = (uint16_t)&EP_TABLE.EP;
      USB0_CTRLB = USB_ATTACH_bm;
      GPCONF = 0;
      _set_config = 0;
      memcpy_P(&EP_TABLE, &ep_init, sizeof(EP_TABLE_t));
      USB0_CTRLA = USB_ENABLE_bm | (USB_ENDPOINTS_MAX - 1);
    }
  }

  // MARK: Endpoint

  bool is_ep_setup (void) { return bit_is_set(EP_REQ.STATUS, USB_EPSETUP_bp); }
  bool is_not_dap (void) { return bit_is_clear(EP_DPO.STATUS, USB_BUSNAK_bp); }
  void ep_req_pending (void) { loop_until_bit_is_set(EP_REQ.STATUS, USB_BUSNAK_bp); }
  void ep_res_pending (void) { loop_until_bit_is_set(EP_RES.STATUS, USB_BUSNAK_bp); }
  void ep_dpi_pending (void) { loop_until_bit_is_set(EP_DPI.STATUS, USB_BUSNAK_bp); }
  void ep_dpo_pending (void) { loop_until_bit_is_set(EP_DPO.STATUS, USB_BUSNAK_bp); }

  void ep_req_listen (void) {
    EP_REQ.CNT = 0;
    loop_until_bit_is_clear(USB0_INTFLAGSB, USB_RMWBUSY_bp);
    USB_EP_STATUS_CLR(USB_EP_REQ) = ~USB_TOGGLE_bm;
  }

  void ep_res_listen (void) {
    EP_RES.MCNT = 0;
    loop_until_bit_is_clear(USB0_INTFLAGSB, USB_RMWBUSY_bp);
    USB_EP_STATUS_CLR(USB_EP_RES) = ~USB_TOGGLE_bm;
  }

  void ep_dpi_listen (void) {
    EP_DPI.CNT = 64;
    EP_DPI.MCNT = 0;
    loop_until_bit_is_clear(USB0_INTFLAGSB, USB_RMWBUSY_bp);
    USB_EP_STATUS_CLR(USB_EP_DPI) = ~USB_TOGGLE_bm;
  }

  void ep_dpo_listen (void) {
    EP_DPO.CNT = 0;
    loop_until_bit_is_clear(USB0_INTFLAGSB, USB_RMWBUSY_bp);
    USB_EP_STATUS_CLR(USB_EP_DPO) = ~USB_TOGGLE_bm;
  }

  void complete_dap_out (void) {
    ep_dpi_listen();
    ep_dpo_listen();  /* continue transaction */
  }

  // MARK: USB Session

  /*** USB Standard Request Enumeration. ***/
  bool request_standard (void) {
    bool _listen = true;
    uint8_t bRequest = EP_MEM.req_data.bRequest;
    if (bRequest == 0x00) {       /* GET_STATUS */
      EP_MEM.res_data[0] = 0;
      EP_MEM.res_data[1] = 0;
      EP_RES.CNT = 2;
    }
    else if (bRequest == 0x01) {  /* CLEAR_FEATURE */
      D1PRINTF(" CF=%02X:%02X\r\n", EP_MEM.req_data.wValue, EP_MEM.req_data.wIndex);
      if (0 == (uint8_t)EP_MEM.req_data.wValue) {
        /* Expects an endpoint number to be passed in. Swaps the high and low */
        /* nibbles to make it a representation of the USB controller. */
        uint8_t _EP = USB_EP_ID_SWAP(EP_MEM.req_data.wIndex);
        loop_until_bit_is_clear(USB0_INTFLAGSB, USB_RMWBUSY_bp);
        USB_EP_STATUS_CLR(_EP) = USB_STALLED_bm | USB_BUSNAK_bm | USB_TOGGLE_bm;
      }
      EP_RES.CNT = 0;
    }
    else if (bRequest == 0x04) {  /* SET_FEATURE */
      /* If used, it will be ignored. */
      D1PRINTF(" SF=%02X:%02X\r\n", EP_MEM.req_data.wValue, EP_MEM.req_data.wIndex);
      EP_RES.CNT = 0;
    }
    else if (bRequest == 0x05) {  /* SET_ADDRESS */
      uint8_t _addr = EP_MEM.req_data.wValue & 0x7F;
      ep_res_listen();
      ep_res_pending();
      USB0_ADDR = _addr;
      D1PRINTF(" USB0_ADDR=%d\r\n", _addr);
      EP_RES.CNT = 0;
    }
    else if (bRequest == 0x06) {  /* GET_DESCRIPTOR */
      size_t _length = EP_MEM.req_data.wLength;
      size_t _size = get_descriptor((uint8_t*)&EP_MEM.res_data, EP_MEM.req_data.wValue);
      EP_RES.CNT = (_size > _length) ? _length : _size;
      _listen = !!_size;
    }
    else if (bRequest == 0x08) {  /* GET_CONFIGURATION */
      EP_MEM.res_data[0] = _set_config;
      D1PRINTF("<GC:%02X>\r\n", _set_config);
      EP_RES.CNT = 1;
    }
    else if (bRequest == 0x09) {  /* SET_CONFIGURATION */
      /* Once the USB connection is fully initiated, it will go through here. */
      _set_config = (uint8_t)EP_MEM.req_data.wValue;
      bit_set(GPCONF, GPCONF_USB_bp);
      _led_next = 0b11110000;
      D1PRINTF("<READY:%02X>\r\n", _set_config);
      EP_RES.CNT = 0;
    }
    else if (bRequest == 0x0A) {  /* GET_INTREFACE */
      /* It seems not to be used. */
      D1PRINTF("<SI:0>\r\n");
      EP_MEM.res_data[0] = 0;
      EP_RES.CNT = 1;
    }
    else if (bRequest == 0x0B) {  /* SET_INTREFACE */
      /* It seems not to be used. */
      D1PRINTF("<GI:%02X>\r\n", EP_MEM.req_data.wValue);
      EP_RES.CNT = 0;
    }
    else {
      D2PRINTF(" RQ=%02X\r\n", bRequest);
      _listen = false;
    }
    return _listen;
  }

  /*** class request processing. ***/
  bool request_class (void) {
    bool _listen = true;
    uint8_t bRequest = EP_MEM.req_data.bRequest;
    if (bRequest == 0x0A) {       /* SET_IDLE */
      /* This is a HID Compliance Class Request. */
      /* It is called but not used. */
      D1PRINTF(" IDL=%02X\r\n", (uint8_t)EP_MEM.req_data.wValue);
      EP_RES.CNT = 0;
    }
    else {
      _listen = false;
    }
    return _listen;
  }

  /*** Accept the EP0 setup packet. ***/
  /* This process is equivalent to a endpoint interrupt. */
  /* The reason for using polling is to prioritize VCP performance. */
  void handling_control_transactions (void) {
    bool _listen = false;
    uint8_t bmRequestType = EP_MEM.req_data.bmRequestType;
    D2PRINTF("RQ=%02X:%04X:%02X:%02X:%04X:%04X:%04X\r\n",
      EP_REQ.STATUS, EP_REQ.CNT, EP_MEM.req_data.bmRequestType, EP_MEM.req_data.bRequest,
      EP_MEM.req_data.wValue, EP_MEM.req_data.wIndex, EP_MEM.req_data.wLength);
    /* Accepts subsequent EP0_DATA packets as needed. */
    if (bit_is_clear(bmRequestType, 7)) ep_req_listen();
    bmRequestType &= (3 << 5);
    if (bmRequestType == (0 << 5)) {
      _listen = request_standard();
    }
    else if (bmRequestType == (1 << 5)) {
      _listen = request_class();
    }
    if (_listen) {
      ep_res_listen();
      ep_req_listen();
    }
    USB0_INTFLAGSB |= USB_EPSETUP_bp;
  }

  /*** This process is equivalent to a bus interrupt. ***/
  /* The reason for using polling is to prioritize VCP performance. */
  /* The trade-off is that power standby is not available. */
  void handling_bus_events (void) {
    uint8_t busstate = USB0_INTFLAGSA;
    USB0_INTFLAGSA = busstate;
    if (bit_is_set(busstate, USB_RESUME_bp)) {
      /* This implementation does not transition to power saving mode. */
      /* This is only passed when the USB cable is unplugged. */
      if (bit_is_set(GPCONF, GPCONF_USB_bp)) {
        D1PRINTF("<BUS=%02X>\r\n", busstate);
        DFLUSH();
        /* System reboot */
        SYS::reboot();
      }
      bit_set(busstate, USB_RESET_bp);
    }
    if (bit_is_set(busstate, USB_RESET_bp)) {
      setup_device(false);
    }
  }

};

// end of code
