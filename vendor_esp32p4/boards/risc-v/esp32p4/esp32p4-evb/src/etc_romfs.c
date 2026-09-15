/****************************************************************************
 * vendor_esp32p4/boards/risc-v/esp32p4/esp32p4-evb/src/etc_romfs.c
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

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/* Minimal ROMFS image for /etc filesystem.
 * This provides a basic init.d/rcS script that prints a startup message.
 */

/****************************************************************************
 * Private Data
 ****************************************************************************/

/* Minimal ROMFS image containing an empty /etc directory.
 * The ROMFS format requires at least a root directory entry.
 */

const unsigned char romfs_img[] =
{
  /* ROMFS header: "-rom1fs-", volume label "etc" (padded to 16 bytes) */
  0x2d, 0x72, 0x6f, 0x6d, 0x31, 0x66, 0x73, 0x2d,  /* "-rom1fs-" */
  0x00, 0x00, 0x00, 0x40,  /* size = 64 bytes */
  0x00, 0x00, 0x00, 0x00,  /* checksum */
  0x65, 0x74, 0x63, 0x00, 0x00, 0x00, 0x00, 0x00,  /* "etc" padded */
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  /* First file entry: empty directory "." */
  0x00, 0x00, 0x00, 0x10,  /* next=0, info=0, type=1 (directory) */
  0x00, 0x00, 0x00, 0x00,  /* size=0 */
  0x00, 0x00, 0x00, 0x00,  /* checksum */
  0x2e, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  /* "." padded */
};

const unsigned int romfs_img_len = sizeof(romfs_img);
