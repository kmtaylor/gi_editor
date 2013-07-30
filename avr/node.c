/* Common functions for uC nodes
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

/* We are going to need timers (3 available) for:
 * Packet timeouts,
 */

#ifndef EEPROM_BROKEN
#include <avr/eeprom.h>
#endif

#include "per_node.h"
#include "node.h"
#if BIT_BANG
#include <avr/power.h>
#include "bit_bang.h"
#endif

unsigned char rx_buf[LOCAL_PACKET_BUF_SIZE];
unsigned char tx_buf[LOCAL_PACKET_BUF_SIZE];

volatile packet_status_t packet_status;

static unsigned char la_hi;
static unsigned char la_lo;

#ifndef EEPROM_BROKEN
static unsigned char addr_hi __attribute__((section(".eeprom"))) = ADDRESS_HIGH;
static unsigned char addr_lo __attribute__((section(".eeprom"))) = ADDRESS_LOW;
#endif

static uint8_t checksum(uint8_t len, uint8_t *data) {
        uint8_t i, sum;

        for (sum = i = 0; i < len; i++) {
                sum = (sum + *data++) & 0x7f;
        }

        if (sum == 0) return 0;
        return 0x80 - sum;
}


char check_rx_packet(void) {
	uint8_t sum;
	if (!(	rx_buf[1] == MIDI_MANU_ID &&
		rx_buf[2] == MIDI_DEV_ID &&
		rx_buf[3] == MIDI_NODE_ID ))
	    return 0;
	sum = checksum(LOCAL_PACKET_BUF_SIZE - 2, &rx_buf[1]);
	if (sum == 0x00) return 1;
	return 0;
}

void pad_tx_packet(void) {
	uint8_t sum;
	tx_buf[0] = PACKET_HEADER;
	tx_buf[1] = MIDI_MANU_ID;
	tx_buf[2] = MIDI_DEV_ID;
	tx_buf[3] = MIDI_NODE_ID;
	sum = checksum(LOCAL_PACKET_BUF_SIZE - 3, &tx_buf[1]);
	tx_buf[LOCAL_PACKET_BUF_SIZE - 2] = sum;
	tx_buf[LOCAL_PACKET_BUF_SIZE - 1] = PACKET_FOOTER;
}

/* This needs to be called with interrupts disabled */
void set_address(unsigned char a_hi, unsigned char a_lo) {
	la_hi = a_hi;
	la_lo = a_lo;
#ifndef EEPROM_BROKEN
	eeprom_write_byte(&addr_hi, la_hi);
	eeprom_write_byte(&addr_lo, la_lo);
#endif
}

void init_node(void) {
#ifdef EEPROM_BROKEN
	la_hi = ADDRESS_HIGH;
	la_lo = ADDRESS_LOW;
#else
	la_hi = eeprom_read_byte(&addr_hi);
	la_lo = eeprom_read_byte(&addr_lo);
#endif
}
