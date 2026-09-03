/****************************************************************************
 * vendor_esp32p4/apps/examples/esp32p4_gpio_test/esp32p4_gpio_test_main.c
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
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <poll.h>
#include <errno.h>
#include <string.h>

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#ifndef CONFIG_EXAMPLES_ESP32P4_GPIO_TEST_LED_GPIO
#  define CONFIG_EXAMPLES_ESP32P4_GPIO_TEST_LED_GPIO 26
#endif

#ifndef CONFIG_EXAMPLES_ESP32P4_GPIO_TEST_BUTTON_GPIO
#  define CONFIG_EXAMPLES_ESP32P4_GPIO_TEST_BUTTON_GPIO 21
#endif

#ifndef CONFIG_EXAMPLES_ESP32P4_GPIO_TEST_BLINK_DELAY
#  define CONFIG_EXAMPLES_ESP32P4_GPIO_TEST_BLINK_DELAY 500
#endif

#define LED_GPIO    CONFIG_EXAMPLES_ESP32P4_GPIO_TEST_LED_GPIO
#define BUTTON_GPIO CONFIG_EXAMPLES_ESP32P4_GPIO_TEST_BUTTON_GPIO
#define BLINK_DELAY CONFIG_EXAMPLES_ESP32P4_GPIO_TEST_BLINK_DELAY

/****************************************************************************
 * Private Functions
 ****************************************************************************/

/****************************************************************************
 * Name: test_led_blink
 *
 * Description:
 *   Test LED blinking on GPIO 26
 *
 ****************************************************************************/

static int test_led_blink(void)
{
  int fd;
  int ret;
  int value;
  int i;
  char devpath[32];

  printf("\n=== Test 1: LED Blink Test ===\n");
  printf("LED GPIO: %d\n", LED_GPIO);
  printf("Blink delay: %d ms\n", BLINK_DELAY);

  /* Open GPIO device */

  snprintf(devpath, sizeof(devpath), "/dev/gpio%d", LED_GPIO);
  fd = open(devpath, O_WRONLY);
  if (fd < 0)
    {
      printf("ERROR: Failed to open %s: %s\n", devpath, strerror(errno));
      return -1;
    }

  /* Blink LED 10 times */

  printf("Blinking LED 10 times...\n");
  for (i = 0; i < 10; i++)
    {
      /* Turn on LED */

      value = 1;
      ret = write(fd, &value, sizeof(value));
      if (ret < 0)
        {
          printf("ERROR: Failed to write GPIO: %s\n", strerror(errno));
          close(fd);
          return -1;
        }

      printf("  [%d/10] LED ON\n", i + 1);
      usleep(BLINK_DELAY * 1000);

      /* Turn off LED */

      value = 0;
      ret = write(fd, &value, sizeof(value));
      if (ret < 0)
        {
          printf("ERROR: Failed to write GPIO: %s\n", strerror(errno));
          close(fd);
          return -1;
        }

      printf("  [%d/10] LED OFF\n", i + 1);
      usleep(BLINK_DELAY * 1000);
    }

  close(fd);
  printf("LED blink test PASSED\n");
  return 0;
}

/****************************************************************************
 * Name: test_button_read
 *
 * Description:
 *   Test button reading on GPIO 21
 *
 ****************************************************************************/

static int test_button_read(void)
{
  int fd;
  int ret;
  int value;
  char devpath[32];

  printf("\n=== Test 2: Button Read Test ===\n");
  printf("Button GPIO: %d\n", BUTTON_GPIO);

  /* Open GPIO device */

  snprintf(devpath, sizeof(devpath), "/dev/gpio%d", BUTTON_GPIO);
  fd = open(devpath, O_RDONLY);
  if (fd < 0)
    {
      printf("ERROR: Failed to open %s: %s\n", devpath, strerror(errno));
      return -1;
    }

  /* Read button state */

  ret = read(fd, &value, sizeof(value));
  if (ret < 0)
    {
      printf("ERROR: Failed to read GPIO: %s\n", strerror(errno));
      close(fd);
      return -1;
    }

  printf("Button state: %d (%s)\n", value, value ? "released" : "pressed");

  close(fd);
  printf("Button read test PASSED\n");
  return 0;
}

/****************************************************************************
 * Name: test_button_interrupt
 *
 * Description:
 *   Test button interrupt on GPIO 21
 *
 ****************************************************************************/

static int test_button_interrupt(void)
{
  int fd;
  int ret;
  struct pollfd pfd;
  int value;
  int count = 0;
  char devpath[32];

  printf("\n=== Test 3: Button Interrupt Test ===\n");
  printf("Button GPIO: %d\n", BUTTON_GPIO);
  printf("Press BOOT button 5 times to complete test...\n");

  /* Open GPIO device */

  snprintf(devpath, sizeof(devpath), "/dev/gpio%d", BUTTON_GPIO);
  fd = open(devpath, O_RDONLY);
  if (fd < 0)
    {
      printf("ERROR: Failed to open %s: %s\n", devpath, strerror(errno));
      return -1;
    }

  /* Setup poll structure */

  pfd.fd = fd;
  pfd.events = POLLPRI;

  /* Wait for 5 button presses */

  while (count < 5)
    {
      ret = poll(&pfd, 1, 10000);  /* 10 second timeout */
      if (ret < 0)
        {
          printf("ERROR: poll failed: %s\n", strerror(errno));
          close(fd);
          return -1;
        }
      else if (ret == 0)
        {
          printf("TIMEOUT: No button press detected\n");
          close(fd);
          return -1;
        }

      if (pfd.revents & POLLPRI)
        {
          /* Read GPIO value */

          lseek(fd, 0, SEEK_SET);
          ret = read(fd, &value, sizeof(value));
          if (ret < 0)
            {
              printf("ERROR: Failed to read GPIO: %s\n", strerror(errno));
              close(fd);
              return -1;
            }

          count++;
          printf("  [%d/5] Button %s\n", count,
                 value ? "released" : "pressed");
        }
    }

  close(fd);
  printf("Button interrupt test PASSED\n");
  return 0;
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: main
 *
 * Description:
 *   ESP32-P4 GPIO test application entry point
 *
 ****************************************************************************/

int main(int argc, char *argv[])
{
  int ret;
  int passed = 0;
  int failed = 0;

  printf("\n========================================\n");
  printf("  ESP32-P4 GPIO Test Application\n");
  printf("========================================\n");
  printf("LED GPIO:    %d\n", LED_GPIO);
  printf("Button GPIO: %d\n", BUTTON_GPIO);

  /* Test 1: LED Blink */

  ret = test_led_blink();
  if (ret == 0)
    {
      passed++;
    }
  else
    {
      failed++;
    }

  /* Test 2: Button Read */

  ret = test_button_read();
  if (ret == 0)
    {
      passed++;
    }
  else
    {
      failed++;
    }

  /* Test 3: Button Interrupt */

  ret = test_button_interrupt();
  if (ret == 0)
    {
      passed++;
    }
  else
    {
      failed++;
    }

  /* Print summary */

  printf("\n========================================\n");
  printf("  Test Summary\n");
  printf("========================================\n");
  printf("Passed: %d\n", passed);
  printf("Failed: %d\n", failed);
  printf("Total:  %d\n", passed + failed);
  printf("Result: %s\n", failed == 0 ? "ALL TESTS PASSED" : "SOME TESTS FAILED");
  printf("========================================\n");

  return failed == 0 ? 0 : 1;
}
