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
	uint8_t byte2;
	uint8_t byte3;
	uint8_t byte4;
};

static void set_arg_16(struct avr_cmd *cmd, uint16_t val) {
	cmd->byte4 = (val >>  0) & 0xf;
	cmd->byte3 = (val >>  4) & 0xf;
	cmd->byte2 = (val >>  8) & 0xf;
	cmd->byte1 = (val >> 12) & 0xf;
}

void avr_toggle_dec(void) {
	struct avr_cmd *cmd = (struct avr_cmd *) (tx_buf + PACKET_DATA_OFFSET);
	cmd->cmd = TOGGLE_BUTTON;
	cmd->byte1 = DEC_BUTTON;
	pad_tx_packet();
	jack_sysex_send_event(AVR_SYSEX_BUF_SIZE, tx_buf);
}

void avr_toggle_inc(void) {
	struct avr_cmd *cmd = (struct avr_cmd *) (tx_buf + PACKET_DATA_OFFSET);
	cmd->cmd = TOGGLE_BUTTON;
	cmd->byte1 = INC_BUTTON;
	pad_tx_packet();
	jack_sysex_send_event(AVR_SYSEX_BUF_SIZE, tx_buf);
}

void avr_toggle_play(void) {
	struct avr_cmd *cmd = (struct avr_cmd *) (tx_buf + PACKET_DATA_OFFSET);
	cmd->cmd = TOGGLE_BUTTON;
	cmd->byte1 = PLAY_BUTTON;
	pad_tx_packet();
	jack_sysex_send_event(AVR_SYSEX_BUF_SIZE, tx_buf);
}

void avr_toggle_stop(void) {
	struct avr_cmd *cmd = (struct avr_cmd *) (tx_buf + PACKET_DATA_OFFSET);
	cmd->cmd = TOGGLE_BUTTON;
	cmd->byte1 = STOP_BUTTON;
	pad_tx_packet();
	jack_sysex_send_event(AVR_SYSEX_BUF_SIZE, tx_buf);
}

void avr_toggle_restart(void) {
	struct avr_cmd *cmd = (struct avr_cmd *) (tx_buf + PACKET_DATA_OFFSET);
	cmd->cmd = TOGGLE_BUTTON;
	cmd->byte1 = RESTART_BUTTON;
	pad_tx_packet();
	jack_sysex_send_event(AVR_SYSEX_BUF_SIZE, tx_buf);
}

void avr_toggle_rec(void) {
	struct avr_cmd *cmd = (struct avr_cmd *) (tx_buf + PACKET_DATA_OFFSET);
	cmd->cmd = TOGGLE_BUTTON;
	cmd->byte1 = RECORD_BUTTON;
	pad_tx_packet();
	jack_sysex_send_event(AVR_SYSEX_BUF_SIZE, tx_buf);
}

void avr_toggle_view(void) {
	struct avr_cmd *cmd = (struct avr_cmd *) (tx_buf + PACKET_DATA_OFFSET);
	cmd->cmd = TOGGLE_BUTTON;
	cmd->byte1 = VIEW_BUTTON;
	pad_tx_packet();
	jack_sysex_send_event(AVR_SYSEX_BUF_SIZE, tx_buf);
}

void avr_req_view(void) {
	struct avr_cmd *cmd = (struct avr_cmd *) (tx_buf + PACKET_DATA_OFFSET);
	cmd->cmd = GET_VIEW;
	pad_tx_packet();
	jack_sysex_send_event(AVR_SYSEX_BUF_SIZE, tx_buf);
}

void avr_delta_measure(int16_t val) {
	struct avr_cmd *cmd = (struct avr_cmd *) (tx_buf + PACKET_DATA_OFFSET);
	cmd->cmd = DELTA_MEASURE;
	set_arg_16(cmd, (uint16_t) val);
	pad_tx_packet();
	jack_sysex_send_event(AVR_SYSEX_BUF_SIZE, tx_buf);
}
