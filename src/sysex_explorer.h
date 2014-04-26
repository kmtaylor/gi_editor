/* Sysex Explorer
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
 * $Id: sysex_explorer.h,v 1.3 2012/06/27 08:53:45 kmtaylor Exp $
 */

extern int dialog_continue(WINDOW *dialog_win, int lines);
extern int dialog_yesno(WINDOW *dialog_win, int lines);
extern int dialog_box(int lines, char **message,
                  int (*type_func)(WINDOW *dialog_win, int line));
extern void report_error(char *msg);
extern void *__interface_allocate(size_t size, char *func_name);

#define ARRAY_SIZE(a) (sizeof(a) / sizeof(a[0]))

#define PATCHDIR "/home/kmtaylor/synth/patches/Patches"
#define STUDIO_DIR PATCHDIR "/Studio"

typedef struct {
	char *name;
	void (*func)(void);
} menu_t;

#define BLANK_LINE(y) \
    {	mvwprintw(menu_sub_win, y, COLS-19, "                ");\
    }

#define PRINT_STRING(value, y) \
    {	mvwprintw(menu_sub_win, y, COLS-19, "%s", value);	\
    }

#define PRINT_BLACK(y) \
    {	wattron(menu_sub_win, A_REVERSE);		\
    	mvwprintw(menu_sub_win, y, COLS-16, "  Blacklisted");   \
	wattroff(menu_sub_win, A_REVERSE);			\
    }

#define PRINT_HEX(value, y) \
    {	wattron(menu_sub_win, A_REVERSE);		\
    	mvwprintw(menu_sub_win, y, COLS-16, "0x%04X", value);  \
	wattroff(menu_sub_win, A_REVERSE);			\
    }

#define PRINT_BASE10(value, y) \
    {	mvwprintw(menu_sub_win, y, COLS-10, " %6i", value);	\
    }

#define PRINT_CHAR(value, y) \
    {	mvwprintw(menu_sub_win, y, COLS-18, "%c", value);	\
    }

#define BLANK_CHAR(y) \
    {	mvwprintw(menu_sub_win, y, COLS-18, "  ");	\
    }

#define PRINT_ON(y) \
    {	wattron(menu_sub_win, A_REVERSE);		\
	mvwprintw(menu_sub_win, y, COLS-12, " ON ");	\
	wattroff(menu_sub_win, A_REVERSE);		\
	mvwprintw(menu_sub_win, y, COLS-8, " OFF ");	\
    }

#define PRINT_OFF(y) \
    {	mvwprintw(menu_sub_win, y, COLS-12, " ON ");	\
	wattron(menu_sub_win, A_REVERSE);		\
	mvwprintw(menu_sub_win, y, COLS-8, " OFF ");	\
	wattroff(menu_sub_win, A_REVERSE);		\
    }

#define PRINT_YES(y, x) \
    {	wattron(dialog_win, A_REVERSE);		    \
	mvwprintw(dialog_win, y, x, " YES ");	    \
	wattroff(dialog_win, A_REVERSE);	    \
	mvwprintw(dialog_win, y, x + 5, " NO ");    \
    }

#define PRINT_NO(y, x) \
    {	mvwprintw(dialog_win, y, x, " YES ");	    \
	wattron(dialog_win, A_REVERSE);		    \
	mvwprintw(dialog_win, y, x + 5, " NO ");    \
	wattroff(dialog_win, A_REVERSE);	    \
    }

#define SYSEX_ERR \
    {	char *packet_message[3];					     \
	packet_message[0] = "Warning:";					     \
	packet_message[1] = "Did not receive correct reply from JunoGi";     \
	packet_message[2] = "Check connections";			     \
	dialog_box(3, packet_message, dialog_continue);			     \
    }
