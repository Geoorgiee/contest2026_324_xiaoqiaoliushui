############################################################################
# vendor_esp32p4/chips/esp32p4/hal_esp32p4.mk
#
# Minimal HAL makefile for ESP32-P4 vendor code.
# This file is included by common/espressif/Make.defs but we override
# most of the common espressif sources with our own vendor implementations.
#
# The vendor code provides its own implementations of:
#   - esp32p4_serial.c (UART driver)
#   - esp32p4_gpio.c (GPIO driver)
#   - esp32p4_irq.c (Interrupt controller)
#   - esp32p4_clockconfig.c (Clock configuration)
#   - esp32p4_start.c (Startup code)
#   - esp32p4_timerisr.c (Timer ISR)
#   - esp32p4_allocateheap.c (Heap allocation)
#
# We do NOT use the common espressif sources for these because the
# vendor code has its own register-level implementations.
#
############################################################################

# Include header paths for vendor code

INCLUDES += $(INCDIR_PREFIX)$(ARCH_SRCDIR)$(DELIM)chip$(DELIM)include
INCLUDES += $(INCDIR_PREFIX)$(ARCH_SRCDIR)$(DELIM)chip$(DELIM)hardware

# Override the context target to prevent cloning esp-hal-3rdparty.
# The vendor code has its own implementations and doesn't need this repo.

context::
	@echo "Using vendor ESP32-P4 code (no esp-hal-3rdparty needed)"

# Override distclean to not try to remove esp-hal-3rdparty

distclean::
	@echo "Vendor ESP32-P4 distclean"

# No additional sources needed - the vendor Make.defs handles everything.
