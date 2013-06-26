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

#include <util/crc16.h>

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

char check_rx_packet(void) {
	uint8_t i;
	uint16_t crc = 0xffff;
	for (i = 1; i < (LOCAL_PACKET_BUF_SIZE - 3); i++) {
	    crc = _crc_ccitt_update(crc, rx_buf[i]);
	}
	if ( ((crc >> 8)   == rx_buf[LOCAL_PACKET_BUF_SIZE - 3]) &&
	     ((crc & 0xff) == rx_buf[LOCAL_PACKET_BUF_SIZE - 2]) )
	    if ( (rx_buf[1] == la_hi) && (rx_buf[2] == la_lo) )
			    return 1;
	return 0;
}

void pad_tx_packet(char local_address, unsigned char a_hi, unsigned char a_lo) {
	uint8_t i;
	uint16_t crc = 0xffff;
	tx_buf[0] = PACKET_HEADER;
	tx_buf[1] = local_address ? la_hi : a_hi;
	tx_buf[2] = local_address ? la_lo : a_lo;
	tx_buf[3] = LOCAL_PACKET_BUF_SIZE - 7;
	for (i = 1; i < (LOCAL_PACKET_BUF_SIZE - 3); i++) {
	    crc = _crc_ccitt_update(crc, tx_buf[i]);
	}
	tx_buf[LOCAL_PACKET_BUF_SIZE - 3] = (crc >> 8);
	tx_buf[LOCAL_PACKET_BUF_SIZE - 2] = (crc & 0xff);
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
