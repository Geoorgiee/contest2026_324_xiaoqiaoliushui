/****************************************************************************
 * vendor_esp32p4/boards/risc-v/esp32p4/esp32p4-evb/src/esp32p4_ml_demo.c
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

#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <errno.h>
#include <math.h>

#include <nuttx/board.h>

#ifdef CONFIG_ESP32P4_TINYML

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/* Model arena size from Kconfig or default */

#ifdef CONFIG_ESP32P4_TFLITE_ARENA_SIZE
#  define TFLITE_ARENA_SIZE (CONFIG_ESP32P4_TFLITE_ARENA_SIZE * 1024)
#else
#  define TFLITE_ARENA_SIZE (64 * 1024)
#endif

/* Simple sine wave model parameters */

#define SINE_MODEL_INPUT_SIZE   1
#define SINE_MODEL_OUTPUT_SIZE  1
#define SINE_MODEL_SAMPLES      100

/****************************************************************************
 * Private Types
 ****************************************************************************/

/* Structure to hold ML demo context */

struct ml_demo_context_s
{
  float *arena;            /* Tensor arena memory */
  size_t arena_size;       /* Arena size in bytes */
  bool initialized;        /* Initialization flag */
};

/****************************************************************************
 * Private Data
 ****************************************************************************/

static struct ml_demo_context_s g_ml_demo;

/****************************************************************************
 * Private Functions
 ****************************************************************************/

/****************************************************************************
 * Name: ml_demo_simple_inference
 *
 * Description:
 *   Perform a simple inference demonstration without TFLite Micro.
 *   This demonstrates the concept of ML inference using a simple
 *   mathematical function (sine approximation).
 *
 * Input Parameters:
 *   input - Input value (angle in radians)
 *   output - Output pointer for result
 *
 * Returned Value:
 *   0 on success, negative errno on failure
 *
 ****************************************************************************/

static int ml_demo_simple_inference(float input, float *output)
{
  if (output == NULL)
    {
      return -EINVAL;
    }

  /* Simple sine approximation using Taylor series:
   * sin(x) ≈ x - x^3/6 + x^5/120 - x^7/5040
   * This is a placeholder for actual TFLite model inference.
   */

  float x = input;
  float x2 = x * x;
  float x3 = x2 * x;
  float x5 = x3 * x2;
  float x7 = x5 * x2;

  *output = x - (x3 / 6.0f) + (x5 / 120.0f) - (x7 / 5040.0f);

  return OK;
}

/****************************************************************************
 * Name: ml_demo_run_sine_test
 *
 * Description:
 *   Run the sine wave prediction test.
 *   This demonstrates ML inference by predicting sine values.
 *
 * Returned Value:
 *   0 on success, negative errno on failure
 *
 ****************************************************************************/

static int ml_demo_run_sine_test(void)
{
  float input;
  float predicted;
  float actual;
  float error;
  float total_error = 0.0f;
  int i;

  syslog(LOG_INFO, "ML Demo: Running sine wave prediction test\n");
  syslog(LOG_INFO, "------------------------------------------\n");
  syslog(LOG_INFO, "  Input   | Predicted |   Actual  |  Error\n");
  syslog(LOG_INFO, "------------------------------------------\n");

  for (i = 0; i < SINE_MODEL_SAMPLES; i += 10)
    {
      /* Generate input: 0 to 2*PI */

      input = (float)i / SINE_MODEL_SAMPLES * 2.0f * M_PI;

      /* Run inference */

      int ret = ml_demo_simple_inference(input, &predicted);
      if (ret < 0)
        {
          syslog(LOG_ERR, "ERROR: Inference failed: %d\n", ret);
          return ret;
        }

      /* Calculate actual sine value */

      actual = sinf(input);
      error = fabsf(predicted - actual);
      total_error += error;

      /* Print results */

      syslog(LOG_INFO, "  %6.3f |  %7.4f  |  %7.4f  | %7.4f\n",
             input, predicted, actual, error);
    }

  syslog(LOG_INFO, "------------------------------------------\n");
  syslog(LOG_INFO, "Average error: %.4f\n",
         total_error / (SINE_MODEL_SAMPLES / 10));

  return OK;
}

/****************************************************************************
 * Name: ml_demo_init
 *
 * Description:
 *   Initialize the ML demo environment.
 *   Allocates tensor arena and prepares for inference.
 *
 * Returned Value:
 *   0 on success, negative errno on failure
 *
 ****************************************************************************/

static int ml_demo_init(void)
{
  syslog(LOG_INFO, "ML Demo: Initializing TinyML environment\n");

  /* Allocate tensor arena from PSRAM if available */

  g_ml_demo.arena_size = TFLITE_ARENA_SIZE;
  g_ml_demo.arena = (float *)malloc(g_ml_demo.arena_size);

  if (g_ml_demo.arena == NULL)
    {
      syslog(LOG_ERR, "ERROR: Failed to allocate tensor arena (%d bytes)\n",
             g_ml_demo.arena_size);
      return -ENOMEM;
    }

  memset(g_ml_demo.arena, 0, g_ml_demo.arena_size);
  g_ml_demo.initialized = true;

  syslog(LOG_INFO, "ML Demo: Tensor arena allocated at %p (%d bytes)\n",
         g_ml_demo.arena, g_ml_demo.arena_size);

  return OK;
}

/****************************************************************************
 * Name: ml_demo_deinit
 *
 * Description:
 *   Clean up ML demo resources.
 *
 ****************************************************************************/

static void ml_demo_deinit(void)
{
  if (g_ml_demo.arena != NULL)
    {
      free(g_ml_demo.arena);
      g_ml_demo.arena = NULL;
    }

  g_ml_demo.initialized = false;
  syslog(LOG_INFO, "ML Demo: Resources released\n");
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: esp32p4_ml_demo_main
 *
 * Description:
 *   Main entry point for the TinyML demo application.
 *   This demonstrates TensorFlow Lite Micro capabilities on ESP32-P4.
 *
 *   Usage: esp32p4_ml_demo [options]
 *
 *   Options:
 *     -h    Show help
 *     -s    Run sine wave test
 *     -a    Show arena info
 *
 ****************************************************************************/

int main(int argc, char *argv[])
{
  int ret;
  bool run_sine = false;
  bool show_arena = false;
  int i;

  syslog(LOG_INFO, "ESP32-P4 TinyML Demo v1.0\n");
  syslog(LOG_INFO, "Tensor Arena Size: %d KB\n",
         TFLITE_ARENA_SIZE / 1024);

  /* Parse command line arguments */

  for (i = 1; i < argc; i++)
    {
      if (strcmp(argv[i], "-h") == 0)
        {
          printf("ESP32-P4 TinyML Demo\n");
          printf("Usage: esp32p4_ml_demo [options]\n");
          printf("Options:\n");
          printf("  -h    Show this help\n");
          printf("  -s    Run sine wave prediction test\n");
          printf("  -a    Show arena information\n");
          return OK;
        }
      else if (strcmp(argv[i], "-s") == 0)
        {
          run_sine = true;
        }
      else if (strcmp(argv[i], "-a") == 0)
        {
          show_arena = true;
        }
    }

  /* Default: run all demos if no options specified */

  if (!run_sine && !show_arena)
    {
      run_sine = true;
      show_arena = true;
    }

  /* Initialize ML environment */

  ret = ml_demo_init();
  if (ret < 0)
    {
      syslog(LOG_ERR, "ERROR: ML demo initialization failed: %d\n", ret);
      return EXIT_FAILURE;
    }

  /* Show arena information */

  if (show_arena)
    {
      syslog(LOG_INFO, "\n=== Tensor Arena Information ===\n");
      syslog(LOG_INFO, "  Address: %p\n", g_ml_demo.arena);
      syslog(LOG_INFO, "  Size:    %d bytes (%d KB)\n",
             g_ml_demo.arena_size, g_ml_demo.arena_size / 1024);
      syslog(LOG_INFO, "  Status:  %s\n",
             g_ml_demo.initialized ? "Initialized" : "Not initialized");
    }

  /* Run sine wave prediction test */

  if (run_sine)
    {
      syslog(LOG_INFO, "\n=== Sine Wave Prediction Test ===\n");
      ret = ml_demo_run_sine_test();
      if (ret < 0)
        {
          syslog(LOG_ERR, "ERROR: Sine wave test failed: %d\n", ret);
        }
    }

  /* Clean up */

  ml_demo_deinit();

  syslog(LOG_INFO, "\nML Demo: Complete\n");
  return EXIT_SUCCESS;
}

#endif /* CONFIG_ESP32P4_TINYML */
