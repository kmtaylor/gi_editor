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

extern int avr_sysex_init(const char *client_name, enum init_flags flags);
extern int avr_sysex_close(void);
extern void avr_sysex_set_timeout(int timeout_time);
extern void avr_sysex_wait_write(void);

extern void avr_toggle_dec(void);
extern void avr_toggle_inc(void);
extern void avr_toggle_play(void);
extern void avr_toggle_stop(void);
extern void avr_toggle_restart(void);
extern void avr_toggle_rec(void);
extern void avr_toggle_view(void);

extern void avr_delta_measure(int16_t val);
