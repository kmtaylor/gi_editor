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

/* The following routine delays by 3n cycles
	"	ldi r16,n		\n\t"
	"l:	dec r16			\n\t"
	"	brne lb			\n\t"
*/

#include <avr/interrupt.h>
#include <avr/power.h>

#include "per_node.h"
#include "node.h"
#include "bit_bang.h"

/* This is the AVR bit bang transmit routine.
 * It executes 59 instructions between each state change.
 * This equates to 31250 baud at 3.6864MHz */
void send_packet(void) {
	TX_ENABLE;
	asm volatile (
	"	cli			\n\t"
	"	ldi r30,lo8(%[tx_buf])	\n\t"
	"	ldi r31,hi8(%[tx_buf])	\n\t"
	"	mov r12,__zero_reg__	\n\t"
	"tstrt: sbi %[tx_port],%[tx_pin]\n\t" /* (2) */	    /* Send start */
	"	mov r11,__zero_reg__	\n\t" /* (1) */
	"	mov r13,__zero_reg__	\n\t" /* (1) */
	"	inc r13			\n\t" /* (1) */
	"	ld r10,Z+		\n\t" /* (2) */
	"	nop			\n\t" /* (1) */
	"	nop			\n\t" /* (1) */
	"	ldi r16,3		\n\t"
	"1:	dec r16			\n\t" /* (9) */
	"	brne 1b			\n\t"
	"tloop:	ldi r16,12		\n\t"
	"1:	dec r16			\n\t" /* (36) */
	"	brne 1b			\n\t"
	"	nop			\n\t" /* (1) */
	"	mov r16,r13		\n\t" /* (1) */
	"	and r16,r10		\n\t" /* (1) */
	"	brne 1f			\n\t" /* (1/2) */
	"	nop			\n\t" /* (1) */
	"	sbi %[tx_port],%[tx_pin]\n\t" /* (2) */	    /* Send one */
	"	rjmp 2f			\n\t" /* (2) */
	"1:	cbi %[tx_port],%[tx_pin]\n\t" /* (2) */	    /* Send zero */
	"	nop			\n\t" /* (1) */
	"	nop			\n\t" /* (1) */
	"2:	nop			\n\t" /* (1) */
	"	nop			\n\t" /* (1) */
	"	ldi r16,2		\n\t"
	"1:	dec r16			\n\t" /* (6) */
	"	brne 1b			\n\t"
	"	lsl r13			\n\t" /* (1) */
	"	inc r11			\n\t" /* (1) */
	"	ldi r16,8		\n\t" /* (1) */
	"	cp r11,r16		\n\t" /* (1) */
	"	brne tloop	    	\n\t" /* (1/2) */
	"	ldi r16,14		\n\t"
	"1:	dec r16			\n\t" /* (42) */
	"	brne 1b			\n\t"
	"	cbi %[tx_port],%[tx_pin]\n\t" /* (2) */	    /* Send stop */
	"	ldi r16,17		\n\t"
	"1:	dec r16			\n\t" /* (51) */
	"	brne 1b			\n\t"
	"	nop			\n\t" /* (1) */
	"	inc r12			\n\t" /* (1) */
	"	ldi r16,%[local_packet_buf_size] \n\t" /* (1) */
	"	cp r12,r16		\n\t" /* (1) */
	"	brne tstrt  		\n\t" /* (1/2) */
	"	sei"
	: /* No output */
	:   [tx_buf] "p" (tx_buf),
	    [tx_port] "i" (TX_PORT),
	    [tx_pin] "i" (TX_PIN),
	    [local_packet_buf_size] "i" (LOCAL_PACKET_BUF_SIZE)
	:   "r10", /* Byte sent */
	    "r11", /* Bit counter */
	    "r12", /* Byte counter */
	    "r13", /* Send mask */
	    "r16", /* Delay counter */
	    "r30", "r31" /* Z pointer */
	);
	TX_DISABLE;
}

/* This is the AVR bit bang receive routine.
 * Temp register r16 is used to filter out noise, it is incremented
 * when an error is encountered. One error per sample is allowed.
 * The last stop bit of a packet is ignored as we do not have enough cycles
 * to check it. 
 * The interrupt routine does not use the standard epilogue/prologue provided
 * by gcc, this wastes too many cycles. Instead, all registers are reserved
 * as global except the pointer registers r30 and r31 these are saved and
 * restored when appropriate
 * At the moment, it only handles Sysex messages with a fixed length (equal to
 * LOCAL_PACKET_BUF_SIZE 
 * Realtime messages also need to be caught here. */
ISR(RX_INT_VECT, ISR_NAKED) {
	static uint8_t p;
	asm volatile (
	"	in r16,__SREG__		    \n\t" /* (1) */ /* Prologue */
	"	push r16		    \n\t" /* (2) */
	"	ldi r16,0		    \n\t" /* (1) */
	"	sbic %[rx_port],%[rx_pin]   \n\t" /* (1/2/3) */ /* Read start */
	"	inc r16			    \n\t" /* (1) */
	"	sbic %[rx_port],%[rx_pin]   \n\t" /* (1/2/3) */
	"	inc r16			    \n\t" /* (1) */
	"	sbic %[rx_port],%[rx_pin]   \n\t" /* (1/2/3) */
	"	inc r16			    \n\t" /* (1) */
	"	cpi r16,2		    \n\t" /* (1) */
	"	brlo rinit		    \n\t" /* (1/2) */
	"	rjmp rexit		    \n\t" /* (2) */ /* Frame error */
      	"rinit:	clr r10			    \n\t" /* (1) */
      	"	clr r11			    \n\t" /* (1) */
 	"	lds r12,%[p]		    \n\t" /* (2) */
	"	clr r13			    \n\t" /* (1) */
      	"	inc r13			    \n\t" /* (1) */
	"	push r30		    \n\t" /* (2) */
	"	push r31		    \n\t" /* (2) */
      	"	mov r30,r12		    \n\t" /* (1) */
      	"	ldi r31,0		    \n\t" /* (1) */
      	"	subi r30,lo8(-(%[rx_buf]-1))\n\t" /* (1) */ /* Load buffer */
      	"	sbci r31,hi8(-(%[rx_buf]-1))\n\t" /* (1) */
	"	rjmp w6			    \n\t" /* (2) */
	"rloop:	ldi r16,1		    \n\t" /* (1) */
	"	cp r11,r16		    \n\t" /* (1) */
	"	brne 1f			    \n\t" /* (1/2) */
	"	ldi r16,1		    \n\t" /* (1) */
	"	cp r12,r16		    \n\t" /* (1) */
	"	brlo w6			    \n\t" /* (1/2) */
	"	lds r16,(%[rx_buf]+%[local_packet_buf_size]-1)	\n\t" /* (2) */
	"	st Z,r16		    \n\t" /* (2) */
	"	rjmp w1			    \n\t" /* (2) */
	"1:	ldi r16,2		    \n\t" /* (1) */
	"	cp r11,r16		    \n\t" /* (1) */ /* Cleanup */
	"	brne w5			    \n\t" /* (1/2) */
	"	pop r31			    \n\t" /* (2) */
	"	pop r30			    \n\t" /* (2) */
	"	rjmp read		    \n\t" /* (2) */
	"w6:	nop			    \n\t" /* (1) */
	"w5:	nop			    \n\t" /* (1) */
	"	nop			    \n\t" /* (1) */
	"	nop			    \n\t" /* (1) */
	"	nop			    \n\t" /* (1) */
	"w1:	nop			    \n\t" /* (1) */
	"read:	ldi r16,9		    \n\t"
	"1:	dec r16			    \n\t" /* (27) */
	"	brne 1b			    \n\t"
	"	ldi r16,0		    \n\t" /* (1) */ /* Read input */
      	"	sbis %[rx_port],%[rx_pin]   \n\t" /* (1/2/3) */
	"	inc r16			    \n\t" /* (1) */
      	"	sbis %[rx_port],%[rx_pin]   \n\t" /* (1/2/3) */
	"	inc r16			    \n\t" /* (1) */
      	"	sbis %[rx_port],%[rx_pin]   \n\t" /* (1/2/3) */
	"	inc r16			    \n\t" /* (1) */
	"	cpi r16,2		    \n\t" /* (1) */ /* Majority wins */
      	"	brsh 1f			    \n\t" /* (1/2) */
      	"	or r10,r13		    \n\t" /* (1) */ /* Received low */
	"	nop			    \n\t" /* (1) */
	"	rjmp 2f			    \n\t" /* (2) */
      	"1:	mov r16,r13		    \n\t" /* (1) */ /* Received high */
      	"	com r16			    \n\t" /* (1) */
      	"	and r10,r16		    \n\t" /* (1) */
      	"2:	lsl r13			    \n\t" /* (1) */
      	"	inc r11			    \n\t" /* (1) */
	"	ldi r16,8		    \n\t" /* (1) */
      	"	cp r11,r16		    \n\t" /* (1) */
      	"	brne rloop		    \n\t" /* (1/2) */
      	"	sts (%[rx_buf]+%[local_packet_buf_size]-1),r10	\n\t" /* (2) */
 	"	mov r16,r10		    \n\t" /* (2) */ /* Check cur byte */
      	"	cpi r16,%[packet_realtime]  \n\t" /* (1) */
      	"	brsh rrltm		    \n\t" /* (1/2) */
 	"	lds r16,(%[rx_buf]+1)	    \n\t" /* (2) */ /* Check byte 1 */
      	"	cpi r16,%[packet_header]    \n\t" /* (1) */
      	"	breq rexit		    \n\t" /* (1/2) */
	"	ldi r16,(%[local_packet_buf_size]-1)	\n\t" /* (1) */
      	"	cp r12,r16		    \n\t" /* (1) */ /* Check byte n */
      	"	brne rstop		    \n\t" /* (1/2) */
	"	ldi r16,0		    \n\t" /* (1) */
 	"	sts %[p],r16		    \n\t" /* (2) */
 	"	mov r16,r10		    \n\t" /* (2) */
      	"	cpi r16,%[packet_footer]    \n\t" /* (1) */
      	"	brne rexit		    \n\t" /* (1/2) */
      	"	ldi r16,%[packet_in_ready]  \n\t"	    /* Time is not */
 	"	sts %[packet_status],r16    \n\t"	    /* critical here */
	"rintd:	ldi r16,%[rx_int_pin]	    \n\t"
	"	mov r10,r16		    \n\t"
	"	com r10			    \n\t"
	"	lds r16,%[rx_int_msk_add]   \n\t"
	"	and r16,r10		    \n\t"
	"	sts %[rx_int_msk_add],r16   \n\t"	    /* Disable int */
	"	rjmp rexit		    \n\t" /* (2) */
	"rstop:	ldi r16,9		    \n\t"
	"1:	dec r16			    \n\t" /* (27) */
	"	brne 1b			    \n\t"
	"	ldi r16,0		    \n\t" /* (1) */ /* Read stop */
	"	sbis %[rx_port],%[rx_pin]   \n\t" /* (1/2/3) */
	"	inc r16			    \n\t" /* (1) */
	"	sbis %[rx_port],%[rx_pin]   \n\t" /* (1/2/3) */
	"	inc r16			    \n\t" /* (1) */
	"	sbis %[rx_port],%[rx_pin]   \n\t" /* (1/2/3) */
	"	inc r16			    \n\t" /* (1) */
	"	cpi r16,2		    \n\t" /* (1) */
	"	brsh rexit		    \n\t" /* (1/2) */
	"	inc r12			    \n\t" /* (1) */ /* Byte valid */
	"	sts %[p],r12		    \n\t" /* (2) */
	"rexit:	pop r16			    \n\t" /* (2) */ /* Epilogue */
	"	out __SREG__,r16	    \n\t" /* (1) */
        "	reti			    \n\t" /* (4) */
      	"rrltm:	ldi r16,%[packet_rltm_ready]\n\t"	    /* Time is not */
 	"	sts %[packet_status],r16    \n\t"	    /* critical here */
	"	rjmp rintd		    \n\t"
	: /* No output, modifies rx_buffer */
	:   [rx_buf] "p" (rx_buf),
	    [p] "m" (p),
	    [packet_status] "m" (packet_status),
	    [rx_port] "i" (RX_PORT),
	    [rx_pin] "i" (RX_PIN),
	    [local_packet_buf_size] "i" (LOCAL_PACKET_BUF_SIZE),
	    [packet_header] "i" (PACKET_HEADER),
	    [packet_footer] "i" (PACKET_FOOTER),
	    [packet_realtime] "i" (PACKET_REALTIME),
	    [packet_in_ready] "i" (PACKET_IN_READY),
	    [packet_rltm_ready] "i" (PACKET_RLTM_READY),
	    [rx_int_msk_add] "i" (RX_INT_MSK_ADD),
	    [rx_int_pin] "i" (_BV(RX_INT_PIN))
	:   "r10", /* Recieved byte */
	    "r11", /* Bit counter */
	    "r12", /* Byte counter */
	    "r13", /* Receive mask */
	    "r16", /* Delay counter */
	    "r30", "r31" /* Z pointer */
	);
}
