#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/sizes.h>

extern void puts(const char *s);
extern void puthex(unsigned long long val);

static void write_pattern(u32 p, u32 sram_size)
{
	u32 *addr = (u32 *)KSEG2;
	u32 loop;

	puts("EBI SRAM write test pattern 0x");
	puthex(p);
	puts("...");

	for (loop = 0; loop < sram_size / 4; loop++)
		*addr++ = p;

	puts("finished\n");
}

static void read_pattern(u32 p, u32 sram_size)
{
	u32 *addr = (u32 *)KSEG2;
	u32 loop;
	u32 val;

	puts("EBI SRAM read test pattern 0x");
	puthex(p);
	puts("...");

	for (loop = 0; loop < sram_size / 4; loop++) {
		val = *addr++;
		if (val != p) {
			puts("failed at 0x");
			puthex(loop*4);
			puts(" : 0x");
			puthex(val);
			puts(" != 0x");
			puthex(p);
			puts("\n");
		}
	}
	puts("success\n");
}

void run_ebi_sram_test(u32 sram_size)
{
	u32 *addr;
	u32 loop;
	u32 val;
	u32 count = 0;

	write_pattern(0xFFFFFFFF, sram_size);
	read_pattern(0xFFFFFFFF, sram_size);

	write_pattern(0x0, sram_size);
	read_pattern(0x0, sram_size);

	write_pattern(0xAAAAAAAA, sram_size);
	read_pattern(0xAAAAAAAA, sram_size);

	write_pattern(0x55555555, sram_size);
	read_pattern(0x55555555, sram_size);

	write_pattern(0xCCCCCCCC, sram_size);
	read_pattern(0xCCCCCCCC, sram_size);

	write_pattern(0x40404040, sram_size);
	read_pattern(0x40404040, sram_size);

	write_pattern(0xFFFF0000, sram_size);
	read_pattern(0xFFFF0000, sram_size);

	write_pattern(0x0000FFFF, sram_size);
	read_pattern(0x0000FFFF, sram_size);

	puts("EBI SRAM write test running...");

	count = 0;
	addr = (u32 *)KSEG2;

	for (loop = 0; loop < sram_size / 4; loop++)
		*addr++ = count++;

	puts("finished\n");
	puts("EBI SRAM read test running...");

	count = 0;
	addr = (u32 *)KSEG2;
	for (loop = 0 ; loop < sram_size / 4; loop++) {
		val = *addr++;
		if (val != count) {
			puts("failed at 0x");
			puthex(loop*4);
			puts(" : 0x");
			puthex(val);
			puts(" != 0x");
			puthex(count);
			puts("\n");
		}
		count++;
	}

	puts("success\n");
}
