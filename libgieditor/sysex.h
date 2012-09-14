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
 * $Id: sysex.h,v 1.7 2012/06/28 05:11:32 kmtaylor Exp $
 */

extern int sysex_init(const char *client_name, int timeout_time,
		enum init_flags flags);

extern int sysex_close(void);

extern void sysex_set_timeout(int timeout_time);

extern void sysex_wait_write(void);

extern int sysex_send(uint8_t dev_id, uint32_t model_id, uint32_t sysex_addr,
		uint32_t sysex_size, uint8_t *data);
extern int sysex_recv(uint8_t dev_id, uint32_t model_id, uint32_t sysex_addr,
		uint32_t sysex_size, uint8_t **data);

extern int sysex_listen_event(uint8_t *command_id,
		uint32_t *sysex_addr, uint8_t **data, int *sum);
