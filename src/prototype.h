/**
 * @file prototype.h
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

#pragma once

#ifndef F_CPU
  #define F_CPU 20000000L
#endif
#ifndef CONSOLE_BAUD
  #define CONSOLE_BAUD 500000L
#endif

#include <avr/io.h>
#include <stddef.h>
#include <setjmp.h>
#include <api/memspace.h>
#include "configuration.h"

// #undef DEBUG
// #define DEBUG 3

#undef Serial
#define DFLUSH()
#define D0PRINTF(FMT, ...)
#define D1PRINTF(FMT, ...)
#define D2PRINTF(FMT, ...)
#define D3PRINTF(FMT, ...)
#define D0PRINTHEX(P,L)
#define D1PRINTHEX(P,L)
#define D2PRINTHEX(P,L)
#define D3PRINTHEX(P,L)
#if defined(DEBUG)
  #include "peripheral.h" /* from Micro_API : import Serial (Debug) */
  #define Serial Serial1C /* PIN_PD6:TxD, PIN_PD7:RxD */
  #undef  DFLUSH
  #define DFLUSH() Serial.flush()
  #undef  D0PRINTF
  #define D0PRINTF(FMT, ...) Serial.printf(F(FMT), ##__VA_ARGS__)
  #undef  D0PRINTHEX
  #define D0PRINTHEX(P,L) Serial.printHex((P),(L),':').ln()
  #if (DEBUG >= 1)
    #undef D1PRINTF
    #define D1PRINTF(FMT, ...) Serial.printf(F(FMT), ##__VA_ARGS__)
    #undef  D1PRINTHEX
    #define D1PRINTHEX(P,L) Serial.printHex((P),(L),':').ln()
  #endif
  #if (DEBUG >= 2)
    #undef  D2PRINTF
    #define D2PRINTF(FMT, ...) Serial.printf(F(FMT), ##__VA_ARGS__)
    #undef  D2PRINTHEX
    #define D2PRINTHEX(P,L) Serial.printHex((P),(L),':').ln()
  #endif
  #if (DEBUG >= 3)
    #undef  D3PRINTF
    #define D3PRINTF(FMT, ...) Serial.printf(F(FMT), ##__VA_ARGS__)
    #undef  D3PRINTHEX
    #define D3PRINTHEX(P,L) Serial.printHex((P),(L),':').ln()
  #endif
#endif

#define PACKED __attribute__((packed))
#define WEAK   __attribute__((weak))
#define RODATA __attribute__((__progmem__))
#define NOINIT __attribute__((section(".noinit")))

#define USB_EP_SIZE_gc(x)  ((x <= 8 ) ? USB_BUFSIZE_DEFAULT_BUF8_gc :\
                            (x <= 16) ? USB_BUFSIZE_DEFAULT_BUF16_gc:\
                            (x <= 32) ? USB_BUFSIZE_DEFAULT_BUF32_gc:\
                                        USB_BUFSIZE_DEFAULT_BUF64_gc)
#define USB_EP_ID_SWAP(x) __builtin_avr_swap(x)
#define USB_EP(EPFIFO) (*(USB_EP_t *)(((uint16_t)&EP_TABLE.EP) + (EPFIFO)))
#define USB_EP_STATUS_CLR(EPFIFO) _SFR_MEM8(&USB0_STATUS0_OUTCLR + ((EPFIFO) >> 2))
#define USB_EP_STATUS_SET(EPFIFO) _SFR_MEM8(&USB0_STATUS0_OUTSET + ((EPFIFO) >> 2))

#define USB_ENDPOINTS_MAX 3

/* In the internal representation of an endpoint number, */
/* the high and low nibbles are reversed from the representation on the USB device. */
#define USB_EP_REQ  (0x00)
#define USB_EP_RES  (0x08)
#define USB_EP_DPI  (0x18)  /* #0 DAP IN  */
#define USB_EP_DPO  (0x20)  /* #0 DAP OUT */

#define EP_REQ  USB_EP(USB_EP_REQ)
#define EP_RES  USB_EP(USB_EP_RES)
#define EP_DPI  USB_EP(USB_EP_DPI)
#define EP_DPO  USB_EP(USB_EP_DPO)

#define GPCONF GPR_GPR2
  #define GPCONF_USB_bp   0         /* USB interface is active */
  #define GPCONF_USB_bm   (1 << 0)
  #define GPCONF_FAIL_bp  7         /* Enable WDT Timeout */
  #define GPCONF_FAIL_bm  (1 << 7)

/*
 * Global struct
 */

typedef struct {
  union {
    uint8_t rawData[540];
    struct {
      uint8_t  token;             /* offset 0 */
      uint8_t  reserve1;
      uint16_t sequence;
      uint8_t  scope;
      uint8_t  cmd;
      union {
        uint8_t data[534];
        struct {  /* CMD=21,23:CMD3_READ,WRITE_MEMORY */
          uint8_t  reserve2;
          uint8_t  bMType;
          uint32_t dwAddr;
          uint32_t dwLength;
          uint8_t  reserve3;
          uint8_t  memData[513];  /* WRITE_MEMORY */
        };
        struct {  /* CMD=1,2:CMD3_GET,SET_PARAMETER */
          uint8_t  reserve4;
          uint8_t  section;
          uint8_t  index;
          uint8_t  length;
          union {
            uint16_t wValue;
            uint8_t  setData[255]; /* SET_PARAMETER */
          };
        };
        struct {  /* CMD3_ERASE_MEMORY */
          uint8_t  reserve5;
          uint8_t  bEType;
          uint32_t dwPageAddr;
        };
      };
    } out;
    struct {
      uint8_t  reserve6;          /* offset -1 */
      uint8_t  token;             /* offset 0  */
      uint16_t sequence;
      uint8_t  scope;
      uint16_t res;
      union {
        uint8_t  data[513];       /* READ_MEMORY */
        uint8_t  bStatus;
        uint16_t wValue;
        uint32_t dwValue;
      };
    } in;
  };
} PACKED JTAG_Packet_t;

typedef struct {
  union {
    uint16_t  wRequestType;
    struct {
      uint8_t bmRequestType;
      uint8_t bRequest;
    };
  };
  uint16_t wValue;
  uint16_t wIndex;
  uint16_t wLength;
} PACKED Setup_Packet_t;

typedef struct {
  Setup_Packet_t req_data;
  union {
    uint8_t res_data[256 + 16];
    struct {
      struct {
        Setup_Packet_t cci_header;
        uint16_t cci_wValue;
      };
      uint8_t dap_data[64];   /* DAP IN/OUT */
    };
  };
} PACKED EP_DATA_t;

typedef struct {
  USB_EP_PAIR_t EP[USB_ENDPOINTS_MAX];        /* USB Device Controller EP */
} PACKED EP_TABLE_t;

/* UPDI device descriptor */
typedef struct {
  uint16_t prog_base;
  uint8_t  flash_page_size;
  uint8_t  eeprom_page_size;
  uint16_t nvm_base_addr;
  uint16_t ocd_base_addr;
  // Configuration below, except for "Extended memory support", is only used by kits with
  // embedded debuggers (XPlained, Curiosity, ...).
  uint16_t default_min_div1_voltage;  // Default minimum voltage for 32M => 4.5V -> 4500
  uint16_t default_min_div2_voltage;  // Default minimum voltage for 16M => 2.7V -> 2700
  uint16_t default_min_div4_voltage;  // Default minimum voltage for 8M  => 2.2V -> 2200
  uint16_t default_min_div8_voltage;  // Default minimum voltage for 4M  => 1.5V -> 1500
  uint16_t pdi_pad_fmax;              // 750
  uint32_t flash_bytes;               // Flash size in bytes
  uint16_t eeprom_bytes;              // EEPROM size in bytes
  uint16_t user_sig_bytes;            // UserSignture size in bytes
  uint8_t  fuses_bytes;               // Fuses size in bytes
  uint8_t  syscfg_offset;             // Offset of SYSCFG0 within FUSE space
  uint8_t  syscfg_write_mask_and;     // AND mask to apply to SYSCFG0 when writing
  uint8_t  syscfg_write_mask_or;      // OR mask to apply to SYSCFG0 when writing
  uint8_t  syscfg_erase_mask_and;     // AND mask to apply to SYSCFG0 after erase
  uint8_t  syscfg_erase_mask_or;      // OR mask to apply to SYSCFG0 after erase
  uint16_t eeprom_base;               // Base address for EEPROM memory
  uint16_t user_sig_base;             // Base address for UserSignature memory
  uint16_t signature_base;            // Base address for Signature memory
  uint16_t fuses_base;                // Base address for Fuses memory
  uint16_t lockbits_base;             // Base address for Lockbits memory
  uint16_t device_id;                 // Two last bytes of the device ID
  // Extended memory support. Needed for flash >= 64kb
  uint8_t  prog_base_msb;             // Extends prog_base, used in 24-bit mode
  uint8_t  flash_page_size_msb;       // Extends flash_page_size, used in 24-bit mode
  uint8_t  address_mode;              // 0x00 = 16-bit mode, 0x01 = 24-bit mode
  uint8_t  hvupdi_variant;            // Indicates the target UPDI HV implementation
} PACKED UPDI_Device_Desc_t;

typedef struct {
  union {
    UPDI_Device_Desc_t UPDI;
  };
} PACKED Device_Desc_t;

/*
 * Global workspace
 */

extern "C" {
  namespace /* NAMELESS */ {

    /* SYSTEM */
    extern uint8_t _led_bits;
    extern uint8_t _led_next;
    extern uint8_t _led_mask;
    extern uint16_t _bootsize;

    /* USB */
    extern EP_TABLE_t EP_TABLE;
    extern EP_DATA_t EP_MEM;
    extern Device_Desc_t Device_Descriptor;
    extern uint8_t _set_config;

    /* JTAG packet payload */
    extern JTAG_Packet_t packet;
    extern size_t  _packet_length;
    extern uint8_t _packet_fragment;
    extern uint8_t _packet_chunks;
    extern uint8_t _packet_endfrag;

    /* JTAG parameter */
    extern uint32_t _before_page; /* before flash page section */
    extern uint8_t _jtag_arch;    /* 5:ARCH */
    extern uint8_t _jtag_conn;

  } /* NAMELESS */;

  extern void nvm_cmd (uint8_t _nvm_cmd);
};

namespace JTAG {
  bool dap_command_check (void);
  void jtag_scope_branch (void);
};

namespace NVM::V4 {
  size_t jtag_scope_updi (void);
};

namespace SYS {
  void reboot (void);
  uint16_t get_vdd (void);
  void delay_55us (void);
  void delay_100us (void);
  void delay_800us (void);
  void delay_2500us (void);
  void delay_125ms (void);
};

namespace USB {
  bool is_ep_setup (void);
  bool is_not_dap (void);
  void ep_dpi_pending (void);
  void complete_dap_out (void);
  void setup_device (bool _force = false);
  void handling_bus_events (void);
  void handling_control_transactions (void);
};

// end of header
