/**
 * @file nvmv4.cpp
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
#include "api/capsule.h"    /* _CAPS macro */
#include "peripheral.h"     /* import Serial (Debug) */
#include "configuration.h"
#include "prototype.h"

/*
 * NOTE:
 *
 * UPDI NVM version 4 is used in AVR-DU series.
 * Features include:
 *
 * - Data area is at the beginning of the 24-bit address space. (MSB=0)
 *   Flash area is at the end of the 24-bit address space. (MSB=1)
 *   All other memory types are in data space.
 *
 * - Signature is at address 0x1080.
 *
 * - There is no page buffer memory. Data space flash (i.e. USERROW)
 *   is heavily affected by this, so RSD fast writes cannot be used.
 *
 * - EEPROM can be written in units of up to 2 bytes.
 *   The normal setting for AVRDUDE is to read and write in units of 1 byte,
 *   which is very slow. Setting page_size=2 can improve this speed.
 *
 * - FUSE should be written in the same way as EEPROM.
 *
 * - Flash can be written in units of 512 bytes.
 *
 * - Erasing and rewriting a flash memory page are separate commands.
 *
 * - A page erase is required because USERROW is written to in the same way as flash.
 *
 * - BOOTROW can be treated the same as USERROW. It is a single page,
 *   so it must be erased before it can be rewritten.
 */

namespace NVM::V4 {

  /* The bootloader implementation cannot read the SIB area */
  /* of ​​the UPDI, so it always returns a fixed value.       */
  const uint8_t PROGMEM _sib[] = "AVR     P:4D:1-3M2 (EDBG.Boot.)"; /* 31 + 1 bytes */

  // MARK: API

  /* RAMPZ is not used because the flash memory of the AVR-DU series is a   */
  /* maximum of 64KiB, so pointers in the code area are limited to 16 bits. */

  size_t read_memory (void) {
    uint8_t   m_type = packet.out.bMType;
    uint16_t _dwAddr = packet.out.dwAddr;     /* The high-order word is ignored. */
    size_t  _wLength = packet.out.dwLength;
    if (m_type == 0xD3) {
      /* MTYPE_SIB */
      memcpy_P(&packet.in.data[0], &_sib, _wLength);
    }
    else if (m_type == 0xB0 || m_type == 0xC0) {
      /* MTYPE_FLASH_PAGE (PROGMEM) */
      memcpy_P(&packet.in.data[0], (void*)_dwAddr, _wLength);
    }
    else {
      memcpy(&packet.in.data[0], (void*)_dwAddr, _wLength);
    }
    return _wLength + 1;
  }

  void set_flmap (uint16_t &_dwAddr) {
    if (_dwAddr & 0x8000) {
      GPR_GPR0 = NVMCTRL_FLMAP_SECTION1_gc;
    }
    else {
      GPR_GPR0 = NVMCTRL_FLMAP_SECTION0_gc;
      _dwAddr |= 0x8000;
    }
    _PROTECTED_WRITE(NVMCTRL_CTRLB, GPR_GPR0);
  }

  size_t erase_memory (void) {
  #ifdef __No_implementation_required__
    /* Chip erasure is not possible. */
    /* Page erasure is not necessary outside of terminal mode. */
    uint8_t e_type = packet.out.bEType;
    uint16_t _dwAddr = packet.out.dwPageAddr;   /* The high-order word is ignored. */
    if (e_type == 0x04) {
      /* XMEGA_ERASE_APP_PAGE */
      set_flmap(_dwAddr);
      e_type = 0x07;
    }
    if (e_type == 0x07) {
      /* XMEGA_ERASE_USERSIG */
      nvm_cmd(NVMCTRL_CMD_FLPER_gc);
      *((uint8_t*)_dwAddr) = 0;
      nvm_cmd(NVMCTRL_CMD_FLWR_gc);
    }
  #endif
    return 1;
  }

  size_t write_memory (void) {
    uint8_t   m_type = packet.out.bMType;
    uint16_t _dwAddr = packet.out.dwAddr;     /* The high-order word is ignored. */
    size_t  _wLength = packet.out.dwLength;
    DFLUSH();
    if (m_type == 0xB0) {
      /* MTYPE_FLASH_PAGE (PROGMEM) */
      if (_dwAddr < _bootsize) return 1;
      set_flmap(_dwAddr);
      m_type = 0xC0;
    }

    if (m_type == 0x22 || m_type == 0xC4) {
      /* MTYPE_EEPROM */
      /* MTYPE_EEPROM_XMEGA */
      nvm_cmd(NVMCTRL_CMD_EEERWR_gc);
    }
    else if (m_type == 0xC0 || m_type == 0xC5) {
      /* MTYPE_FLASH (alias) */
      /* MTYPE_USERSIG (USERROW, BOOTROW) */
      nvm_cmd(NVMCTRL_CMD_FLPER_gc);
      *((uint8_t*)_dwAddr) = 0;
      nvm_cmd(NVMCTRL_CMD_FLWR_gc);
    }

    memcpy((void*)_dwAddr, &packet.out.memData[0], _wLength);
    nvm_cmd(NVMCTRL_CMD_NONE_gc);

    return 1;
  }

  // MARK: JTAG SCOPE

  /* ARCH=UPDI scope Provides functionality. */
  size_t jtag_scope_updi (void) {
    size_t _rspsize = 0;
    uint8_t _cmd = packet.out.cmd;
    if (_cmd == 0x10) {             /* CMD3_SIGN_ON */
      D1PRINTF(" UPDI_SIGN_ON=EXT:%02X\r\n", packet.out.bMType);
      memcpy_P(&packet.in.data[0], &_sib[0], 4);
      packet.in.res = 0x84;         /* RSP3_DATA */
      return 5;
    }
    else if (_cmd == 0x11) {        /* CMD3_SIGN_OFF */
      D1PRINTF(" UPDI_SIGN_OFF\r\n");
      /* If UPDI control has failed, RSP3_OK is always returned. */
      _rspsize = 1;
    }
    else if (_cmd == 0x15) {        /* CMD3_ENTER_PROGMODE */
      D1PRINTF(" UPDI_ENTER_PROG\r\n");
      /* On failure, RSP3_OK is returned if a UPDI connection is available. */
      _rspsize = 1;
    }
    else if (_cmd == 0x16) {        /* CMD3_LEAVE_PROGMODE */
      D1PRINTF(" UPDI_LEAVE_PROG\r\n");
      /* There is nothing to do. */
      /* The actual termination process is delayed until CMD3_SIGN_OFF. */
      _rspsize = 1;
    }
    else if (_cmd == 0x20) {        /* CMD3_ERASE_MEMORY */
      D1PRINTF(" UPDI_ERASE=%02X:%06lX\r\n",
        packet.out.bEType, packet.out.dwPageAddr);
      _rspsize = erase_memory();
    }
    else if (_cmd == 0x21) {        /* CMD3_READ_MEMORY */
      D1PRINTF(" UPDI_READ=%02X:%06lX:%04X\r\n", packet.out.bMType,
        packet.out.dwAddr, (size_t)packet.out.dwLength);
      _rspsize = read_memory();
      packet.in.res = 0x184;        /* RSP3_DATA */
      return _rspsize;
    }
    else if (_cmd == 0x23) {        /* CMD3_WRITE_MEMORY */
      D1PRINTF(" UPDI_WRITE=%02X:%06lX:%04X\r\n", packet.out.bMType,
        packet.out.dwAddr, (size_t)packet.out.dwLength);
      _rspsize = write_memory();
    }
    packet.in.res = _rspsize ? 0x80 : 0xA0;     /* RSP3_OK : RSP3_FAILED */
    return _rspsize;
  }

};

// end of code
