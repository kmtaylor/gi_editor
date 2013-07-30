/* SysEx message handler
 *
 * Juno Gi Editor
 *
 * Copyright (C) 2012 Kim Taylor <kmtaylor@gmx.com>
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
 */

extern int jack_sysex_init(const char *client_name, int timeout_time,
		enum init_flags flags);

extern int jack_sysex_close(void);

extern void jack_sysex_set_timeout(int timeout_time);

extern void jack_sysex_wait_write(void);
extern void jack_flush_sysex_in_list(void);
extern int jack_sysex_listen_event();
extern void jack_sysex_send_event(uint32_t sysex_size, uint8_t *data);
