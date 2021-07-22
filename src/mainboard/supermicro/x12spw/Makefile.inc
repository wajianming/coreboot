## SPDX-License-Identifier: GPL-2.0-only

subdirs-y += variants/baseboard
CPPFLAGS_common += -I$(src)/mainboard/$(MAINBOARDDIR)/variants/baseboard/include
CPPFLAGS_common += -I$(src)/mainboard/$(MAINBOARDDIR)/include

subdirs-y += spd

bootblock-y += bootblock.c
bootblock-y += variants/$(VARIANT_DIR)/gpio_early.c

romstage-y += romstage_fsp_params.c

ramstage-y += mainboard.c
ramstage-y += variants/$(VARIANT_DIR)/gpio.c
