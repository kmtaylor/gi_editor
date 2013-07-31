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

#include <avr/interrupt.h>
#include <avr/power.h>

#include "per_node.h"
#include "node.h"
#include "bit_bang.h"

#define OUTPUT_CHANNELS 7
#define DINPUT_CHANNELS 3

static volatile char view_set;

static inline void setup_ports(void) {
	/* Port C is unused */
	DDRC = 0x00;
	PORTC = 0x00;
	/* PD3 is DTR shutdown, PD0/2 are UART(TX/RX), PD1 is uart enable */
	DDRD = 0xF3;
	PORTD = 0x08;
	/* PB0-2 outputs, PB3-5 DI, PB6/7 XTAL */
	DDRB = 0x07;
	PORTB = 0x38;
	/* Interrupt on PB3 for debounce */
	PCICR |= _BV(PCIE0);
}

static inline void setup_power(void) {
	PRR = 0xEF;	// Everything is off
	ACSR = 0x80;	// Turn off analog comparator
	DIDR0 = 0x3F;	// Turn off PORTC inputs
	DIDR1 = 0x03;	// PD6 and PD7 are output only
}

static volatile uint8_t *output_port[OUTPUT_CHANNELS] = {
	&PORTD, &PORTD, &PORTD, &PORTD,
	&PORTB, &PORTB, &PORTB
};

static const uint8_t output_address[OUTPUT_CHANNELS] = {
	PD4, PD5, PD6, PD7, PB0, PB1, PB2
};

//static volatile uint8_t *dinput_port[DINPUT_CHANNELS] = {
//	&PINB, &PINB, &PINB
//};

//static const uint8_t dinput_address[DINPUT_CHANNELS] = {
//	PB3, PB4, PB5
//};

struct __attribute__((packed)) control_change {
	uint8_t change_cmd;
	uint8_t channel;
	uint8_t val;
};

ISR(PCINT0_vect) {
	view_set = 1;
}

static void transmit_view(void) {
	struct control_change *cmd = (struct control_change *) tx_buf;
	cmd->change_cmd = MIDI_CONTROL_CHANNEL;
	cmd->channel = 0;

	PCMSK0 |=  _BV(PCINT3);
	busy_wait();
	PCMSK0 &= ~_BV(PCINT3);

	if (view_set) {
		cmd->val = 0x7F;
	} else {
		cmd->val = 0x00;
	}
	send_packet();
	view_set = 0;
}

static void toggle_output(uint8_t channel, uint8_t wait) {
	*output_port[channel] |=  _BV(output_address[channel]);
	busy_wait();
	*output_port[channel] &= ~_BV(output_address[channel]);
	if (wait) busy_wait();
}

static void delta_measure(uint8_t *val_bytes) {
	int16_t val = 0, i;
	val |= val_bytes[3] << 0;
	val |= val_bytes[2] << 4;
	val |= val_bytes[1] << 8;
	val |= val_bytes[0] << 12;

	if (val == 0) return;

	PCMSK0 |=  _BV(PCINT3);
        busy_wait();
        PCMSK0 &= ~_BV(PCINT3);

	if (!view_set) toggle_output(VIEW_BUTTON, 0);

	if (val > 0) {
	    for (i = 0; i < val; i++) toggle_output(INC_BUTTON, 1);
	} else {
	    val = -val;
	    for (i = 0; i < val; i++) toggle_output(DEC_BUTTON, 1);
	}

	if (!view_set) toggle_output(VIEW_BUTTON, 0);
	view_set = 0;
}

static void process_command(uint8_t *command) {
	switch (command[0]) {
	    case TOGGLE_BUTTON :
		if (command[1] >= OUTPUT_CHANNELS) break;
		toggle_output(command[1], 0);
		break;
	    case DELTA_MEASURE :
		delta_measure(&command[1]);
		break;
	    case GET_VIEW :
		transmit_view();
		break;
	}
}

static void process_realtime(uint8_t val) {
	if (val == MIDI_RLTM_CONT) toggle_output(PLAY_BUTTON, 0);
	if (val == MIDI_RLTM_STOP) toggle_output(STOP_BUTTON, 0);
}

int main (void) {
	setup_ports();
	setup_power();
	init_uart();
	sei();
	while (1) {
	    if (packet_status == PACKET_IN_READY) {
		if (check_rx_packet())
			process_command(&rx_buf[PACKET_DATA_OFFSET]);

		receive_packet();
	    }
	    if (packet_status == PACKET_RLTM_READY) {
		process_realtime(rx_buf[AVR_SYSEX_BUF_SIZE - 1]);

		receive_packet();
	    }
	}
}
