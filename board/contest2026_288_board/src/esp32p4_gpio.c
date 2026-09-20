/****************************************************************************
 * board/contest_board/src/esp32p4_gpio.c
 *
 * SPDX-License-Identifier: Apache-2.0
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

/****************************************************************************
 * GPIO4 输出驱动 - 注册为 /dev/gpio0
 *
 * 工作原理：
 *   1. 定义一个 gpio_operations_s 结构体，包含 read/write 函数指针
 *   2. 在 esp_gpio_init() 中把这个结构体注册给内核
 *   3. 内核自动创建 /dev/gpio0 设备节点
 *   4. 用户空间通过 gpio 命令或 ioctl 操作这个设备
 ****************************************************************************/

/****************************************************************************
 * Included Files
 ****************************************************************************/

/* 内核配置宏，如 CONFIG_DEV_GPIO */

#include <nuttx/config.h>

#include <sys/types.h>
#include <syslog.h>
#include <assert.h>
#include <debug.h>

#include <arch/irq.h>
#include <nuttx/irq.h>

/* GPIO upper-half 框架头文件
 * 定义了 gpio_dev_s, gpio_operations_s, gpio_pin_register() 等 */

#include <nuttx/ioexpander/gpio.h>

/* ESP 芯片层 GPIO 操作函数：
 * esp_configgpio(), esp_gpiowrite(), esp_gpioread(),
 * esp_gpio_matrix_out() */

#include "espressif/esp_gpio.h"

/* 板级头文件，声明 esp_gpio_init() */

#include "esp32p4-function-ev-board.h"

/* 包含 BOARD_NGPIOOUT 等宏 */

#include <arch/board/board.h>

/* 包含 SIG_GPIO_OUT_IDX 常量 */

#include <arch/chip/gpio_sig_map.h>

/* 条件编译：只有 defconfig 里打开了 CONFIG_DEV_GPIO
 * 且没有用 lower-half 模式才编译这个文件 */

#if defined(CONFIG_DEV_GPIO) && !defined(CONFIG_GPIO_LOWER_HALF)

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/* 我们选择的物理 GPIO 引脚编号
 * GPIO4 对应 J1 Pin 18（右列），原理图确认无其他外设占用 */

#define GPIO_OUT1  4

/****************************************************************************
 * Private Types
 ****************************************************************************/

/* 扩展 NuttX 的 gpio_dev_s，加一个 id 字段用来索引数组 */

struct espgpio_dev_s
{
  struct gpio_dev_s gpio;  /* 必须放第一个，这样指针可以互相转换 */
  uint8_t id;              /* 在 g_gpiooutputs[] 数组中的索引 */
};

/****************************************************************************
 * Private Function Prototypes
 ****************************************************************************/

static int gpout_read(struct gpio_dev_s *dev, bool *value);
static int gpout_write(struct gpio_dev_s *dev, bool value);
static int gpout_setpintype(struct gpio_dev_s *dev,
                            enum gpio_pintype_e pintype);

/****************************************************************************
 * Private Data
 ****************************************************************************/

/* 操作函数表 - 告诉 upper-half 框架怎么操作这个引脚 */

static const struct gpio_operations_s gpout_ops =
{
  .go_read       = gpout_read,       /* 读取当前引脚电平 */
  .go_write      = gpout_write,      /* 设置引脚高/低电平 */
  .go_attach     = NULL,             /* 输出引脚不需要中断 */
  .go_enable     = NULL,             /* 输出引脚不需要中断使能 */
  .go_setpintype = gpout_setpintype, /* 动态改变引脚模式（可选） */
};

/* 物理引脚编号数组 - 通过 BOARD_NGPIOOUT 控制大小
 * 我们只有一个元素：GPIO4 */

static const uint32_t g_gpiooutputs[BOARD_NGPIOOUT] =
{
  GPIO_OUT1,   /* 索引 0 -> GPIO4 -> 将注册为 /dev/gpio0 */
};

/* 设备实例数组 - 每个输出引脚一个 */

static struct espgpio_dev_s g_gpout[BOARD_NGPIOOUT];

/****************************************************************************
 * Private Functions
 ****************************************************************************/

/****************************************************************************
 * Name: gpout_read
 *
 * Description:
 *   读取输出引脚当前的电平状态。
 *   即使是输出引脚，也可以回读当前状态。
 *
 ****************************************************************************/

static int gpout_read(struct gpio_dev_s *dev, bool *value)
{
  struct espgpio_dev_s *espgpio = (struct espgpio_dev_s *)dev;

  DEBUGASSERT(espgpio != NULL && value != NULL);
  DEBUGASSERT(espgpio->id < BOARD_NGPIOOUT);

  /* esp_gpioread() 是芯片层函数，读取 GPIO 寄存器的当前输出值 */

  *value = esp_gpioread(g_gpiooutputs[espgpio->id]);
  return OK;
}

/****************************************************************************
 * Name: gpout_write
 *
 * Description:
 *   设置输出引脚的电平。
 *   value = true  -> 高电平 3.3V
 *   value = false -> 低电平 0V
 *
 ****************************************************************************/

static int gpout_write(struct gpio_dev_s *dev, bool value)
{
  struct espgpio_dev_s *espgpio = (struct espgpio_dev_s *)dev;

  DEBUGASSERT(espgpio != NULL);
  DEBUGASSERT(espgpio->id < BOARD_NGPIOOUT);

  /* esp_gpiowrite() 是芯片层函数，直接操作 GPIO 输出寄存器 */

  esp_gpiowrite(g_gpiooutputs[espgpio->id], value);
  return OK;
}

/****************************************************************************
 * Name: gpout_setpintype
 *
 * Description:
 *   动态修改引脚模式。一般不需要用到，但框架要求实现。
 *
 ****************************************************************************/

static int gpout_setpintype(struct gpio_dev_s *dev,
                            enum gpio_pintype_e pintype)
{
  struct espgpio_dev_s *espgpio = (struct espgpio_dev_s *)dev;

  DEBUGASSERT(espgpio != NULL);
  DEBUGASSERT(espgpio->id < BOARD_NGPIOOUT);

  /* esp_gpio_matrix_out() 将 GPIO 连接到"简单 GPIO 输出"信号
   * SIG_GPIO_OUT_IDX 表示不经过任何外设，直接控制引脚电平 */

  esp_gpio_matrix_out(g_gpiooutputs[espgpio->id],
                      SIG_GPIO_OUT_IDX, 0, 0);

  switch (pintype)
    {
      case GPIO_OUTPUT_PIN:
        /* 推挽输出模式（最常用） */

        esp_configgpio(g_gpiooutputs[espgpio->id], INPUT | OUTPUT);
        break;

      case GPIO_OUTPUT_PIN_OPENDRAIN:
        /* 开漏输出模式 */

        esp_configgpio(g_gpiooutputs[espgpio->id],
                       INPUT | OUTPUT_OPEN_DRAIN);
        break;

      default:
        return ERROR;
    }

  return OK;
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: esp_gpio_init
 *
 * Description:
 *   初始化所有 GPIO 输出设备并注册到内核。
 *   由 esp_bringup() 在系统启动时调用。
 *
 * Returned Value:
 *   OK(0) 表示成功。
 *
 ****************************************************************************/

int esp_gpio_init(void)
{
  int pincount = 0;
  int i;
  int ret;

  for (i = 0; i < BOARD_NGPIOOUT; i++)
    {
      /* --- 第一步：填充设备结构体 --- */

      g_gpout[i].gpio.gp_pintype = GPIO_OUTPUT_PIN;  /* 标记为输出类型 */
      g_gpout[i].gpio.gp_ops     = &gpout_ops;       /* 绑定操作函数 */
      g_gpout[i].id              = i;                 /* 记录数组索引 */

      /* --- 第二步：注册设备节点 ---
       * i=0 时注册为 /dev/gpio0 */

      ret = gpio_pin_register(&g_gpout[i].gpio, pincount);
      if (ret < 0)
        {
          syslog(LOG_ERR, "ERROR: Failed to register /dev/gpio%d: %d\n",
                 pincount, ret);
          return ret;
        }

      /* --- 第三步：配置物理引脚硬件 --- */

      /* 将 GPIO4 连接到简单输出信号（不经过 SPI/I2C 等外设矩阵） */

      esp_gpio_matrix_out(g_gpiooutputs[i], SIG_GPIO_OUT_IDX, 0, 0);

      /* 配置为 GPIO 模式，启用输出+可回读
       * 注意：不带 FUNCTION 标志，让 esp_configgpio 选择 PIN_FUNC_GPIO */

      esp_configgpio(g_gpiooutputs[i], INPUT | OUTPUT);

      /* 初始输出低电平（0V） */

      esp_gpiowrite(g_gpiooutputs[i], 0);

      pincount++;
    }

  return OK;
}

#endif /* CONFIG_DEV_GPIO && !CONFIG_GPIO_LOWER_HALF */
