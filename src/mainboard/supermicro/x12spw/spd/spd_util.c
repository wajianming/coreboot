/* SPDX-License-Identifier: GPL-2.0-only */

#include <arch/cpu.h>
#include <intelblocks/mp_init.h>
#include <stdint.h>
#include <string.h>

#include "spd.h"

enum icl_dimm_type {
	icl_u_ddr4 = 0,
	icl_u_lpddr4 = 1,
	icl_u_lpddr4_type_3 = 4,
	icl_y_lpddr4 = 6
};

void mainboard_fill_dq_map_ch0(void *dq_map_ptr)
{
	/* DQ byte map Ch0 */
	const u8 dq_map[12] = {
		0x0F, 0xF0, 0x0F, 0xF0, 0xFF, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };

	memcpy(dq_map_ptr, dq_map, sizeof(dq_map));
}

void mainboard_fill_dq_map_ch1(void *dq_map_ptr)
{
	const u8 dq_map[12] = {
		0x0F, 0xF0, 0x0F, 0xF0, 0xFF, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };

	memcpy(dq_map_ptr, dq_map, sizeof(dq_map));
}

void mainboard_fill_dqs_map_ch0(void *dqs_map_ptr)
{
	/* DQS CPU<>DRAM map Ch0 */
	const u8 dqs_map_u_ddr[8] = { 2, 0, 1, 3, 6, 4, 7, 5 };

	memcpy(dqs_map_ptr, dqs_map_u_ddr, sizeof(dqs_map_u_ddr));
}

void mainboard_fill_dqs_map_ch1(void *dqs_map_ptr)
{
	/* DQS CPU<>DRAM map Ch1 */
	const u8 dqs_map_u_ddr[8] = { 1, 3, 2, 0, 5, 7, 6, 4 };

	memcpy(dqs_map_ptr, dqs_map_u_ddr, sizeof(dqs_map_u_ddr));
}

void mainboard_fill_rcomp_res_data(void *rcomp_ptr)
{
	/* Rcomp resistor */
	const u16 RcompResistor[3] = { 100, 100, 100 };
	memcpy(rcomp_ptr, RcompResistor, sizeof(RcompResistor));
}

void mainboard_fill_rcomp_strength_data(void *rcomp_strength_ptr)
{
	/* Rcomp target */
	static const u16 RcompTarget_DDR4[5] = {
			100, 33, 32, 33, 28 };

    memcpy(rcomp_strength_ptr, RcompTarget_DDR4,
            sizeof(RcompTarget_DDR4));
}
