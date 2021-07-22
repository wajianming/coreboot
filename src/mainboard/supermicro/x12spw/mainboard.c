/* SPDX-License-Identifier: GPL-2.0-only */

#include <mainboard/gpio.h>
#include <console/console.h>
#include <soc/ramstage.h>

void mainboard_silicon_init_params(FSPS_UPD *params)
{
	mainboard_configure_gpios();
    printk(BIOS_INFO, "JM: mainboard_chip_init\n");
}
