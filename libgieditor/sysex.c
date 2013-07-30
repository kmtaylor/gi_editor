/* SysEx message handler
 *
 * Juno Gi Editor
 *
 * Copyright (C) 2012 Kim Taylor <kmtaylor@gmx.com>
 * Thanks to Claudio Matsuoka
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
 *
 * $Id: sysex.c,v 1.15 2012/06/28 08:00:42 kmtaylor Exp $
 */

#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include <jack/jack.h>
#include <jack/midiport.h>

#include "libgieditor.h"
#include "midi_jack.h"

#define allocate(t, num) __common_allocate(sizeof(t) * num, "libgieditor")

#define MIDI_CMD_COMMON_SYSEX	    0xf0
#define MIDI_CMD_COMMON_SYSEX_END   0xf7
#define MIDI_CMD_RQ1		    0x11
#define MIDI_CMD_DT1		    0x12
#define MIDI_ROLAND_ID		    0x41
#define MAX_SYSEX_SIZE		    512
#define SYSEX_DATA_OFFSET	    11
#define SYSEX_COMMAND_OFFSET	    6
#define SYSEX_ADDRESS_OFFSET	    7
#define SYSEX_NOT_DATA_BYTES	    13

int sysex_init(const char *client_name, int timeout_time,
                enum init_flags flags) {
	return jack_sysex_init(client_name, timeout_time, flags);
}

int sysex_close(void) {
	return jack_sysex_close();
}

void sysex_set_timeout(int timeout_time) {
	jack_sysex_set_timeout(timeout_time);
}

void sysex_wait_write(void) {
	jack_sysex_wait_write();
}

static int checksum(int len, uint8_t *data) {
	int i, sum;
	
	for (sum = i = 0; i < len; i++) {
		sum = (sum + *data++) & 0x7f;
	}

	if (sum == 0) return 0;
	return 0x80 - sum;
}

int sysex_listen_event(uint8_t *command_id, 
		                uint32_t *sysex_addr, uint8_t **data,
				int *sum) {
	int data_bytes;
	uint8_t *priv_data;

	*data = NULL;

	data_bytes = jack_sysex_listen_event(&priv_data);

	if (data_bytes < 0) return -1;

	*command_id = priv_data[SYSEX_COMMAND_OFFSET];
	*sysex_addr = priv_data[SYSEX_ADDRESS_OFFSET]	<< 24 |
		    priv_data[SYSEX_ADDRESS_OFFSET+1]	<< 16 |
		    priv_data[SYSEX_ADDRESS_OFFSET+2]	<< 8 |
		    priv_data[SYSEX_ADDRESS_OFFSET+3];
	data_bytes = data_bytes - SYSEX_NOT_DATA_BYTES;
	
	*sum = checksum(data_bytes + 5, priv_data + SYSEX_ADDRESS_OFFSET);

	if (data_bytes > 0) {
	    *data = allocate(uint8_t, data_bytes);
	    memcpy(*data, priv_data + SYSEX_DATA_OFFSET, data_bytes);
	} else data_bytes = 0;

	free(priv_data);

	return data_bytes;
}

extern int sysex_send(uint8_t dev_id, uint32_t model_id, uint32_t sysex_addr,
		                uint32_t sysex_size, uint8_t *data) {
	static uint8_t buf[MAX_SYSEX_SIZE + 50];
	int sum, start;
	int i;

	if (sysex_size > MAX_SYSEX_SIZE) return -1;

	i = 0;
	buf[i++] = MIDI_CMD_COMMON_SYSEX;
	buf[i++] = MIDI_ROLAND_ID;
	buf[i++] = dev_id;			/* Device ID */

	buf[i++] = (model_id & 0xff0000) >> 16;	/* Model ID MSB */
	buf[i++] = (model_id & 0x00ff00) >> 8;	/* Model ID*/
	buf[i++] = (model_id & 0x0000ff);	/* Model ID LSB */

	buf[i++] = MIDI_CMD_DT1;		/* Command ID (DT1) */

	start = i;

	buf[i++] = (sysex_addr & 0xff000000) >> 24;
	buf[i++] = (sysex_addr & 0x00ff0000) >> 16;
	buf[i++] = (sysex_addr & 0x0000ff00) >> 8;
	buf[i++] = (sysex_addr & 0x000000ff);
	
	memcpy(buf + i, data, sysex_size);
	i += sysex_size;

	sum = checksum(sysex_size + 4, buf + start);

	buf[i++] = sum;
	buf[i++] = MIDI_CMD_COMMON_SYSEX_END;

	jack_sysex_send_event(i, buf);
	return 0;
}

int sysex_recv(uint8_t dev_id, uint32_t model_id,
		uint32_t sysex_addr, uint32_t sysex_size, uint8_t **data) {
	static uint8_t buf[MAX_SYSEX_SIZE + 50];
	uint8_t cmd_id;
	int sum, start, bytes_received;
	int i;

	*data = NULL;
	if (sysex_size > MAX_SYSEX_SIZE) return -1;

	i = 0;
	buf[i++] = MIDI_CMD_COMMON_SYSEX;
	buf[i++] = MIDI_ROLAND_ID;
	buf[i++] = dev_id;			/* Device ID */

	buf[i++] = (model_id & 0xff0000) >> 16;	/* Model ID MSB */
	buf[i++] = (model_id & 0x00ff00) >> 8;	/* Model ID*/
	buf[i++] = (model_id & 0x0000ff);	/* Model ID LSB */

	buf[i++] = MIDI_CMD_RQ1;		/* Command ID (RQ1) */

	start = i;

	buf[i++] = (sysex_addr & 0xff000000) >> 24;
	buf[i++] = (sysex_addr & 0x00ff0000) >> 16;
	buf[i++] = (sysex_addr & 0x0000ff00) >> 8;
	buf[i++] = (sysex_addr & 0x000000ff);
	
	buf[i++] = (sysex_size & 0xff000000) >> 24;
	buf[i++] = (sysex_size & 0x00ff0000) >> 16;
	buf[i++] = (sysex_size & 0x0000ff00) >> 8;
	buf[i++] = (sysex_size & 0x000000ff);
	
	sum = checksum(i - start, buf + start);

	buf[i++] = sum;
	buf[i++] = MIDI_CMD_COMMON_SYSEX_END;

	jack_sysex_send_event(i, buf);

	bytes_received = sysex_listen_event(&cmd_id, &sysex_addr, data, &sum);

	if (bytes_received < 1)
		return -1;

	if (sum != 0x00)
		return -1;

	return 0;
}
