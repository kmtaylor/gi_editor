#include <stdio.h>
#include <stdint.h>

int32_t libgieditor_add_addresses(uint32_t address1, uint32_t address2) {
        uint8_t a1, a2, a3, a4, b1, b2, b3, b4, c1, c2, c3, c4;
        a1 = (address1 & 0xff000000) >> 24; b1 = (address2 & 0xff000000) >> 24;
        a2 = (address1 & 0x00ff0000) >> 16; b2 = (address2 & 0x00ff0000) >> 16;
        a3 = (address1 & 0x0000ff00) >> 8;  b3 = (address2 & 0x0000ff00) >> 8;
        a4 = (address1 & 0x000000ff);       b4 = (address2 & 0x000000ff);

        c4 = a4 + b4;
        c3 = a3 + b3 + ((c4 & 0x80)>>7);
        c2 = a2 + b2 + ((c3 & 0x80)>>7);
        c1 = a1 + b1 + ((c2 & 0x80)>>7);

        c4 &= 0x7f;
        c3 &= 0x7f;
        c2 &= 0x7f;
        c1 &= 0x7f;

        return ( c1<<24 | c2<<16 | c3<<8 | c4 );
}

int main(void) {
	int b1, b2, num;
	int address = 0x2000;
	b1 = b2 = num = 0;
	for (num = 1; num <= 256; num++) {
	    b1 = (address & 0xff00) >> 8;
	    b2 = address & 0xff;
#if 0
	    printf("| %02X %02X 00 00 | User Live Set (%04u) |\n",
			    b1, b2, num);
#else
	printf("                  \"User Live Set (%04u)\",               \"Live Set\",\n", num);
#endif
	    address = libgieditor_add_addresses(address, 1);
	}
}

