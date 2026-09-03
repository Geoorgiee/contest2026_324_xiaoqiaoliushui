/****************************************************************************
 * vendor_esp32p4/apps/examples/esp32p4_uart_test/esp32p4_uart_test_main.c
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
#include <string.h>
#include <errno.h>
#include <termios.h>

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#ifndef CONFIG_EXAMPLES_ESP32P4_UART_TEST_PORT
#  define CONFIG_EXAMPLES_ESP32P4_UART_TEST_PORT 0
#endif

#ifndef CONFIG_EXAMPLES_ESP32P4_UART_TEST_BAUD
#  define CONFIG_EXAMPLES_ESP32P4_UART_TEST_BAUD 115200
#endif

#define UART_PORT CONFIG_EXAMPLES_ESP32P4_UART_TEST_PORT
#define UART_BAUD CONFIG_EXAMPLES_ESP32P4_UART_TEST_BAUD

#define TEST_STRING "Hello ESP32-P4 UART Test!"
#define TEST_COUNT  5

/****************************************************************************
 * Private Functions
 ****************************************************************************/

/****************************************************************************
 * name: configure_uart
 *
 * Description:
 *   Configure UART port with specified baud rate
 *
 ****************************************************************************/

static int configure_uart(int fd, int baud)
{
  struct termios tio;
  int ret;

  /* Get current terminal attributes */

  ret = tcgetattr(fd, &tio);
  if (ret < 0)
    {
      printf("ERROR: tcgetattr failed: %s\n", strerror(errno));
      return -1;
    }

  /* Set baud rate */

  cfsetispeed(&tio, baud);
  cfsetospeed(&tio, baud);

  /* Set 8N1 (8 data bits, no parity, 1 stop bit) */

  tio.c_cflag &= ~PARENB;
  tio.c_cflag &= ~CSTOPB;
  tio.c_cflag &= ~CSIZE;
  tio.c_cflag |= CS8;

  /* Enable receiver */

  tio.c_cflag |= CREAD;

  /* Set raw input mode */

  tio.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);
  tio.c_iflag &= ~(IXON | IXOFF | IXANY);
  tio.c_oflag &= ~OPOST;

  /* Set terminal attributes */

  ret = tcsetattr(fd, TCSANOW, &tio);
  if (ret < 0)
    {
      printf("ERROR: tcsetattr failed: %s\n", strerror(errno));
      return -1;
    }

  return 0;
}

/****************************************************************************
 * Name: test_uart_write
 *
 * Description:
 *   Test UART data transmission
 *
 ****************************************************************************/

static int test_uart_write(void)
{
  int fd;
  int ret;
  char devpath[32];
  int i;

  printf("\n=== Test 1: UART Write Test ===\n");
  printf("UART Port: %d\n", UART_PORT);
  printf("Baud Rate: %d\n", UART_BAUD);

  /* Open UART device */

  snprintf(devpath, sizeof(devpath), "/dev/ttyS%d", UART_PORT);
  fd = open(devpath, O_WRONLY);
  if (fd < 0)
    {
      printf("ERROR: Failed to open %s: %s\n", devpath, strerror(errno));
      return -1;
    }

  /* Configure UART */

  ret = configure_uart(fd, UART_BAUD);
  if (ret < 0)
    {
      close(fd);
      return -1;
    }

  /* Write test data */

  printf("Writing %d test strings...\n", TEST_COUNT);
  for (i = 0; i < TEST_COUNT; i++)
    {
      char buffer[64];
      int len;

      len = snprintf(buffer, sizeof(buffer), "[%d/%d] %s\n",
                     i + 1, TEST_COUNT, TEST_STRING);

      ret = write(fd, buffer, len);
      if (ret < 0)
        {
          printf("ERROR: write failed: %s\n", strerror(errno));
          close(fd);
          return -1;
        }

      printf("  Sent: %s", buffer);
      usleep(100000);  /* 100ms delay */
    }

  close(fd);
  printf("UART write test PASSED\n");
  return 0;
}

/****************************************************************************
 * Name: test_uart_read
 *
 * Description:
 *   Test UART data reception
 *
 ****************************************************************************/

static int test_uart_read(void)
{
  int fd;
  int ret;
  char devpath[32];
  char buffer[256];
  int total_read = 0;

  printf("\n=== Test 2: UART Read Test ===\n");
  printf("UART Port: %d\n", UART_PORT);
  printf("Waiting for data (5 seconds timeout)...\n");
  printf("Send data from terminal to test...\n");

  /* Open UART device */

  snprintf(devpath, sizeof(devpath), "/dev/ttyS%d", UART_PORT);
  fd = open(devpath, O_RDONLY);
  if (fd < 0)
    {
      printf("ERROR: Failed to open %s: %s\n", devpath, strerror(errno));
      return -1;
    }

  /* Configure UART */

  ret = configure_uart(fd, UART_BAUD);
  if (ret < 0)
    {
      close(fd);
      return -1;
    }

  /* Read data with timeout */

  while (total_read < sizeof(buffer) - 1)
    {
      ret = read(fd, buffer + total_read, 1);
      if (ret < 0)
        {
          if (errno == EAGAIN)
            {
              /* Timeout, exit loop */

              break;
            }

          printf("ERROR: read failed: %s\n", strerror(errno));
          close(fd);
          return -1;
        }
      else if (ret == 0)
        {
          /* No more data */

          break;
        }

      total_read += ret;

      /* Check for newline */

      if (buffer[total_read - 1] == '\n')
        {
          break;
        }
    }

  buffer[total_read] = '\0';

  if (total_read > 0)
    {
      printf("Received %d bytes: %s\n", total_read, buffer);
    }
  else
    {
      printf("No data received\n");
    }

  close(fd);
  printf("UART read test PASSED\n");
  return 0;
}

/****************************************************************************
 * Name: test_uart_loopback
 *
 * Description:
 *   Test UART loopback (requires TX and RX connected)
 *
 ****************************************************************************/

static int test_uart_loopback(void)
{
  int fd;
  int ret;
  char devpath[32];
  char buffer[256];
  int i;

  printf("\n=== Test 3: UART Loopback Test ===\n");
  printf("UART Port: %d\n", UART_PORT);
  printf("NOTE: Requires TX and RX pins connected!\n");

  /* Open UART device */

  snprintf(devpath, sizeof(devpath), "/dev/ttyS%d", UART_PORT);
  fd = open(devpath, O_RDWR);
  if (fd < 0)
    {
      printf("ERROR: Failed to open %s: %s\n", devpath, strerror(errno));
      return -1;
    }

  /* Configure UART */

  ret = configure_uart(fd, UART_BAUD);
  if (ret < 0)
    {
      close(fd);
      return -1;
    }

  /* Write and read back data */

  printf("Sending and receiving %d strings...\n", TEST_COUNT);
  for (i = 0; i < TEST_COUNT; i++)
    {
      char tx_buffer[64];
      char rx_buffer[64];
      int tx_len;
      int rx_len;

      /* Prepare test string */

      tx_len = snprintf(tx_buffer, sizeof(tx_buffer),
                        "Loopback test %d\n", i + 1);

      /* Write data */

      ret = write(fd, tx_buffer, tx_len);
      if (ret < 0)
        {
          printf("ERROR: write failed: %s\n", strerror(errno));
          close(fd);
          return -1;
        }

      /* Read back data */

      rx_len = 0;
      while (rx_len < tx_len)
        {
          ret = read(fd, rx_buffer + rx_len, 1);
          if (ret < 0)
            {
              printf("ERROR: read failed: %s\n", strerror(errno));
              close(fd);
              return -1;
            }
          else if (ret == 0)
            {
              printf("ERROR: No data received\n");
              close(fd);
              return -1;
            }

          rx_len += ret;
        }

      rx_buffer[rx_len] = '\0';

      /* Compare data */

      if (strcmp(tx_buffer, rx_buffer) == 0)
        {
          printf("  [%d/%d] Loopback OK: %s", i + 1, TEST_COUNT,
                 tx_buffer);
        }
      else
        {
          printf("  [%d/%d] Loopback FAILED\n", i + 1, TEST_COUNT);
          printf("    TX: %s\n", tx_buffer);
          printf("    RX: %s\n", rx_buffer);
          close(fd);
          return -1;
        }
    }

  close(fd);
  printf("UART loopback test PASSED\n");
  return 0;
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: main
 *
 * Description:
 *   ESP32-P4 UART test application entry point
 *
 ****************************************************************************/

int main(int argc, char *argv[])
{
  int ret;
  int passed = 0;
  int failed = 0;

  printf("\n========================================\n");
  printf("  ESP32-P4 UART Test Application\n");
  printf("========================================\n");
  printf("UART Port: %d\n", UART_PORT);
  printf("Baud Rate: %d\n", UART_BAUD);

  /* Test 1: UART Write */

  ret = test_uart_write();
  if (ret == 0)
    {
      passed++;
    }
  else
    {
      failed++;
    }

  /* Test 2: UART Read */

  ret = test_uart_read();
  if (ret == 0)
    {
      passed++;
    }
  else
    {
      failed++;
    }

  /* Test 3: UART Loopback */

  ret = test_uart_loopback();
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
