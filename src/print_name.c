#include <stdio.h>
#include <stdint.h>
#include "libgieditor.h"

int main(int argc, char **argv) {
	uint32_t sysex_addr;
	if (argc != 2) return 1;

	sscanf(argv[1], "0x%x", &sysex_addr);

	const char *desc = libgieditor_get_desc(sysex_addr + 402653184);
	if (!desc) {
	    printf("Address not found\n");
	    return 1;
	}

	printf("%s\n", desc);
	return 0;
}
