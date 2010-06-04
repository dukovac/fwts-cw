/*
 * Copyright (C) 2006, Intel Corporation
 * Copyright (C) 2010 Canonical
 *
 * This code was originally part of the Linux-ready Firmware Developer Kit
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>

#include "framework.h"
#include "iasl.h"

static text_list *klog;

#define HPET_REG_SIZE  (0x400)
#define MAX_CLK_PERIOD (100000000)

static unsigned long long hpet_base_p = 0;
static void     *hpet_base_v = 0;

#if 0
/* check_hpet_base_hpet() -- used to parse the HPET Table for HPET base info */
static void check_hpet_base_hpet(void)
{
        unsigned long address = 0;
        unsigned long size = 0;
	struct hpet_table *table;
	char *table_ptr;

	if (locate_acpi_table("HPET", &address, &size))
		return;

        if (address == 0 || address == -1) 
                return;

        table = (struct hpet_table *) address;

	hpet_base_p = table->base_address.address;
	free((void *) address);
}
#endif

/* check_hpet_base_dsdt() -- used to parse the DSDT for HPET base info */
static void hpet_check_base_acpi_table(log *results, framework *fw, char *table, int which)
{
	char *val, *idx;
	int hpet_found = 0;

	text_list *output;
	text_list_element *item;

	output = iasl_disassemble(results, fw, table, which);
	if (output == NULL)
		return;

	for (item = output->head; item != NULL; item = item->next) {
		if (!hpet_found) {
			if (strstr(item->text, "Device (HPET)") != NULL)
				hpet_found = 1;
		} else {
			/* HPET section is found, looking for base */
			val = strstr(item->text, "0x");
			if (val != NULL) {
				unsigned long long address_base;
				idx = index(val, ',');
				if (idx)
					*idx = '\0';

				address_base = strtoul(val, NULL, 0x10);

				if (hpet_base_p != 0) {
					if (hpet_base_p != address_base)
						framework_failed(fw, 
			     				"Mismatched HPET base between %s (%lx) and the kernel (%lx)",
							table,
			     				(unsigned long)hpet_base_p, (unsigned long)address_base);
					else
						framework_passed(fw,
							"HPET base matches that between %s and the kernel (%lx)",
							table,
							(unsigned long)hpet_base_p);
					break;
				}
				/*hpet_base_p = address_base;*/
				hpet_found = 0;
			}
		}
	}
	text_list_free(output);
}


static int hpet_check_init(log *results, framework *fw)
{
	if (check_root_euid(results))
		return 1;

	if ((klog = klog_read()) == NULL) {
		log_error(results, "Cannot read kernel log");
		return 1;
	}
	return 0;
}

static int hpet_check_deinit(log *results, framework *fw)
{
	if (klog)
		text_list_free(klog);

	return 0;
}

static char *hpet_check_headline(void)
{
	return "HPET configuration test";
}

static int hpet_check_test1(log *results, framework *fw)
{
	if (klog == NULL)
		return 1;

	text_list_element *item;

	log_info(results,
		   "This test checks the HPET PCI BAR for each timer block in the timer.\n"
		   "The base address is passed by the firmware via an ACPI table.\n"
		   "IRQ routing and initialization is also verified by the test.");

	for (item = klog->head; item != NULL; item = item->next) {
		if ((strstr(item->text, "ACPI: HPET id:")) != NULL) {
			char *txt = strstr(item->text, "base: ");
			if (txt)
				hpet_base_p = strtoul(txt+6,  NULL, 0x10);
			log_warning(results, "HPET driver in the kernel is enabled, inaccurate results follow");
			framework_passed(fw, "Found HPET base %x in kernel log\n", hpet_base_p);
			break;
		}
	}

	return 0;
}

static int hpet_check_test2(log *results, framework *fw)
{
	int fd;
	unsigned long long hpet_id;
	unsigned long clk_period;

	if ((fd = open("/dev/mem", O_RDONLY)) < 0) {
		log_error(results, "Cannot open /dev/mem");
		return 1;
	}
	hpet_base_v = 
	    mmap(NULL, HPET_REG_SIZE, PROT_READ, MAP_SHARED, fd,
		 hpet_base_p);

	if (hpet_base_v == NULL) {
		log_error(results, "Cannot mmap to /dev/mem");
		return 1;
	}

	hpet_id = *(unsigned long long *) hpet_base_v;

	log_info(results, "HPET found, VendorID is: %04X", ((hpet_id & 0xffff0000) >> 16));

	clk_period = hpet_id >> 32;
	if ((clk_period > MAX_CLK_PERIOD) || (clk_period == 0))
		framework_failed(fw, "Invalid clock period %li, must be non-zero and less than 10^8 fs", clk_period);
	else
		framework_passed(fw, "Valid clock period %li", clk_period);

	return 0;
}

static int hpet_check_test3(log *results, framework *fw)
{
	int i;

	hpet_check_base_acpi_table(results, fw, "DSDT", 0);
	for (i=0;i<11;i++) {
		hpet_check_base_acpi_table(results, fw, "SSDT", i);
	}
	return 0;
}

static framework_tests hpet_check_tests[] = {
	hpet_check_test1,
	hpet_check_test2,
	hpet_check_test3,
	NULL
};

static framework_ops hpet_check_ops = {
	hpet_check_headline,
	hpet_check_init,
	hpet_check_deinit,
	hpet_check_tests
};

FRAMEWORK(hpet_check, &hpet_check_ops, TEST_ANYTIME);
