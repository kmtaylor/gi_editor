/* This file contains information specific to a particular node and may
 * be edited for each build
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

/* A note on the AVR fuse bits. All nodes so far have been set to 0xDD. This
 * turns on the BOD, at 7.3728MHz */

/* FIXME: I don't know why this is happening. Perhaps it has
 * something to do with coming out of sleep mode. Or cosmic rays */
#define EEPROM_BROKEN

#ifdef CONTROL_NODE
#define LOCAL_PACKET_BUF_SIZE	3
#define ADDRESS_HIGH		0x01
#define ADDRESS_LOW		0x01
#define ADC_DIVISOR		0x06 // Divide by 64
#define CLOCK_DIVISOR		clock_div_4
/* Node specific EIA-485 control signals */
#define TX_ENABLE		PORTD |= (1<<PD1)
#define TX_DISABLE		PORTD &= ~(1<<PD1)
#define BIT_BANG		1
#endif

/* Put this here for want of a better place */
static inline uint8_t take_sample(const uint8_t channel) {
	uint8_t val;
	PRR &= ~(1<<PRADC); ADCSRA |= (1<<ADEN);
	ADMUX &= 0xf0;
	ADMUX |= channel;
	ADCSRA |= (1<<ADSC);
	while (ADCSRA & (1<<ADSC)) {} // Warm up ADC
	ADCSRA |= (1<<ADSC);
	while (ADCSRA & (1<<ADSC)) {} // Take actual sample
	val = ADCH;
	ADCSRA &= ~(1<<ADEN); PRR |= (1<<PRADC);
	return val;
}
