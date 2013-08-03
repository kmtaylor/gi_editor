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

#define PACKET_HEADER 0xf0
#define PACKET_FOOTER 0xf7
#define PACKET_REALTIME 0xf8
#define PACKET_NOT_SYSEX 0x80
#define PACKET_DATA_OFFSET 4
#define PACKET_RLTM_OFFSET AVR_SYSEX_BUF_SIZE - 1

static inline void busy_wait(void) {
	volatile int i = 10000;
	while (i) i--;
}

typedef enum {
	PACKET_WAITING,
	PACKET_IN_READY,
	PACKET_RLTM_READY
} packet_status_t;

extern volatile packet_status_t packet_status;

extern unsigned char rx_buf[AVR_SYSEX_BUF_SIZE];
extern unsigned char tx_buf[AVR_SYSEX_BUF_SIZE];

static inline uint8_t realtime_byte(void) {
	return rx_buf[PACKET_RLTM_OFFSET];
}

static inline uint8_t check_rltm(void) {
	return (realtime_byte() >= PACKET_REALTIME);
}

extern char check_rx_packet(void);
extern void pad_tx_packet(void);
extern uint8_t checksum(uint8_t len, uint8_t *data);
