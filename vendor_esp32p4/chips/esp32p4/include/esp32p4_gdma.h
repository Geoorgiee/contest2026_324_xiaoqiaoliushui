/****************************************************************************
 * vendor_esp32p4/chips/esp32p4/include/esp32p4_gdma.h
 *
 * Licensed to the Apache Software Foundation (ASF) under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.  The
 * ASF licenses this file to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance with the
 * License.  You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.  See the
 * License for the specific language governing permissions and limitations
 * under the License.
 *
 ****************************************************************************/

#ifndef __VENDOR_ESP32P4_CHIPS_ESP32P4_INCLUDE_ESP32P4_GDMA_H
#define __VENDOR_ESP32P4_CHIPS_ESP32P4_INCLUDE_ESP32P4_GDMA_H

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/* GDMA controller types */

#define GDMA_TYPE_AHB             0     /* AHB-GDMA controller (group 0) */
#define GDMA_TYPE_AXI             1     /* AXI-GDMA controller (group 1) */

/* Number of channels per controller */

#define GDMA_AHB_NUM_CHANNELS     3
#define GDMA_AXI_NUM_CHANNELS     3
#define GDMA_MAX_CHANNELS         3

/* DMA descriptor alignment */

#define GDMA_AHB_DESC_ALIGN       4
#define GDMA_AXI_DESC_ALIGN       8

/* Maximum buffer size per descriptor */

#define GDMA_MAX_BUF_SIZE         4095

/* DMA event flags */

#define GDMA_EVENT_DONE           (1 << 0)  /* Transfer complete */
#define GDMA_EVENT_EOF            (1 << 1)  /* End of frame */
#define GDMA_EVENT_ERR_EOF        (1 << 2)  /* Error in received data */
#define GDMA_EVENT_DESC_ERR       (1 << 3)  /* Descriptor error */
#define GDMA_EVENT_DESC_EMPTY     (1 << 4)  /* Descriptor chain empty */
#define GDMA_EVENT_FIFO_OVF       (1 << 5)  /* FIFO overflow */
#define GDMA_EVENT_FIFO_UDF       (1 << 6)  /* FIFO underflow */

/* Peripheral IDs for GDMA trigger connection
 * These match the SOC_GDMA_TRIG_PERIPH_* definitions from ESP-IDF
 */

#define GDMA_PERIPH_M2M           (-1)  /* Memory-to-memory */
#define GDMA_PERIPH_I3C0          0
#define GDMA_PERIPH_UHCI0         2
#define GDMA_PERIPH_I2S0          3
#define GDMA_PERIPH_I2S1          4
#define GDMA_PERIPH_I2S2          5
#define GDMA_PERIPH_ADC0          8
#define GDMA_PERIPH_RMT0          10
#define GDMA_PERIPH_SPI2          1
#define GDMA_PERIPH_SPI3          2
#define GDMA_PERIPH_PARLIO0       3
#define GDMA_PERIPH_AES0          4
#define GDMA_PERIPH_SHA0          5

/****************************************************************************
 * Public Types
 ****************************************************************************/

/* DMA transfer direction */

enum gdma_direction_e
{
  GDMA_DIR_P2M = 0,  /* Peripheral to Memory (RX) */
  GDMA_DIR_M2P = 1,  /* Memory to Peripheral (TX) */
};

/* DMA channel status */

enum gdma_channel_state_e
{
  GDMA_STATE_IDLE = 0,       /* Channel is idle */
  GDMA_STATE_CONFIGURED,     /* Channel is configured but not started */
  GDMA_STATE_RUNNING,        /* Transfer in progress */
  GDMA_STATE_DONE,           /* Transfer completed */
  GDMA_STATE_ERROR,          /* Transfer error */
};

/* DMA descriptor (hardware-linked list item)
 *
 * Each descriptor describes one buffer segment in a linked list.
 * The DMA engine traverses the linked list to transfer data.
 *
 *   dw0:  Control/status word
 *   buf:  Pointer to data buffer
 *   next: Pointer to next descriptor (NULL for last)
 */

struct gdma_desc_s
{
  uint32_t dw0;       /* Word 0: size, length, flags, owner */
  uint8_t *buf;       /* Buffer pointer */
  struct gdma_desc_s *next;  /* Next descriptor pointer */
};

/* Bit fields for dw0 */

#define GDMA_DESC_SIZE_MASK       0x00000fff  /* bits [11:0]  - buffer size */
#define GDMA_DESC_SIZE_SHIFT      0
#define GDMA_DESC_LEN_MASK        0x00fff000  /* bits [23:12] - valid data length */
#define GDMA_DESC_LEN_SHIFT       12
#define GDMA_DESC_ERR_EOF_BIT     (1 << 28)   /* bit 28 - error in received data */
#define GDMA_DESC_SUC_EOF_BIT     (1 << 30)   /* bit 30 - successful EOF */
#define GDMA_DESC_OWNER_BIT       (1 << 31)   /* bit 31 - owner (1=DMA, 0=CPU) */

#define GDMA_DESC_SET_SIZE(d, s) \
  do { (d)->dw0 = ((d)->dw0 & ~GDMA_DESC_SIZE_MASK) | \
       (((s) << GDMA_DESC_SIZE_SHIFT) & GDMA_DESC_SIZE_MASK); } while (0)
#define GDMA_DESC_SET_LEN(d, l) \
  do { (d)->dw0 = ((d)->dw0 & ~GDMA_DESC_LEN_MASK) | \
       (((l) << GDMA_DESC_LEN_SHIFT) & GDMA_DESC_LEN_MASK); } while (0)
#define GDMA_DESC_SET_EOF(d)     do { (d)->dw0 |= GDMA_DESC_SUC_EOF_BIT; } while (0)
#define GDMA_DESC_SET_OWNER_DMA(d) do { (d)->dw0 |= GDMA_DESC_OWNER_BIT; } while (0)
#define GDMA_DESC_SET_OWNER_CPU(d) do { (d)->dw0 &= ~GDMA_DESC_OWNER_BIT; } while (0)

#define GDMA_DESC_GET_SIZE(d)    (((d)->dw0 & GDMA_DESC_SIZE_MASK) >> GDMA_DESC_SIZE_SHIFT)
#define GDMA_DESC_GET_LEN(d)     (((d)->dw0 & GDMA_DESC_LEN_MASK) >> GDMA_DESC_LEN_SHIFT)
#define GDMA_DESC_IS_EOF(d)      (((d)->dw0 & GDMA_DESC_SUC_EOF_BIT) != 0)
#define GDMA_DESC_IS_OWNER_DMA(d) (((d)->dw0 & GDMA_DESC_OWNER_BIT) != 0)

/* GDMA event callback type
 *
 * Parameters:
 *   channel  - Channel number that generated the event
 *   events   - Bitmask of GDMA_EVENT_* flags
 *   arg      - User-provided argument
 *
 * Returns: true if a context switch is needed (from ISR)
 */

typedef bool (*gdma_callback_t)(int channel, uint32_t events, void *arg);

/* GDMA channel configuration structure */

struct gdma_config_s
{
  uint8_t type;         /* GDMA_TYPE_AHB or GDMA_TYPE_AXI */
  uint8_t direction;    /* GDMA_DIR_P2M or GDMA_DIR_M2P */
  int     periph_id;    /* Peripheral ID (GDMA_PERIPH_*) or GDMA_PERIPH_M2M */
  bool    psram_access; /* Enable access to PSRAM region */
  uint8_t burst_size;   /* Burst size: 0(disabled), 4, 8, 16, 32, 64 (AHB) or 8-128 (AXI) */
  uint8_t priority;     /* Channel priority [0, 5], higher = more priority */
};

/* GDMA transfer request structure */

struct gdma_xfer_s
{
  struct gdma_desc_s *desc;   /* Descriptor chain head */
  gdma_callback_t     cb;     /* Completion callback (from ISR context) */
  void               *arg;    /* Callback argument */
};

/* GDMA channel handle (opaque) */

struct gdma_channel_s;
typedef struct gdma_channel_s *gdma_chan_t;

/****************************************************************************
 * Public Function Prototypes
 ****************************************************************************/

#undef EXTERN
#if defined(__cplusplus)
#define EXTERN extern "C"
extern "C"
{
#else
#define EXTERN extern
#endif

/****************************************************************************
 * Name: esp32p4_gdma_initialize
 *
 * Description:
 *   Initialize the GDMA subsystem. Must be called once before any
 *   other GDMA API.
 *
 * Returned Value:
 *   OK on success; negative errno on failure.
 *
 ****************************************************************************/

int esp32p4_gdma_initialize(void);

/****************************************************************************
 * Name: esp32p4_gdma_channel_alloc
 *
 * Description:
 *   Allocate a GDMA channel with the given configuration.
 *
 * Input Parameters:
 *   config - Channel configuration
 *
 * Returned Value:
 *   Channel handle on success; NULL on failure.
 *
 ****************************************************************************/

gdma_chan_t esp32p4_gdma_channel_alloc(const struct gdma_config_s *config);

/****************************************************************************
 * Name: esp32p4_gdma_channel_free
 *
 * Description:
 *   Free a previously allocated GDMA channel.
 *
 * Input Parameters:
 *   chan - Channel handle from esp32p4_gdma_channel_alloc
 *
 * Returned Value:
 *   OK on success; negative errno on failure.
 *
 ****************************************************************************/

int esp32p4_gdma_channel_free(gdma_chan_t chan);

/****************************************************************************
 * Name: esp32p4_gdma_config_transfer
 *
 * Description:
 *   Configure transfer parameters for a GDMA channel (burst size,
 *   PSRAM access, priority).
 *
 * Input Parameters:
 *   chan       - Channel handle
 *   burst_size - Burst size in bytes (0=disabled, power of 2)
 *   psram      - true to enable PSRAM region access
 *   priority   - Channel priority [0, 5]
 *
 * Returned Value:
 *   OK on success; negative errno on failure.
 *
 ****************************************************************************/

int esp32p4_gdma_config_transfer(gdma_chan_t chan, uint8_t burst_size,
                                  bool psram, uint8_t priority);

/****************************************************************************
 * Name: esp32p4_gdma_start
 *
 * Description:
 *   Start a DMA transfer on the given channel.
 *
 * Input Parameters:
 *   chan - Channel handle
 *   xfer - Transfer request (descriptor chain + callback)
 *
 * Returned Value:
 *   OK on success; negative errno on failure.
 *
 ****************************************************************************/

int esp32p4_gdma_start(gdma_chan_t chan, const struct gdma_xfer_s *xfer);

/****************************************************************************
 * Name: esp32p4_gdma_stop
 *
 * Description:
 *   Stop the DMA transfer on the given channel.
 *
 * Input Parameters:
 *   chan - Channel handle
 *
 * Returned Value:
 *   OK on success; negative errno on failure.
 *
 ****************************************************************************/

int esp32p4_gdma_stop(gdma_chan_t chan);

/****************************************************************************
 * Name: esp32p4_gdma_reset
 *
 * Description:
 *   Reset the DMA channel FIFO and state machine.
 *
 * Input Parameters:
 *   chan - Channel handle
 *
 * Returned Value:
 *   OK on success; negative errno on failure.
 *
 ****************************************************************************/

int esp32p4_gdma_reset(gdma_chan_t chan);

/****************************************************************************
 * Name: esp32p4_gdma_get_state
 *
 * Description:
 *   Get the current state of a GDMA channel.
 *
 * Input Parameters:
 *   chan - Channel handle
 *
 * Returned Value:
 *   Current channel state (enum gdma_channel_state_e).
 *
 ****************************************************************************/

int esp32p4_gdma_get_state(gdma_chan_t chan);

/****************************************************************************
 * Name: esp32p4_gdma_desc_init
 *
 * Description:
 *   Initialize a DMA descriptor with buffer address, size, and length.
 *
 * Input Parameters:
 *   desc   - Pointer to descriptor
 *   buf    - Data buffer address
 *   size   - Buffer size (max 4095)
 *   length - Valid data length (<= size)
 *   eof    - true to mark this descriptor as EOF
 *
 ****************************************************************************/

void esp32p4_gdma_desc_init(struct gdma_desc_s *desc, void *buf,
                             uint16_t size, uint16_t length, bool eof);

/****************************************************************************
 * Name: esp32p4_gdma_desc_chain
 *
 * Description:
 *   Chain two descriptors together (set next pointer).
 *
 * Input Parameters:
 *   desc - Current descriptor
 *   next - Next descriptor (NULL to terminate chain)
 *
 ****************************************************************************/

void esp32p4_gdma_desc_chain(struct gdma_desc_s *desc,
                              struct gdma_desc_s *next);

/****************************************************************************
 * Name: esp32p4_gdma_desc_alloc
 *
 * Description:
 *   Allocate a DMA descriptor with proper alignment.
 *
 * Input Parameters:
 *   type - GDMA_TYPE_AHB or GDMA_TYPE_AXI
 *
 * Returned Value:
 *   Pointer to aligned descriptor; NULL on failure.
 *
 ****************************************************************************/

struct gdma_desc_s *esp32p4_gdma_desc_alloc(uint8_t type);

/****************************************************************************
 * Name: esp32p4_gdma_desc_free
 *
 * Description:
 *   Free a previously allocated DMA descriptor.
 *
 * Input Parameters:
 *   desc - Descriptor to free
 *
 ****************************************************************************/

void esp32p4_gdma_desc_free(struct gdma_desc_s *desc);

/****************************************************************************
 * Name: esp32p4_gdma_mem_toPeriph
 *
 * Description:
 *   Convenience function: start a memory-to-peripheral DMA transfer.
 *   Sets up a single descriptor from the buffer and starts the channel.
 *
 * Input Parameters:
 *   chan   - Channel handle (must be configured for M2P)
 *   buf    - Source buffer
 *   len    - Transfer length in bytes
 *   cb     - Completion callback
 *   arg    - Callback argument
 *
 * Returned Value:
 *   OK on success; negative errno on failure.
 *
 ****************************************************************************/

int esp32p4_gdma_mem_toPeriph(gdma_chan_t chan, const void *buf,
                               size_t len, gdma_callback_t cb, void *arg);

/****************************************************************************
 * Name: esp32p4_gdma_periph_toMem
 *
 * Description:
 *   Convenience function: start a peripheral-to-memory DMA transfer.
 *   Sets up a single descriptor for the buffer and starts the channel.
 *
 * Input Parameters:
 *   chan   - Channel handle (must be configured for P2M)
 *   buf    - Destination buffer
 *   len    - Transfer length in bytes
 *   cb     - Completion callback
 *   arg    - Callback argument
 *
 * Returned Value:
 *   OK on success; negative errno on failure.
 *
 ****************************************************************************/

int esp32p4_gdma_periph_toMem(gdma_chan_t chan, void *buf,
                               size_t len, gdma_callback_t cb, void *arg);

/****************************************************************************
 * Name: esp32p4_gdma_test
 *
 * Description:
 *   Run a simple DMA loopback test. Allocates AHB and AXI channels,
 *   performs a memory-to-memory transfer, and verifies the data.
 *
 * Returned Value:
 *   OK on success; negative errno on failure.
 *
 ****************************************************************************/

#ifdef CONFIG_ESP32P4_GDMA_TEST
int esp32p4_gdma_test(void);
#endif

#undef EXTERN
#if defined(__cplusplus)
}
#endif

#endif /* __VENDOR_ESP32P4_CHIPS_ESP32P4_INCLUDE_ESP32P4_GDMA_H */
