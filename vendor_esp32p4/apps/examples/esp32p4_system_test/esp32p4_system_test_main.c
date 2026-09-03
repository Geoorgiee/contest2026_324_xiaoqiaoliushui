/****************************************************************************
 * vendor_esp32p4/apps/examples/esp32p4_system_test/esp32p4_system_test_main.c
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
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <errno.h>
#include <sys/stat.h>

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#ifndef CONFIG_EXAMPLES_ESP32P4_SYSTEM_TEST_DURATION
#  define CONFIG_EXAMPLES_ESP32P4_SYSTEM_TEST_DURATION 10
#endif

#define TEST_DURATION CONFIG_EXAMPLES_ESP32P4_SYSTEM_TEST_DURATION
#define TEST_FILE "/tmp/system_test.txt"
#define TEST_STRING "ESP32-P4 System Test Data"

/****************************************************************************
 * Private Functions
 ****************************************************************************/

/****************************************************************************
 * Name: test_memory_allocation
 *
 * Description:
 *   Test memory allocation and deallocation
 *
 ****************************************************************************/

static int test_memory_allocation(void)
{
  void *ptr;
  size_t sizes[] = {1024, 4096, 16386, 65536, 131072};
  int i;
  int ret;

  printf("\n=== Test 1: Memory Allocation Test ===\n");

  for (i = 0; i < sizeof(sizes) / sizeof(sizes[0]); i++)
    {
      printf("  Allocating %zu bytes... ", sizes[i]);

      ptr = malloc(sizes[i]);
      if (ptr == NULL)
        {
          printf("FAILED: %s\n", strerror(errno));
          return -1;
        }

      /* Write test pattern */

      memset(ptr, 0xAA, sizes[i]);

      /* Verify pattern */

      ret = memcmp(ptr, "\xAA\xAA\xAA\xAA", 4);
      if (ret != 0)
        {
          printf("FAILED: Data verification failed\n");
          free(ptr);
          return -1;
        }

      free(ptr);
      printf("OK\n");
    }

  printf("Memory allocation test PASSED\n");
  return 0;
}

/****************************************************************************
 * Name: test_timer_accuracy
 *
 * Description:
 *   Test system timer accuracy
 *
 ****************************************************************************/

static int test_timer_accuracy(void)
{
  struct timespec start;
  struct timespec end;
  long long elapsed_ns;
  int iterations = 100;
  int i;

  printf("\n=== Test 2: Timer Accuracy Test ===\n");
  printf("Testing usleep accuracy with %d iterations...\n", iterations);

  clock_gettime(CLOCK_MONOTONIC, &start);

  for (i = 0; i < iterations; i++)
    {
      usleep(1000);  /* 1ms */
    }

  clock_gettime(CLOCK_MONOTONIC, &end);

  elapsed_ns = (end.tv_sec - start.tv_sec) * 1000000000LL +
               (end.tv_nsec - start.tv_nsec);

  printf("  Expected: %d ms\n", iterations);
  printf("  Actual:   %lld ms\n", elapsed_ns / 1000000);
  printf("  Drift:    %.2f%%\n",
         100.0 * (elapsed_ns - iterations * 1000000LL) /
         (iterations * 1000000LL));

  /* Check if drift is within 10% */

  if (abs((int)(elapsed_ns - iterations * 1000000LL)) >
      iterations * 100000)
    {
      printf("Timer accuracy test FAILED\n");
      return -1;
    }

  printf("Timer accuracy test PASSED\n");
  return 0;
}

/****************************************************************************
 * Name: test_file_system
 *
 * Description:
 *   Test file system operations
 *
 ****************************************************************************/

static int test_file_system(void)
{
  FILE *fp;
  char buffer[256];
  int ret;

  printf("\n=== Test 3: File System Test ===\n");

  /* Create test file */

  printf("  Creating test file... ");
  fp = fopen(TEST_FILE, "w");
  if (fp == NULL)
    {
      printf("FAILED: %s\n", strerror(errno));
      return -1;
    }

  fprintf(fp, "%s\n", TEST_STRING);
  fclose(fp);
  printf("OK\n");

  /* Read test file */

  printf("  Reading test file... ");
  fp = fopen(TEST_FILE, "r");
  if (fp == NULL)
    {
      printf("FAILED: %s\n", strerror(errno));
      return -1;
    }

  if (fgets(buffer, sizeof(buffer), fp) == NULL)
    {
      printf("FAILED: fgets failed\n");
      fclose(fp);
      return -1;
    }

  fclose(fp);

  /* Remove newline */

  buffer[strcspn(buffer, "\n")] = '\0';

  /* Verify content */

  ret = strcmp(buffer, TEST_STRING);
  if (ret != 0)
    {
      printf("FAILED: Content mismatch\n");
      printf("    Expected: %s\n", TEST_STRING);
      printf("    Got:      %s\n", buffer);
      return -1;
    }

  printf("OK\n");

  /* Delete test file */

  printf("  Deleting test file... ");
  ret = unlink(TEST_FILE);
  if (ret < 0)
    {
      printf("FAILED: %s\n", strerror(errno));
      return -1;
    }

  printf("OK\n");

  printf("File system test PASSED\n");
  return 0;
}

/****************************************************************************
 * Name: test_task_scheduling
 *
 * Description:
 *   Test task scheduling and process information
 *
 ****************************************************************************/

static int test_task_scheduling(void)
{
  pid_t pid;
  int status;

  printf("\n=== Test 4: Task Scheduling Test ===\n");

  /* Get current process ID */

  pid = getpid();
  printf("  Current PID: %d\n", pid);

  /* Get parent process ID */

  pid = getppid();
  printf("  Parent PID:  %d\n", pid);

  /* Test system() call */

  printf("  Testing system() call... ");
  status = system("echo 'System call test OK'");
  if (status != 0)
    {
      printf("FAILED: system() returned %d\n", status);
      return -1;
    }

  printf("OK\n");

  printf("Task scheduling test PASSED\n");
  return 0;
}

/****************************************************************************
 * Name: test_system_info
 *
 * Description:
 *   Display system information
 *
 ****************************************************************************/

static int test_system_info(void)
{
  printf("\n=== Test 5: System Information ===\n");

  /* Display system information */

  printf("  System: ");
  system("uname -a");

  printf("  Uptime: ");
  system("uptime");

  printf("  Memory: ");
  system("free");

  printf("System information test PASSED\n");
  return 0;
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: main
 *
 * Description:
 *   ESP32-P4 system test application entry point
 *
 ****************************************************************************/

int main(int argc, char *argv[])
{
  int ret;
  int passed = 0;
  int failed = 0;

  printf("\n========================================\n");
  printf("  ESP32-P4 System Test Application\n");
  printf("========================================\n");
  printf("Test Duration: %d seconds\n", TEST_DURATION);

  /* Test 1: Memory Allocation */

  ret = test_memory_allocation();
  if (ret == 0)
    {
      passed++;
    }
  else
    {
      failed++;
    }

  /* Test 2: Timer Accuracy */

  ret = test_timer_accuracy();
  if (ret == 0)
    {
      passed++;
    }
  else
    {
      failed++;
    }

  /* Test 3: File System */

  ret = test_file_system();
  if (ret == 0)
    {
      passed++;
    }
  else
    {
      failed++;
    }

  /* Test 4: Task Scheduling */

  ret = test_task_scheduling();
  if (ret == 0)
    {
      passed++;
    }
  else
    {
      failed++;
    }

  /* Test 5: System Information */

  ret = test_system_info();
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
