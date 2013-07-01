/* Bit-banging uart emulator
 *
 * Copyright (C) 2008 Kim Taylor <kmtaylor@gmx.com>
 *
 *	This program is free software; you can redistribute it and/or modify it
 *	under the terms of the GNU General Public License as published by the
 *	Free Software Foundation version 2 of the License.
 * 
 *	This program is distributed in the hope that it will be useful, but
 *	WITHOUT ANY WARRANTY; without even the implied warranty of
 *	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *	General Public License for more details.
 * 
 *	You should have received a copy of the GNU General Public License along
 *	with this program; if not, write to the Free Software Foundation, Inc.,
 *	675 Mass Ave, Cambridge, MA 02139, USA.
 */

/* Timing is critical. These routines are based on the avr core running at
 * 1.8432MHz. At a baud rate of 31250, we have 59 cycles per bit.
 * Thre transmit procedure is as follows:
 *  - Start bit:    59 cycles with tx pin low
 *  - 8 data bits:  transmit data holding 59 cycles per bit
 *  - Stop:	    59 cycles with tx pin high
 */

#define TX_PORT		_SFR_IO_ADDR(PORTD)
#define TX_PIN		PD0
#define RX_PORT		_SFR_IO_ADDR(PIND)
#define RX_PIN		PD2
#define RX_INT_VECT	PCINT2_vect
#define RX_INT_MSK	PCMSK2
#define RX_INT_MSK_ADD	_SFR_MEM_ADDR(RX_INT_MSK)
#define RX_INT_PIN	PCINT18

/* Registers defined to speed up epilogue/prologue times 
 * Warning: These are not saved/restored during interrupt servicing */
register unsigned char byte_val	    asm("r10");
register unsigned char bit_counter  asm("r11");
register unsigned char byte_counter asm("r12");
register unsigned char byte_mask    asm("r13");
register unsigned char temp_reg	    asm("r16"); // This needs to be r16 or r17


static inline void receive_packet(void) {
	packet_status = PACKET_WAITING;
	RX_INT_MSK |= (1<<RX_INT_PIN);
}

static inline void init_uart(void) {
	RX_INT_MSK |= (1<<RX_INT_PIN);
	PCICR |= (1<<PCIE2);
	DDRD |= (1<<DDD1) | (1<<DDD0);
	PORTD &= ~(1<<PD0);
	rx_buf[0] = PACKET_HEADER;
	clock_prescale_set(CLOCK_DIVISOR);
}

/* Interrupts will be disabled when executing the following */
extern void send_packet(void);
