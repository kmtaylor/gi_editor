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

/* Packets take the following form:
 *
 * Address  | Description    |  Example
 * --------------------------------------------------------------------------
 *     0 | One byte header:  |  0xff
 *     1 | Two byte address: |  hi8, lo8
 *     3 | One byte size:    |  n
 *     4 | n bytes of data:  |  d1, d2, ...
 * n + 4 | Two byte crc sum: |  hi8, lo8 - calculated from start of address, 
 *	 |		     |		    to end of data
 * n + 6 | One byte footer:  |  0xff
 *
 * Packet is rejected if header or footer is missing, address is wrong,
 * or if crc sum is bad.
 */

#define PACKET_HEADER 0xff
#define PACKET_FOOTER 0xff

typedef enum {
	PACKET_WAITING,
	PACKET_IN_READY
} packet_status_t;

extern volatile packet_status_t packet_status;

extern unsigned char rx_buf[LOCAL_PACKET_BUF_SIZE];
extern unsigned char tx_buf[LOCAL_PACKET_BUF_SIZE];

extern char check_rx_packet(void);
extern void pad_tx_packet(char local_address, unsigned char a_hi,
					unsigned char a_lo);
extern void set_address(unsigned char a_hi, unsigned char a_lo);
extern void init_node(void);
