############################################################################
# vendor_esp32p4/boards/risc-v/esp32p4/esp32p4-evb/scripts/Config.mk
#
# Licensed to the Apache Software Foundation (ASF) under one or more
# contributor license agreements.  See the NOTICE file distributed with
# this work for additional information regarding copyright ownership.  The
# ASF licenses this file to you under the Apache License, Version 2.0 (the
# "License"); you may not use this file except in compliance with the
# License.  You may obtain a copy of the License at
#
#   http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
# WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.  See the
# License for the specific language governing permissions and limitations
# under the License.
#
############################################################################

# ESP32-P4 Flash Tool Configuration
# Reference: tools/esp32c3/Config.mk (simplified for ESP32-P4)

# Flash size configuration (ESP32-P4 has 16MB flash)
FLASH_SIZE := 16MB

# Flash mode (DIO is standard for ESP32-P4)
FLASH_MODE := dio

# Flash frequency (40MHz is safe default for ESP32-P4)
FLASH_FREQ := 40m

# Write flash options
ESPTOOL_WRITEFLASH_OPTS := -fs $(FLASH_SIZE) -fm $(FLASH_MODE) -ff $(FLASH_FREQ)

# Minimum esptool version required
ESPTOOL_MIN_VERSION := 4.8.0

# Application binary (simple boot mode, no MCUboot)
APP_IMAGE := nuttx.bin
FLASH_APP := 0x0 $(APP_IMAGE)
ESPTOOL_BINS := $(FLASH_APP)

# MKIMAGE -- Convert an ELF file into a compatible binary file
# Reference: ESP32-C3's MKIMAGE but adapted for ESP32-P4

define MKIMAGE
	$(Q) echo "MKIMAGE: ESP32-P4 binary"
	@python3 tools/espressif/check_esptool.py -v $(ESPTOOL_MIN_VERSION)
	$(Q) if [ -z "$(FLASH_SIZE)" ]; then \
		echo "Missing Flash memory size configuration for the ESP32-P4 chip."; \
		exit 1; \
	fi
	esptool.py -c esp32p4 elf2image -fs $(FLASH_SIZE) -fm $(FLASH_MODE) -ff $(FLASH_FREQ) -o nuttx.bin nuttx
	$(Q) echo nuttx.bin >> nuttx.manifest
	$(Q) echo "Generated: nuttx.bin (ESP32-P4 compatible)"
endef

# POSTBUILD -- Perform post build operations

define POSTBUILD
	$(call MKIMAGE)
endef

# ESPTOOL_BAUD -- Serial port baud rate used when flashing/reading via esptool.py

ESPTOOL_BAUD ?= 921600

# FLASH -- Download a binary image via esptool.py
# Reference: ESP32-C3's FLASH macro but for ESP32-P4

define FLASH
	$(Q) if [ -z "$(ESPTOOL_PORT)" ]; then \
		echo "FLASH error: Missing serial port device argument."; \
		echo "USAGE: make flash ESPTOOL_PORT=<port> [ ESPTOOL_BAUD=<baud> ]"; \
		exit 1; \
	fi

	$(eval ESPTOOL_OPTS := -c esp32p4 -p $(ESPTOOL_PORT) -b $(ESPTOOL_BAUD))
	esptool.py $(ESPTOOL_OPTS) write_flash $(ESPTOOL_WRITEFLASH_OPTS) $(ESPTOOL_BINS)
endef
