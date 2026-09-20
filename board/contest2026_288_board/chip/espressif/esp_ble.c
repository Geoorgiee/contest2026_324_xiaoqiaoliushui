/****************************************************************************
 * vendor/openvela/boards/contest2026_288_board/chip/espressif/esp_ble.c
 *
 * ESP32-P4 Bluetooth Low Energy chip-level stub.
 *
 * Copyright (C) 2026 contest2026_288_board BSP authors.
 *
 * Licensed under the Apache License, Version 2.0 (the "License"); you may
 * not use this file except in compliance with the License.
 *
 * Background
 * ==========
 * ESP32-P4 has no on-chip 2.4 GHz radio.  The production Bluetooth path on
 * the P4 EV platform is the ESP32-C6 co-processor attached over SPI, running
 * the SoC firmware that provides an HCI transport.
 *
 * This file currently implements ONLY the NuttX-side glue surface so that
 * board applications (blesak/bledctl/lvbled/etc.) can build and run against
 * a stable state machine.  All silicon interaction is stubbed out; each
 * function logs its invocation and reports the recorded state.
 *
 * TODO (real bring-up, see board README Bluetooth section):
 *   1. Confirm the ESP32-C6 sub-board mounting (EV board "J" section strap
 *      defaults) and the null-modem SPI wiring to the P4 (CLK/MOSI/MISO/CS
 *      GPIOs + READY/HANDSHAKE handshake lines).
 *   2. Implement the HCI-over-SPI transport driver in this file (chip side),
 *      then expose it as a NuttX Bluetooth radio driver so the in-kernel
 *      host stack (CONFIG_WIRELESS_BLUETOOTH) attaches through
 *      bt_radio_register() with the standard /dev/bt0 netdev.
 *   3. Power/PHY bring-up: esp_phy calibration for the C6, shared
 *      regulator, and coexistence straps when Wi-Fi is enabled.
 *
 ****************************************************************************/

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>

#include <debug.h>
#include <errno.h>
#include <stdbool.h>
#include <string.h>
#include <sys/types.h>

#include <nuttx/kmalloc.h>
#include <nuttx/mutex.h>

/* The stack-level Bluetooth include is only guaranteed when the NuttX
 * BLE host stack is enabled; the stub API is designed to build standalone.
 */

#if defined(CONFIG_DEBUG_WIRELESS)
#  define ble_dbg(format, ...) _err(format, ##__VA_ARGS__)
#else
#  define ble_dbg(format, ...) _info(format, ##__VA_ARGS__)
#endif

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/* Maximum advertising device name we accept from the caller */

#define ESP_BLE_MAX_NAME_LEN       32

/* Default local device name used by "bledctl adv" without argument */

#define ESP_BLE_DEFAULT_NAME       "openvela-p4"

/****************************************************************************
 * Private Types
 ****************************************************************************/

enum esp_ble_state_e
{
  ESP_BLE_STATE_UNINIT = 0,       /* Controller driver not initialized */
  ESP_BLE_STATE_READY,            /* Initialized, not advertising */
  ESP_BLE_STATE_ADVERTISING       /* Advertising active */
};

/****************************************************************************
 * Private Data
 ****************************************************************************/

/* Stub controller state.  Protected by g_esp_ble_lock. */

static mutex_t g_esp_ble_lock = NXMUTEX_INITIALIZER;
static enum esp_ble_state_e g_esp_ble_state = ESP_BLE_STATE_UNINIT;
static char g_esp_ble_name[ESP_BLE_MAX_NAME_LEN + 1];

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: esp_ble_bled_init
 *
 * Description:
 *   Initialize the (stub) BLE controller.  On a real implementation this
 *   would power up the ESP32-C6 co-processor, load/reset it, and attach
 *   the HCI transport.  For now it only flips the stub state machine.
 *
 * Input Parameters:
 *   None.
 *
 * Returned Value:
 *   OK (0) on success, negative errno on failure.
 *
 ****************************************************************************/

int esp_ble_bled_init(void)
{
  int ret;

  ret = nxmutex_lock(&g_esp_ble_lock);
  if (ret < 0)
    {
      return ret;
    }

  if (g_esp_ble_state != ESP_BLE_STATE_UNINIT)
    {
      nxmutex_unlock(&g_esp_ble_lock);
      return OK; /* Already initialized */
    }

  /* TODO: ESP32-C6 co-processor bring-up happens here:
   *       - claim SPI bus / GPIO handshake lines
   *       - reset the C6 BLE core and verify HCI reset sequence
   */

  g_esp_ble_state = ESP_BLE_STATE_READY;
  nxmutex_unlock(&g_esp_ble_lock);

  ble_dbg("esp_ble: stub controller initialized (no radio attached)\n");
  return OK;
}

/****************************************************************************
 * Name: esp_ble_bled_start_adv
 *
 * Description:
 *   Start BLE advertising with the given local name (ADV data uses a
 *   shortened GAP name entry only).
 *
 * Input Parameters:
 *   name - Local device name to advertise.  Passing NULL selects
 *          ESP_BLE_DEFAULT_NAME.
 *
 * Returned Value:
 *   OK (0) on success, negative errno on failure.
 *
 ****************************************************************************/

int esp_ble_bled_start_adv(const char *name)
{
  int ret;
  size_t len = 0;

  if (name != NULL)
    {
      len = strlen(name);
      if (len == 0)
        {
          return -EINVAL;
        }
    }

  ret = nxmutex_lock(&g_esp_ble_lock);
  if (ret < 0)
    {
      return ret;
    }

  if (g_esp_ble_state == ESP_BLE_STATE_UNINIT)
    {
      /* Lazy init for convenience when called straight from an app */

      nxmutex_unlock(&g_esp_ble_lock);
      ret = esp_ble_bled_init();
      if (ret < 0)
        {
          return ret;
        }

      ret = nxmutex_lock(&g_esp_ble_lock);
      if (ret < 0)
        {
          return ret;
        }
    }

  if (name != NULL)
    {
      strncpy(g_esp_ble_name, name, ESP_BLE_MAX_NAME_LEN);
      g_esp_ble_name[ESP_BLE_MAX_NAME_LEN] = '\0';
    }

  /* TODO: hand the adv payload to the C6 HCI transport (LE_SET_ADV_DATA,
   *       LE_SET_ADV_ENABLE).  For now we just record the state; the stub
   *       never transmits anything on air.
   */

  g_esp_ble_state = ESP_BLE_STATE_ADVERTISING;
  nxmutex_unlock(&g_esp_ble_lock);

  if (name == NULL)
    {
      _info("esp_ble: stub advertising started (default name '%s', "
           "no RF attached yet)\n", ESP_BLE_DEFAULT_NAME);
    }
  else
    {
      _info("esp_ble: stub advertising started (name '%s', "
           "no RF attached yet)\n", g_esp_ble_name);
    }

  return OK;
}

/****************************************************************************
 * Name: esp_ble_bled_stop_adv
 *
 * Description:
 *   Stop BLE advertising.
 *
 * Returned Value:
 *   OK (0) on success, negative errno on failure.
 *
 ****************************************************************************/

int esp_ble_bled_stop_adv(void)
{
  int ret;

  ret = nxmutex_lock(&g_esp_ble_lock);
  if (ret < 0)
    {
      return ret;
    }

  g_esp_ble_state = ESP_BLE_STATE_READY;
  nxmutex_unlock(&g_esp_ble_lock);

  ble_dbg("esp_ble: stub advertising stopped\n");
  return OK;
}

/****************************************************************************
 * Name: esp_ble_bled_state
 *
 * Description:
 *   Query the current stub controller state.
 *
 * Returned Value:
 *    0 - not initialized
 *    1 - ready / idle
 *    2 - advertising active
 *
 ****************************************************************************/

int esp_ble_bled_state(void)
{
  int ret;
  int state;

  ret = nxmutex_lock(&g_esp_ble_lock);
  if (ret < 0)
    {
      return ret;
    }

  state = (int)g_esp_ble_state;
  nxmutex_unlock(&g_esp_ble_lock);

  return state;
}

/****************************************************************************
 * Name: esp_ble_bled_getname
 *
 * Description:
 *   Return the currently configured advertising name.
 *
 * Input Parameters:
 *   buf      - output buffer, may be NULL to only query the length.
 *   bufsize  - size of buf.
 *
 * Returned Value:
 *   Number of characters copied on success, negative errno on failure.
 *
 ****************************************************************************/

int esp_ble_bled_getname(char *buf, size_t bufsize)
{
  int ret;
  size_t len;

  ret = nxmutex_lock(&g_esp_ble_lock);
  if (ret < 0)
    {
      return ret;
    }

  if (g_esp_ble_name[0] == '\0')
    {
      len = strlen(ESP_BLE_DEFAULT_NAME);
      if (buf != NULL && bufsize > 0)
        {
          strncpy(buf, ESP_BLE_DEFAULT_NAME, bufsize - 1);
          buf[bufsize - 1] = '\0';
        }
    }
  else
    {
      len = strlen(g_esp_ble_name);
      if (buf != NULL && bufsize > 0)
        {
          strncpy(buf, g_esp_ble_name, bufsize - 1);
          buf[bufsize - 1] = '\0';
        }
    }

  nxmutex_unlock(&g_esp_ble_lock);
  return (int)len;
}
