/* AVR interface
 *
 * Juno Gi Editor
 *
 * Copyright (C) 2012 Kim Taylor <kmtaylor@gmx.com>
 *
 *      This program is free software; you can redistribute it and/or modify it
 *      under the terms of the GNU General Public License as published by the
 *      Free Software Foundation version 2 of the License.
 * 
 *      This program is distributed in the hope that it will be useful, but
 *      WITHOUT ANY WARRANTY; without even the implied warranty of
 *      MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *      General Public License for more details.
 * 
 *      You should have received a copy of the GNU General Public License along
 *      with this program; if not, write to the Free Software Foundation, Inc.,
 *      675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */

#include <stdint.h>
#include <stdio.h>

#include "libgieditor.h"
#include "midi_jack.h"
#define CONTROL_NODE
#include "../avr/per_node.h"
#include "../avr/node.h"

int avr_api_init(const char *client_name, enum init_flags flags) {
	jack_sysex_init(client_name, TIMEOUT_TIME, flags);
}

int avr_api_close(void) {
	jack_sysex_close();
}

void avr_api_set_timeout(int timeout) {
	jack_sysex_set_timeout(timeout);
}

void avr_api_wait_write(void) {
	jack_sysex_wait_write();
}

struct __attribute__((packed)) avr_cmd {
	uint8_t cmd;
	uint8_t byte1;
};

void avr_toggle_back(void) {
	struct avr_cmd *cmd = (struct avr_cmd *) tx_buf + PACKET_DATA_OFFSET;
	cmd->cmd = TOGGLE_BUTTON;
	cmd->byte1 = DEC_BUTTON;
	pad_tx_packet();
	jack_sysex_send_event(LOCAL_PACKET_BUF_SIZE, tx_buf);
}
