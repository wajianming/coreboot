/*
 * This file is part of the coreboot project.
 *
 * Copyright (C) 2005 - 2008 Advanced Micro Devices, Inc.
 * Copyright (C) 2005 coresystems GmbH
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA
 */

#include <types.h>
#include <lib.h>
#include <console.h>
#include <device/pci.h>
#include <device/pcix.h>
#include <device/pci_ids.h>

/* We don't implement this because:
 * 1. There's only one pair of registers for both devices.
 *      - This breaks our model for resource allocation.
 * 2. The datasheet recommends against it.
 */
/* #define NPUML 0xD9 Non prefetchable upper memory limit */
/* #define NPUMB 0xD8 Non prefetchable upper memory base */

static void amd8132_walk_children(struct bus *bus,
	void (*visit)(struct device * dev, void *ptr), void *ptr)
{
	struct device * child;
	for(child = bus->children; child; child = child->sibling)
	{
		if (child->path.type != DEVICE_PATH_PCI) {
			continue;
		}
		if (child->hdr_type == PCI_HEADER_TYPE_BRIDGE) {
			amd8132_walk_children(&child->link[0], visit, ptr);
		}
		visit(child, ptr);
	}
}

struct amd8132_bus_info {
	unsigned sstatus;
	unsigned rev;
	int master_devices;
	int max_func;
};

static void amd8132_count_dev(struct device * dev, void *ptr)
{
	struct amd8132_bus_info *info = ptr;
	/* Don't count pci bridges */
	if (dev->hdr_type != PCI_HEADER_TYPE_BRIDGE) {
		info->master_devices++;
	}
	if (PCI_FUNC(dev->path.pci.devfn) > info->max_func) {
		info->max_func = PCI_FUNC(dev->path.pci.devfn);
	}
}


static void amd8132_pcix_tune_dev(struct device * dev, void *ptr)
{
	struct amd8132_bus_info *info = ptr;
	unsigned cap;
	unsigned status, cmd, orig_cmd;
	unsigned max_read, max_tran;
	int  sibs;

	if (dev->hdr_type != PCI_HEADER_TYPE_NORMAL) {
		return;
	}
	cap = pci_find_capability(dev, PCI_CAP_ID_PCIX);
	if (!cap) {
		return;
	}
	/* How many siblings does this device have? */
	sibs = info->master_devices - 1;

	printk(BIOS_DEBUG, "%s AMD8132 PCI-X tuning\n", dev_path(dev));
	status = pci_read_config32(dev, cap + PCI_X_STATUS);
	orig_cmd = cmd = pci_read_config16(dev,cap + PCI_X_CMD);

	max_read = (status & PCI_X_STATUS_MAX_READ) >> 21;
	max_tran = (status & PCI_X_STATUS_MAX_SPLIT) >> 23;

	if (info->rev == 0x01) { // only a1 need it 
		/* Errata #53 Limit the number of split transactions to avoid starvation */
		if (sibs >= 2) {
			/* At most 2 outstanding split transactions when we have
			* 3 or more bus master devices on the bus.
			*/
			if (max_tran > 1) {
				max_tran = 1;
			}
		}
		else if (sibs == 1) {
			/* At most 4 outstanding split transactions when we have
			* 2 bus master devices on the bus.
			*/
			if (max_tran > 3) {
				max_tran = 3;
			}
		}
		else {
			/* At most 8 outstanding split transactions when we have
			* only one bus master device on the bus.
			*/
			if (max_tran > 4) {
				max_tran = 4;
			}
		}
	}

	if (max_read != ((cmd & PCI_X_CMD_MAX_READ) >> 2)) {
		cmd &= ~PCI_X_CMD_MAX_READ;
		cmd |= max_read << 2;
		}
	if (max_tran != ((cmd & PCI_X_CMD_MAX_SPLIT) >> 4)) {
		cmd &= ~PCI_X_CMD_MAX_SPLIT;
		cmd |= max_tran << 4;
	}

	/* Don't attempt to handle PCI-X errors */
	cmd &= ~PCI_X_CMD_DPERR_E;
	if (orig_cmd != cmd) {
		pci_write_config16(dev, cap + PCI_X_CMD, cmd);
	}


}
static unsigned int amd8132_scan_bus(struct bus *bus,
	unsigned min_devfn, unsigned max_devfn, unsigned int max)
{
	struct amd8132_bus_info info;
	unsigned pos;

	/* Find the children on the bus */
	max = pci_scan_bus(bus, min_devfn, max_devfn, max);

	/* Find the revision of the 8132 */
	info.rev = pci_read_config8(bus->dev, PCI_CLASS_REVISION);

	/* Find the pcix capability and get the secondary bus status */
	pos = pci_find_capability(bus->dev, PCI_CAP_ID_PCIX);
	info.sstatus = pci_read_config16(bus->dev, pos + PCI_X_SEC_STATUS);

	/* Print the PCI-X bus speed */
	printk(BIOS_DEBUG, "PCI: %02x: %s sstatus=%04x rev=%02x \n", bus->secondary, pcix_speed(info.sstatus), info.sstatus, info.rev);


	/* Examine the bus and find out how loaded it is */
	info.max_func = 0;
	info.master_devices  = 0;
	amd8132_walk_children(bus, amd8132_count_dev, &info);

#warning Bus disabling disabled for amd8132
#if 0
	/* Disable the bus if there are no devices on it 
	 */
	if (!bus->children)
	{
		unsigned pcix_misc;
		/* Disable all of my children */
		disable_children(bus);

		/* Remember the device is disabled */
		bus->dev->enabled = 0;

		/* Disable the PCI-X clocks */
		pcix_misc = pci_read_config32(bus->dev, 0x40);
		pcix_misc &= ~(0x1f << 16);
		pci_write_config32(bus->dev, 0x40, pcix_misc);
		
		return max;
	}
#endif

	/* If we are in conventional PCI mode nothing more is necessary.
	 */
	if (PCI_X_SSTATUS_MFREQ(info.sstatus) == PCI_X_SSTATUS_CONVENTIONAL_PCI) {
		return max;
	}

	/* Tune the devices on the bus */
	amd8132_walk_children(bus, amd8132_pcix_tune_dev, &info);

	return max;
}

static unsigned int amd8132_scan_bridge(struct device * dev, unsigned int max)
{
	return do_pci_scan_bridge(dev, max, amd8132_scan_bus);
}


static void amd8132_pcix_init(struct device * dev)
{
	u32 dword;
	u8 byte;
	unsigned chip_rev;

	/* Find the revision of the 8132 */
	chip_rev = pci_read_config8(dev, PCI_CLASS_REVISION);

	/* Enable memory write and invalidate ??? */
	dword = pci_read_config32(dev, PCI_COMMAND);
	dword |= 0x10;
	dword &= ~(1<<6); // PERSP Parity Error Response
	pci_write_config32(dev, PCI_COMMAND, dword);

	if (chip_rev == 0x01) {
		/* Errata #37 */
		byte = pci_read_config8(dev, PCI_CACHE_LINE_SIZE);
		if(byte == 0x08 )
			pci_write_config8(dev, PCI_CACHE_LINE_SIZE, 0x10);

#if 0
		/* Errata #59*/
		dword = pci_read_config32(dev, 0x40);
		dword &= ~(1<<31);
		pci_write_config32(dev, 0x40, dword);
#endif

	}

	/* Set up error reporting, enable all */
	/* system error enable */
	dword = pci_read_config32(dev, PCI_COMMAND);
	dword |= (1<<8);
	pci_write_config32(dev, PCI_COMMAND, dword);
	
	/* system and error parity enable */
	dword = pci_read_config32(dev, 0x3c);
        dword |= (3<<16);
        pci_write_config32(dev, 0x3c, dword);

        dword = pci_read_config32(dev, 0x40);
//        dword &= ~(1<<31); /* WriteChainEnable */
	dword |= (1<<31); 
	dword |= (1<<7);// must set to 1
	dword |= (3<<21); //PCIErrorSerrDisable
        pci_write_config32(dev, 0x40, dword);

        /* EXTARB = 1, COMPAT = 0 */
        dword = pci_read_config32(dev, 0x48);
        dword |= (1<<3);
	dword &= ~(1<<0);
	dword |= (1<<15); //CLEARPCILOG_L
	dword |= (1<<19); //PERR FATAL Enable
	dword |= (1<<22); // SERR FATAL Enable
	dword |= (1<<23); // LPMARBENABLE
	dword |= (0x61<<24); //LPMARBCOUNT
        pci_write_config32(dev, 0x48, dword);

        dword = pci_read_config32(dev, 0x4c);
        dword |= (1<<6); //intial prefetch for memory read line request
	dword |= (1<<9); //continuous prefetch Enable for memory read line request
        pci_write_config32(dev, 0x4c, dword);


       /* Disable Single-Bit-Error Correction [30] = 0 */
        dword = pci_read_config32(dev, 0x70);
        dword &= ~(1<<30);
        pci_write_config32(dev, 0x70, dword);

	//link
        dword = pci_read_config32(dev, 0xd4);
        dword |= (0x5c<<16);
        pci_write_config32(dev, 0xd4, dword);

        /* TxSlack0 [16:17] = 0, RxHwLookahdEn0 [18] = 1, TxSlack1 [24:25] = 0, RxHwLookahdEn1 [26] = 1 */
        dword = pci_read_config32(dev, 0xdc);
	dword |= (1<<1) |  (1<<4); // stream disable 1 to 0 , DBLINSRATE
        dword |= (1<<18)|(1<<26);
        dword &= ~((3<<16)|(3<<24));
        pci_write_config32(dev, 0xdc, dword);

	/* Set up CRC flood enable */
	dword = pci_read_config32(dev, 0xc0);
	if(dword) {  /* do device A only */
#if 0
		dword = pci_read_config32(dev, 0xc4);
		dword |= (1<<1);
		pci_write_config32(dev, 0xc4, dword);
		dword = pci_read_config32(dev, 0xc8);
		dword |= (1<<1);
		pci_write_config32(dev, 0xc8, dword);
#endif

	        if (chip_rev == 0x11) {
        	        /* [18] Clock Gate Enable = 1 */
                	dword = pci_read_config32(dev, 0xf0);
	                dword |= 0x00040008;
        	        pci_write_config32(dev, 0xf0, dword);
	        }

	}
	return;
}

struct device_operations amd8132_pcix = {
	.id = {.type = DEVICE_ID_PCI,
		{.pci = {.vendor = PCI_VENDOR_ID_AMD,
			 .device = PCI_DEVICE_ID_AMD_8132_PCIX}}},
	.constructor		 = default_device_constructor,
	.reset_bus		 = pci_bus_reset,
	.phase3_scan		 = amd8132_scan_bridge,
	.phase4_read_resources	 = pci_bus_read_resources,
	.phase4_set_resources	 = pci_set_resources,
	.phase5_enable_resources = pci_bus_enable_resources,
	.phase6_init		 = amd8132_pcix_init,
	.ops_pci		 = &pci_bus_ops_pci,
};

static void ioapic_enable(struct device * dev)
{
	u32 value;

	value = pci_read_config32(dev, 0x44);
	if (dev->enabled) {
		value |= ((1 << 1) | (1 << 0));
	} else {
		value &= ~((1 << 1) | (1 << 0));
	}
	pci_write_config32(dev, 0x44, value);
}

static void amd8132_ioapic_init(struct device * dev)
{
        u32 dword;
        unsigned chip_rev;

        /* Find the revision of the 8132 */
        chip_rev = pci_read_config8(dev, PCI_CLASS_REVISION);

        if (chip_rev == 0x01) {
#if 0
                /* Errata #43 */
                dword = pci_read_config32(dev, 0xc8);
		dword |= (0x3<<23);
		pci_write_config32(dev, 0xc8, dword);
#endif

        }


        if( (chip_rev == 0x11) ||(chip_rev == 0x12) ) {
                //for b1 b2
                /* Errata #73 */
                dword = pci_read_config32(dev, 0x80);
                dword |= (0x1f<<5);
                pci_write_config32(dev, 0x80, dword);
                dword = pci_read_config32(dev, 0x88);
                dword |= (0x1f<<5);
                pci_write_config32(dev, 0x88, dword);

                /* Errata #74 */
                dword = pci_read_config32(dev, 0x7c);
                dword &= ~(0x3<<30);
                dword |= (0x01<<30);
                pci_write_config32(dev, 0x7c, dword);
        }

}

static struct pci_operations pci_ops_pci_dev = {
	.set_subsystem    = pci_dev_set_subsystem,
};

struct device_operations amd8132_apic = {
	.id = {.type = DEVICE_ID_PCI,
		{.pci = {.vendor = PCI_VENDOR_ID_AMD,
			 .device = PCI_DEVICE_ID_AMD_8132_IOAPIC}}},
	.constructor		 = default_device_constructor,
	.phase3_scan		 = 0,
	.phase3_chip_setup_dev	 = ioapic_enable,
	.phase4_read_resources	 = pci_dev_read_resources,
	.phase4_set_resources	 = pci_set_resources,
	.phase5_enable_resources = pci_dev_enable_resources,
	.phase6_init		 = amd8132_ioapic_init,
	.ops_pci		 = &pci_ops_pci_dev,
};
