/**
 * @file jtag.cpp
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
#include "api/btools.h"     /* bswap16,32 */
#include "peripheral.h"     /* import Serial (Debug) */
#include "configuration.h"
#include "prototype.h"

/*
 * NOTE:
 *
 * Handles JTAGICE3 payloads.
 *
 * Encapsulates higher level payloads in various formats using leading scope numbers.
 * ATMEL based JTAG, AVRISP, STK600-XPRG, etc.
 *
 * EDBG payload is an ATMEL vendor extension to CSMIS-DAP.
 * Splits JTAG3 payload into chunks to fit into USB-HID report packets.
 *
 * CSMIS-DAP is a common application foundation that leverages USB-HID directly.
 * Encapsulates ARM based SWO/SWD technology and JTAG conventions.
 * Only EDBG extensions and parts of the common API are actually used here.
 */

namespace JTAG {

  /* PARM3_HW_VER, PARM3_FW_MAJOR, PARM3_FW_MINOR, PARM3_FW_REL[2] */
  const uint8_t PROGMEM jtag_version[] = CONFIG_SYS_FWVER;
  const uint8_t PROGMEM jtag_physical[] = {0x90, 0x28, 0x00, 0x18, 0x38, 0x00, 0x00, 0x00};

  /*** Only a subset of the CMSIS-DAP commands are implemented. ***/
  /*
   * Command numbers 0x80 and above are vendor extensions, EDBG Payload uses 0x80 and x81.
   * Additionally, 0x82 is reserved for device event notification.
   */
  bool dap_command_check (void) {
    bool _result = false;
    uint8_t _cmd = EP_MEM.dap_data[0];
    uint8_t _sub = EP_MEM.dap_data[1];
    D2PRINTF("DAP=%02X SUB=%02X\r\n", _cmd, _sub);
    DFLUSH();
    USB::ep_dpi_pending();

    /*** EDBG Payload ***/
    /*
     * The payload is split into 60-byte chunks, a header is added,
     * and the data is sent and received with a fixed length of 64 bytes,
     * determined by the value specified in the HID Report.
     * A maximum of 15 chunks is possible,
     * resulting in a maximum payload length of 900 bytes.
     */
    if (_cmd == 0x80) {             /* DAP_EDBG_VENDOR_AVR_CMD */
      uint8_t _endf = _sub & 0x0F;
      uint8_t _frag = _sub >> 4;
      uint8_t _size = EP_MEM.dap_data[3];
      size_t  _ofst = (_frag - 1) * 60;
      if (_endf >= 10) {
        /* Only a maximum of 540 bytes : 9 fragment records is accepted. */
        EP_MEM.dap_data[1] = 0x00;  /* EDBG_RSP_FAIL */
      }
      else {
        /* Detect the first chunk. */
        if (_frag == 1) _packet_chunks = 0;
        ++_packet_chunks;
        memcpy(&packet.rawData[_ofst], &EP_MEM.dap_data[4], _size);
        EP_MEM.dap_data[1] = 0x01;  /* EDBG_RSP_OK */
        D3PRINTHEX(&EP_MEM.dap_data, _size + 4);
        if (_endf == _frag) {       /* end of defragment */
          _packet_length = _ofst + _size;
          D3PRINTF(" SQ=%03X:%03X<", packet.out.sequence, _packet_length);
          D3PRINTHEX(&packet, _packet_length);
          if (_packet_chunks == _endf) {
            /* True if an EDBG Payload is received. */
            _packet_endfrag = 0;
            _result = true;
          }
          else {
            /* A missing chunk is detected, so an error is returned. */
            EP_MEM.dap_data[1] = 0x00;  /* EDBG_RSP_FAIL */
          }
        }
      }
    }
    else if (_cmd == 0x81) {        /* DAP_EDBG_VENDOR_AVR_RSP */
      EP_MEM.dap_data[2] = 0;       /* Always zero */
      if (_packet_endfrag == 0) {
        EP_MEM.dap_data[1] = 0;
        EP_MEM.dap_data[3] = 0;
      }
      else {
        memcpy(&EP_MEM.dap_data[4], &packet.in.token + (_packet_fragment * 60), 60);
        EP_MEM.dap_data[1] = ((++_packet_fragment) << 4) | _packet_endfrag;
        EP_MEM.dap_data[3] = _packet_fragment == _packet_endfrag ? _packet_length : 60;
        _packet_length -= 60;
        D3PRINTF(" PI=");
        D3PRINTHEX(&EP_MEM.dap_data, EP_MEM.dap_data[3] + 4);
      }
    }

    /*** DAP Standard ***/
    else if (_cmd == 0x00) {        /* DAP_CMD_INFO */
      if (_sub == 0xFF              /* DAP_INFO_PACKET_SIZE      */
       || _sub == 0xFB              /* UART Receive Buffer Size  */
       || _sub == 0xFC) {           /* UART Transmit Buffer Size */
        EP_MEM.dap_data[1] = 0x02;  /* length=2 */
        EP_MEM.dap_data[2] = 0x40;  /* MaxPacketSize = 64 */
        EP_MEM.dap_data[3] = 0x00;
        D3PRINTF(" PI=");
        D3PRINTHEX(&EP_MEM.dap_data, 4);
      }
      else if (_sub == 0xF1) {      /* DAP_INFO_Capabilities */
        EP_MEM.dap_data[1] = 0x02;  /* length=2 */
        EP_MEM.dap_data[2] = 0x00;  /* 7:UART Communication Port */
        EP_MEM.dap_data[3] = 0x00;  /* 0:USB COM Port */
        D3PRINTF(" PI=");
        D3PRINTHEX(&EP_MEM.dap_data, 4);
      }
    }
    else if (_cmd == 0x02) {        /* DAP_CMD_CONNECT */
      /* EP_MEM.dap_data[1] == CONN_TYPE */
      /* Here, the response is returned without processing. */
      D3PRINTF(" PI=");
      D3PRINTHEX(&EP_MEM.dap_data, 2);
    }
    else if (_cmd == 0x01           /* DAP_CMD_HOSTSTATUS */
          && _sub == 0x00) {        /* DAP_LED_CONNECT */
      /* EP_MEM.dap_data[2] == LED_ON/OFF */
      /* Here, the response is returned without processing. */
      _led_next = 0b11111111;
      // TCA0_SINGLE_PER = F_CPU / 1024 / 20;
      D3PRINTF(" PI=");
      D3PRINTHEX(&EP_MEM.dap_data, 3);
    }
    else if (_cmd == 0x03) {        /* DAP_CMD_DISCONNECT */
      /* Here, the response is returned without processing. */
      D3PRINTF(" PI=");
      D3PRINTHEX(&EP_MEM.dap_data, 2);
      loop_until_bit_is_clear(WDT_STATUS, WDT_SYNCBUSY_bp);
      _PROTECTED_WRITE(WDT_CTRLA, WDT_PERIOD_128CLK_gc);
      GPCONF = GPCONF_FAIL_bm;
    }
    else {
      EP_MEM.dap_data[1] = 0x00;    /* other 0 length result */
    }
    USB::complete_dap_out();
    return _result; /* True if an EDBG Payload is received. */
  }

  uint8_t div8 (size_t _x, uint8_t _y) {
    uint8_t _z = 0;
    while (_x > 0 && _x >= _y) { _z++; _x -= _y; }
    return _z;
  }

  /*** Prepare for EDBG payload request from device to host ***/
  void complete_jtag_transactions (size_t _length) {
    _packet_length = _length + 6; /* TOKEN + SEQ[2] + EOT + PAD */
    _packet_fragment = 0;
    _packet_endfrag = div8(_length + 65, 60);   /* 1 to 15 */
    packet.in.token = 0x0E;                     /* TOKEN */
    packet.rawData[_packet_length] = 0;         /* EOT */
    D3PRINTF(" SQ=%03X:%03X:%03X>", packet.out.sequence, _length, _packet_length);
    D3PRINTHEX(&packet.in.token, _packet_length);
  }

  /*** Only a subset of JTAGICE3 commands are implemented. ***/
  size_t jtag_scope_general (void) {
    size_t  _rspsize = 0;
    uint8_t _cmd     = packet.out.cmd;
    uint8_t _section = packet.out.section;
    uint8_t _index   = packet.out.index;
    uint8_t _length  = packet.out.length;
    if (_cmd == 0x02) {             /* CMD3_GET_PARAMETER */
      // D1PRINTF(" GEN_GET_PARAM=%02X:%02X:%02X\r\n", _section, _index, _length);
      if (_section == 0) {          /* SET_GET_CTXT_CONFIG */
        /* _index == 0-5 */
        memcpy_P(&packet.in.data[0], &jtag_version[_index], _length);
        D1PRINTF(" VER=");
        D1PRINTHEX(&packet.in.data[0], _length);
      }
      else if (_section == 1) {     /* SET_GET_CTXT_PHYSICAL */
        if (_index == 0 || _index == 0x20) {  /* PARM3_VTARGET */
          packet.in.wValue = SYS::get_vdd();
          D1PRINTF(" VTG=%d\r\n", packet.in.wValue);
        }
      }
      packet.in.res = 0x184;        /* RSP3_DATA */
      _rspsize = _length + 1;
    }
    else if (_cmd == 0x10) {        /* CMD3_SIGN_ON */
      D1PRINTF(" GEN_SIGN_ON\r\n");
      _jtag_arch = 0;
      packet.in.res = 0x80;         /* RSP3_OK */
    }
    else if (_cmd == 0x11) {        /* CMD3_SIGN_OFF */
      D1PRINTF(" GEN_SIGN_OFF\r\n");
      packet.in.res = 0x80;         /* RSP3_OK */
    }
    return _rspsize;
  }

  /*** The EDBG scope provides access to the writer's hardware specifications. ***/
  /* There is no impact on operation if it is not called at all. */
  size_t jtag_scope_edbg (void) {
    size_t  _rspsize = 0;
    uint8_t _cmd     = packet.out.cmd;
    uint8_t _length  = packet.out.length;
    if (_cmd == 0x01) {             /* CMD3_SET_PARAMETER */
      packet.in.res = 0x80;         /* RSP3_OK */
    }
    else if (_cmd == 0x02) {        /* CMD3_GET_PARAMETER */
      packet.in.res = 0x184;        /* RSP3_DATA */
      _rspsize = _length + 1;
    }
    return _rspsize;
  }

  /* The AVR scope is further branched by the ARCH designator. */
  size_t jtag_scope_avr_core (void) {
    size_t  _rspsize = 0;
    uint8_t _cmd     = packet.out.cmd;
    uint8_t _section = packet.out.section;
    uint8_t _index   = packet.out.index;
    uint8_t _length  = packet.out.length;
    if (_cmd == 0x01) {             /* CMD3_SET_PARAMETER */
      uint16_t _data = packet.out.wValue & 0xFF;
      if (_section == 0) {          /* SET_GET_CTXT_CONFIG */
        if (_index == 0) {          /* PARM3_ARCH */
          D1PRINTF(" ARCH=%02X\r\n", _data);
          _jtag_arch = _data;       /* 5:UPDI 3:PDI */
        }
      }
      else if (_section == 1) {     /* SET_GET_CTXT_PHYSICAL */
        if (_index == 0) {          /* PARM3_CONNECTION */
          D1PRINTF(" CONNECTION=%02X\r\n", _data);
          _jtag_conn = _data;       /* 8:PARM3_CONN_UPDI */
        }
      }
      else if (_section == 2) {     /* SET_GET_CTXT_DEVICE */
        if (_index == 0) {          /* PARM3_DEVICEDESC */
          D1PRINTF(" DEVICEDESC=%X\r\n", _length);
          memcpy(&Device_Descriptor, &packet.out.setData[0], _length & 63);
  #if DEBUG >= 1
          if (_jtag_arch == 5) {
            D2PRINTF("(UPDI)  prog_base=%02X:%04X\r\n", Device_Descriptor.UPDI.prog_base_msb, Device_Descriptor.UPDI.prog_base);
            D2PRINTF("  flash_page_size=%02X:%02X\r\n", Device_Descriptor.UPDI.flash_page_size_msb, Device_Descriptor.UPDI.flash_page_size);
            D2PRINTF("      flash_bytes=%06lX\r\n", Device_Descriptor.UPDI.flash_bytes);
            D2PRINTF("     eeprom_bytes=%04X\r\n", Device_Descriptor.UPDI.eeprom_bytes);
            D2PRINTF("   user_sig_bytes=%04X\r\n", Device_Descriptor.UPDI.user_sig_bytes);
            D2PRINTF("      fuses_bytes=%04X\r\n", Device_Descriptor.UPDI.fuses_bytes);
            D2PRINTF("      eeprom_base=%04X\r\n", Device_Descriptor.UPDI.eeprom_base);
            D2PRINTF("    user_sig_base=%04X\r\n", Device_Descriptor.UPDI.user_sig_base);
            D2PRINTF("   signature_base=%04X\r\n", Device_Descriptor.UPDI.signature_base);
            D2PRINTF("       fuses_base=%04X\r\n", Device_Descriptor.UPDI.fuses_base);
            D2PRINTF("    lockbits_base=%04X\r\n", Device_Descriptor.UPDI.lockbits_base);
            D2PRINTF("     address_mode=%02X\r\n", Device_Descriptor.UPDI.address_mode);
            D2PRINTF("   hvupdi_variant=%02X\r\n", Device_Descriptor.UPDI.hvupdi_variant);
            /* Even with all this, the BOOTROW information is still undefined! */
            /* Re-analysis of newer ICE FW is needed! */
          }
          /* STUB: And other descriptors. */
  #endif
        }
      }
      packet.in.res = 0x80;         /* RSP3_OK */
    }
    else if (_cmd == 0x02) {        /* CMD3_GET_PARAMETER */
      if (_section == 0) {          /* SET_GET_CTXT_CONFIG */
        if (_index == 0) {          /* PARM3_ARCH */
          packet.in.data[0] = _jtag_arch;
        }
      }
      else if (_section == 1) {     /* SET_GET_CTXT_PHYSICAL */
        if (_index == 0) {          /* PARM3_CONNECTION */
          /* This is a stub that is called but not used. */
          packet.in.data[0] = _jtag_conn;
        }
        else if (_index == 0x31) {  /* PARM3_CLK_XMEGA_PDI */
          D1PRINTF(" BOOT=%d\r\n", _bootsize);
          packet.in.wValue = _bootsize;
        }
      }
      packet.in.res = 0x184;        /* RSP3_DATA */
      _rspsize = _length + 1;
    }
    /* AVR-DU series support */
    else if (_jtag_arch == 0x05) _rspsize = NVM::V4::jtag_scope_updi();
    else packet.in.res = 0xA0;      /* RSP3_FAILED */
    return _rspsize;
  } /* jtag_scope_avr_core */

  /* Processing branches depending on the scope specifier. */
  /* Currently, four types of scope are known: */
  void jtag_scope_branch (void) {
    size_t _rspsize = 0;
    uint8_t _scope  = packet.out.scope;
    D2PRINTF("SQ=%d:%d>SCOPE=%02X,C=%02X,S=%02X,L=%02X\r\n",
      packet.out.sequence,
      _packet_length,
      _scope,
      packet.out.cmd,
      packet.out.section,
      packet.out.index);
    if      (_scope == 0x01) _rspsize = jtag_scope_general();       /* SCOPE_GENERAL */
    else if (_scope == 0x12) _rspsize = jtag_scope_avr_core();      /* SCOPE_AVR */
    else if (_scope == 0x20) _rspsize = jtag_scope_edbg();          /* SCOPE_EDBG */
    complete_jtag_transactions(_rspsize);
  } /* jtag_scope_branch */

};

// end of code
