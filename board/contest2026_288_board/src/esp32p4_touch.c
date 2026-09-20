/****************************************************************************
 * board/contest2026_288_board/src/esp32p4_touch.c
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * I2C1 polled GT911 touchscreen lower-half driver.
 *
 * nuttx has no in-tree GT911 driver suitable for board bring-up here
 * (drivers/input/gt9xx.c requires board irq callbacks and has no product
 * probe), so this file follows the polling lower-half pattern used by
 * boards/contest2026_284_board/src/velapoka_gt911.c: a LPWORK worker polls
 * the 0x814e status register and reports through the touchscreen upper
 * half (touch_register()).
 *
 * TODO(真机):
 *   1. GT911 的 I2C 地址必须先在真机用 i2ctool ("i2c -b 1") 扫描确认,
 *      常见值 0x5d / 0x14 (由 INT 引脚上电状态决定; 本板 INT 若未接
 *      SoC 则无法用上电时序固定地址).
 *   2. 若本板原理图将 FT 图标 GT911 的 INT/RESET 接到了 SoC GPIO,
 *      应改为中断驱动并补充复位时序 (参考 vendor_esp32p4 的
 *      esp32p4_touch.c 注释).
 ****************************************************************************/

#include <nuttx/config.h>

#ifdef CONFIG_ESPRESSIF_BOARD_GT911

#include <errno.h>
#include <string.h>
#include <syslog.h>

#include <nuttx/i2c/i2c_master.h>
#include <nuttx/input/touchscreen.h>
#include <nuttx/wqueue.h>

#include "espressif/esp_i2c.h"

#include "esp32p4-function-ev-board.h"

/* GT911 register map (subset) */

#define GT911_STATUS_REG      0x814e
#define GT911_POINT_REG       0x814f
#define GT911_PRODUCT_ID_REG  0x8140

#define BOARD_GT911_MAX_POINTS  5
#define BOARD_GT911_POINT_BYTES 8
#define BOARD_GT911_BUFFER_SIZE \
  (1 + BOARD_GT911_MAX_POINTS * BOARD_GT911_POINT_BYTES)

/* GT911 7-bit control address; verify on hardware (0x5d / 0x14). */

#define BOARD_GT911_I2C_ADDRESS    CONFIG_ESPRESSIF_BOARD_GT911_I2C_ADDRESS
#define BOARD_GT911_I2C_BUS        ESPRESSIF_I2C1
#define BOARD_GT911_I2C_FREQUENCY  400000

/* Display orientation: report coordinates clamped to 800x1280, both axes
 * mirrored as in the 284 board reference until hardware decides the
 * actual mounting orientation.
 */

#define BOARD_GT911_X_RES      800
#define BOARD_GT911_Y_RES      1280
#define BOARD_GT911_POLL_TICKS MSEC2TICK(20)

struct gt911_dev_s
{
  struct touch_lowerhalf_s lower;
  FAR struct i2c_master_s *i2c;
  struct work_s work;
  uint8_t buffer[BOARD_GT911_BUFFER_SIZE];
  bool contact;
  int16_t last_x;
  int16_t last_y;
  uint8_t last_id;
};

static struct gt911_dev_s g_gt911;

static int gt911_read(FAR struct gt911_dev_s *dev, uint16_t reg,
                      FAR uint8_t *buffer, size_t buflen)
{
  uint8_t regbuf[2] = {reg >> 8, reg & 0xff};
  struct i2c_msg_s msgs[2] =
  {
    {
      .frequency = BOARD_GT911_I2C_FREQUENCY,
      .addr = BOARD_GT911_I2C_ADDRESS,
      .flags = 0,
      .buffer = regbuf,
      .length = sizeof(regbuf),
    },
    {
      .frequency = BOARD_GT911_I2C_FREQUENCY,
      .addr = BOARD_GT911_I2C_ADDRESS,
      .flags = I2C_M_READ,
      .buffer = buffer,
      .length = buflen,
    }
  };

  return I2C_TRANSFER(dev->i2c, msgs, 2);
}

static int gt911_write_u8(FAR struct gt911_dev_s *dev, uint16_t reg,
                          uint8_t value)
{
  uint8_t buffer[3] = {reg >> 8, reg & 0xff, value};
  struct i2c_msg_s msg =
  {
    .frequency = BOARD_GT911_I2C_FREQUENCY,
    .addr = BOARD_GT911_I2C_ADDRESS,
    .flags = 0,
    .buffer = buffer,
    .length = sizeof(buffer),
  };

  return I2C_TRANSFER(dev->i2c, &msg, 1);
}

static uint16_t gt911_get_le16(FAR const uint8_t *value)
{
  return (uint16_t)value[0] | ((uint16_t)value[1] << 8);
}

static void gt911_report(FAR struct gt911_dev_s *dev, bool down,
                         FAR const uint8_t *point_data)
{
  struct touch_sample_s sample;
  FAR struct touch_point_s *point = &sample.point[0];
  uint16_t raw_x;
  uint16_t raw_y;

  memset(&sample, 0, sizeof(sample));
  sample.npoints = 1;

  if (down)
    {
      raw_x = gt911_get_le16(point_data + 1);
      raw_y = gt911_get_le16(point_data + 3);
      dev->last_id = point_data[0];

      if (raw_x >= BOARD_GT911_X_RES)
        {
          raw_x = BOARD_GT911_X_RES - 1;
        }

      if (raw_y >= BOARD_GT911_Y_RES)
        {
          raw_y = BOARD_GT911_Y_RES - 1;
        }

      dev->last_x = BOARD_GT911_X_RES - 1 - raw_x;
      dev->last_y = BOARD_GT911_Y_RES - 1 - raw_y;
    }

  point->id = dev->last_id;
  point->x = dev->last_x;
  point->y = dev->last_y;
  point->pressure = down ? 1 : 0;
  point->flags = TOUCH_ID_VALID | TOUCH_POS_VALID | TOUCH_PRESSURE_VALID;
  point->flags |= down ? (dev->contact ? TOUCH_MOVE : TOUCH_DOWN)
                       : TOUCH_UP;
  dev->contact = down;
  touch_event(dev->lower.priv, &sample);
}

static void gt911_worker(FAR void *arg)
{
  FAR struct gt911_dev_s *dev = arg;
  uint8_t status;
  uint8_t points;
  bool valid;
  int ret;

  ret = gt911_read(dev, GT911_STATUS_REG, dev->buffer, 1);
  if (ret < 0)
    {
      syslog(LOG_ERR, "GT911 status read failed: %d\n", ret);
      goto queue_again;
    }

  status = dev->buffer[0];
  valid = (status & 0x80) != 0;
  points = status & 0x0f;

  if (valid && points > 0 && points <= BOARD_GT911_MAX_POINTS)
    {
      ret = gt911_read(dev, GT911_POINT_REG, &dev->buffer[1],
                       points * BOARD_GT911_POINT_BYTES);
      if (ret >= 0)
        {
          gt911_report(dev, true, &dev->buffer[1]);
          ret = 1;      /* mark valid to clear status below */
        }
      else
        {
          syslog(LOG_ERR, "GT911 point read failed: %d\n", ret);
        }
    }
  else if (dev->contact)
    {
      gt911_report(dev, false, NULL);
    }

  if (valid)
    {
      ret = gt911_write_u8(dev, GT911_STATUS_REG, 0);
      if (ret < 0)
        {
          syslog(LOG_ERR, "GT911 status clear failed: %d\n", ret);
        }
    }

queue_again:
  ret = work_queue(LPWORK, &dev->work, gt911_worker, dev,
                   BOARD_GT911_POLL_TICKS);
  if (ret < 0)
    {
      syslog(LOG_ERR, "GT911 work_queue failed: %d\n", ret);
    }
}

int esp32p4_gt911_initialize(void)
{
  FAR struct gt911_dev_s *dev = &g_gt911;
  uint8_t product_id[4];
  int ret;

  memset(dev, 0, sizeof(*dev));

  dev->i2c = esp_i2cbus_initialize(BOARD_GT911_I2C_BUS);
  if (dev->i2c == NULL)
    {
      syslog(LOG_ERR, "GT911: failed to initialize I2C%d\n",
             BOARD_GT911_I2C_BUS);
      return -ENODEV;
    }

  /* TODO(真机): 若实际地址为 0x14, 把 CONFIG_ESPRESSIF_BOARD_GT911_I2C_ADDRESS
   * 改为 0x14 即可, 无需改代码. */

  ret = gt911_read(dev, GT911_PRODUCT_ID_REG, product_id,
                   sizeof(product_id));
  if (ret < 0)
    {
      syslog(LOG_ERR, "GT911: probe 0x%02x failed: %d\n",
             BOARD_GT911_I2C_ADDRESS, ret);
      esp_i2cbus_uninitialize(dev->i2c);
      return ret;
    }

  ret = touch_register(&dev->lower, "/dev/input/event0", 2);
  if (ret < 0)
    {
      syslog(LOG_ERR, "GT911: touch_register failed: %d\n", ret);
      esp_i2cbus_uninitialize(dev->i2c);
      return ret;
    }

  ret = work_queue(LPWORK, &dev->work, gt911_worker, dev,
                   BOARD_GT911_POLL_TICKS);
  if (ret < 0)
    {
      syslog(LOG_ERR, "GT911: work_queue failed: %d\n", ret);
      return ret;
    }

  syslog(LOG_INFO,
         "GT911: product %.4s registered at /dev/input0 (I2C%d 0x%02x)\n",
         product_id, BOARD_GT911_I2C_BUS, BOARD_GT911_I2C_ADDRESS);
  return OK;
}

#endif /* CONFIG_ESPRESSIF_BOARD_GT911 */
