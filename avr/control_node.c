/* Controller node.
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

#include <avr/io.h>
#include <avr/interrupt.h>

#ifndef EEPROM_BROKEN
#include <avr/eeprom.h>
#endif

#include <avr/sleep.h>
#include <avr/power.h>

#include "per_node.h"
#include "node.h"
#include "bit_bang.h"

#define OUTPUT_CHANNELS 8
#define AINPUT_CHANNELS 7
#define DINPUT_CHANNELS 2
#define WAKE_VOLTS 155

#ifndef EEPROM_BROKEN
static unsigned char w_volts __attribute__((section(".eeprom"))) = WAKE_VOLTS;
#endif
static unsigned char wake_volts;

/* Interrupt once per second */
ISR(TIMER1_OVF_vect) {
//	send_packet();
//	PORTB ^= (1<<PB2);
}

static inline void setup_ports(void) {
	/* Port C is unused */
	DDRC = 0x00;
	PORTC = 0x00;
	/* PD3 is DTR shutdown, PD0/2 are UART(TX/RX), PD1 is uart enable */
	DDRD = 0xF3;
	PORTD = 0x00;
	/* PB0-2 outputs, PB3/4 DI, PB5 shutdown PB6/7 XTAL */
	DDRB = 0x27;
	PORTB = 0x38;
}

static inline void setup_power(void) {
	PRR = 0xEF;	// Everything is off
	ACSR = 0x80;	// Turn off analog comparator
	DIDR0 = 0x3F;	// Turn off PORTC inputs
	DIDR1 = 0x03;	// PD6 and PD7 are output only
}

static inline void setup_adc(void) {
	ADMUX = 0x60;	/* Just use 8 bit accuracy */
	ADCSRA = ADC_DIVISOR;
	ADCSRB = 0x00;	/* Auto trigger not used */
#ifdef EEPROM_BROKEN
	wake_volts = WAKE_VOLTS;
#else
	wake_volts = eeprom_read_byte(&w_volts);
#endif
}

static inline void start_timer1(void) {
	/* Enable timer 1 */
	PRR &= ~(1<<PRTIM1);
	TCCR1A = 0;	// Normal mode
	TCCR1B = 0x03;	// Prescaler 64 (1s)
	TIMSK1 = 0x01;	// Enable overflow interrupt
}

//static volatile uint8_t *output_port[OUTPUT_CHANNELS] = {
//	&PORTD, &PORTD, &PORTD, &PORTD,
//	&PORTB, &PORTB, &PORTB, &PORTC
//};

//static const uint8_t output_address[OUTPUT_CHANNELS] = {
//	PD4, PD5, PD6, PD7, PB0, PB1, PB2, PC0
//};

//static const uint8_t ainput_address[AINPUT_CHANNELS] = {
//	0x06, 0x05, 0x04, 0x03, 0x02, 0x01, 0x07
//};

//static volatile uint8_t *dinput_port[DINPUT_CHANNELS] = {
//	&PINB, &PINB
//};

//static const uint8_t dinput_address[DINPUT_CHANNELS] = {
//	PB3, PB4
//};

int main (void) {
	setup_ports();
	setup_adc();
	setup_power();
	init_node();
	init_uart();
	start_timer1();
	sei();
	while (1) {
	    if (packet_status == PACKET_IN_READY) {
		PORTB ^= (1<<PB2);
		memcpy(tx_buf, rx_buf, LOCAL_PACKET_BUF_SIZE);
		send_packet();

		receive_packet();
	    }
	    if (packet_status == PACKET_RLTM_READY) {
		PORTB ^= (1<<PB2);

		receive_packet();
	    }
	    //sleep_mode();
	}
}
