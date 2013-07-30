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

#include <stdint.h>

#define CONTROL_NODE
#include "per_node.h"
#include "node.h"

unsigned char rx_buf[LOCAL_PACKET_BUF_SIZE];
unsigned char tx_buf[LOCAL_PACKET_BUF_SIZE];

volatile packet_status_t packet_status;

uint8_t checksum(uint8_t len, uint8_t *data) {
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
