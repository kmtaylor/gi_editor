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
 * $Id: sysex_explorer.c,v 1.16 2012/07/18 05:42:11 kmtaylor Exp $
 */

#include <stdlib.h>
#include <ctype.h>
#include <stdint.h>
#include <unistd.h>
#include <string.h>
#include <malloc.h>
#include <sys/wait.h>
#include <menu.h>
#include <panel.h>
#include <form.h>

#include "log.h"
#include "libgieditor.h"
#include "midi_addresses.h"
#include "sysex_explorer.h"

#define CLIENT_NAME "sysex_explorer"

#define allocate(type, num, func_name) \
	__interface_allocate(((num) * sizeof(type)), func_name)

static int global_want_quit;

void report_error(char *msg) {
	char *message[2];
	message[0] = "Error:";
	message[1] = msg;
	dialog_box(2, message, dialog_continue);
}

static int natural(int num) {
	if (num < 0) return 0;
	return num;
}

static int maximum(int max, int num) {
	if (num > max) return max;
	return num;
}

/* ----------------------------------------------------------------------------
 * These functions are exported to allow for error dialogs in the parser
 * ------------------------------------------------------------------------- */
int dialog_continue(WINDOW *dialog_win, int lines) {
	mvwprintw(dialog_win, lines + 2, 2, "Press any key to continue...");
	update_panels();
	doupdate();
	while (getch() == ERR);
	return 0;
}

int dialog_yesno(WINDOW *dialog_win, int lines) {
	int retval = 0, c;
	do {
		if (retval) { PRINT_YES(lines + 2, 20); }
		else { PRINT_NO(lines + 2, 20); }
		update_panels();
		doupdate();
		c = getch();
		switch(c) {
			case KEY_LEFT:
				retval = 1;
				break;
			case KEY_RIGHT:
				retval = 0;
				break;
			case 10:
				return retval;
		}
	} while (1);
}		

int dialog_box(int lines, char **message, 
		int (*type_func)(WINDOW *dialog_win, int line)) {
	int i, retval;
	WINDOW *dialog_win;
	PANEL *dialog_panel;
	dialog_win = newwin(lines + 4, 50, LINES/4, natural(COLS/2 - 25));
	dialog_panel = new_panel(dialog_win);
	box(dialog_win, 0, 0);
	for (i = 0; i < lines ; i++ ) {
		mvwprintw(dialog_win, i + 1, 2, message[i]);
	}
	retval = type_func(dialog_win, lines);
	del_panel(dialog_panel);
	delwin(dialog_win);
	return retval;
}

void *__interface_allocate(size_t size, char *func_name) {
	char *message[3];
	void *temp = malloc(size);
	if (temp == NULL) {
		message[0] = "Error:";
		message[1] = "Insufficient memory while executing:";
		message[2] = func_name;
		dialog_box(3, message, dialog_continue);
		exit(1);
	}
	return temp;
}

static void print_in_middle(WINDOW *win, int starty, int startx, int width, 
		char *string) {
        int length;
        float temp;

        if (win == NULL) win = stdscr;
        if (width == 0) width = COLS;

        length = strlen(string);
        temp = (width - length)/ 2;
        startx += (int)temp;
        mvwprintw(win, starty, startx, "%s", string);
}

static void print_to_left(WINDOW *win, int starty, int startx, int width,
		char *string) {
	int length, temp;

	if (win == NULL) win = stdscr;
	if (width == 0) width = COLS;
	
	length = strlen(string);
	temp = width - length;
	startx += temp;
	mvwprintw(win, starty, startx, "%s", string);
}

static void branch(char *const prog, char *const argv[]) {
	pid_t pid;
	def_prog_mode();
	endwin();
	pid = fork();
	switch (pid) {
	    case 0 : 
		if (execv(prog, argv) < 0) {
		    char *message[2];
		    message[0] = "Warning: Could not execute programme:";
		    message[1] = prog;
		    dialog_box(2, message, dialog_continue);
		}
	    case -1 :
		exit(1);
	    default :
		waitpid(pid, NULL, 0);
		reset_prog_mode();
		refresh();
	}
}

static void increment_decrement_value(uint32_t sysex_address, int delta) {
	uint8_t *data;
	midi_address *m_address = libgieditor_match_midi_address(sysex_address);
	uint32_t sysex_size = m_address->sysex_size;
	uint32_t sysex_value = m_address->value;
	sysex_value += delta;
	if (delta) libgieditor_send_sysex_value(sysex_address, 
			sysex_size, sysex_value);
	if (libgieditor_get_sysex(sysex_address, sysex_size, &data) < 0)
	    return;
	m_address->value = libgieditor_get_sysex_value(data, sysex_size);
	free(data);
}

static int get_string(char *message, char **string) {
	WINDOW *set_win, *form_win;
	PANEL *set_panel;
	FORM *set_form;
	FIELD *set_fields[2];
	int nr_fields, i, c, want_quit = 0;
	char set_string[1][MAX_SET_NAME_SIZE + 1];
	char *cp, *cs;
	int retval = 0;

	set_fields[0] = new_field(1, MAX_SET_NAME_SIZE, 0, 10, 0, 0);
	set_fields[1] = NULL;
	nr_fields = ARRAY_SIZE(set_fields) - 1;

	memset(set_string[0], 0, MAX_SET_NAME_SIZE + 1);
	set_field_back(set_fields[0], A_UNDERLINE);
	set_field_buffer(set_fields[0], 0, set_string[0]);

	set_field_type(set_fields[0], TYPE_REGEXP, "[A-Za-z0-9 _]+");
	
	set_win = newwin(8, 40, LINES/4, natural(COLS/2 - 20));
	form_win = derwin(set_win, 4, 38 , 3, 1);
	set_form = new_form(set_fields);
	set_panel = new_panel(set_win);
	set_form_win(set_form, set_win);
	set_form_sub(set_form, form_win);

	curs_set(1);
	box(set_win, 0, 0);
        mvwaddch(set_win, 2, 0, ACS_LTEE);
        mvwhline(set_win, 2, 1, ACS_HLINE, 38);
        mvwaddch(set_win, 2, 39, ACS_RTEE);
	print_in_middle(set_win, 1, 0, 40, message); 
	post_form(set_form);
	mvwprintw(form_win, 0,  1, "Name:");
	mvwprintw(form_win, 3,  1, "Press Enter to confirm, 'q' to exit.");

	do {
		update_panels();
		doupdate();
		c = getch();
		switch(c) {
			case 10:
				want_quit = 2;
				form_driver(set_form, REQ_NEXT_FIELD);
				break;
			case 'q':
				want_quit = 1;
				break;
			default:
				form_driver(set_form, c);
				break;
		}
	} while (!want_quit);
	
	cs = field_buffer(set_fields[0], 0);
	cp = field_buffer(set_fields[0], 0) + MAX_SET_NAME_SIZE - 1;
	while (*cp == ' ') {
	    *cp = '\0';
	    if (cs == cp) break;
	    cp--;
	}

	if (strlen(field_buffer(set_fields[0], 0)) == 0) retval = -1;

	if (want_quit == 2 && retval == 0) {
	    asprintf(string, "%s", field_buffer(set_fields[0], 0));
	}

	if (want_quit == 1) retval = -1;

	for (i=0; i < nr_fields; i++)
		free_field(set_fields[i]);
	free_form(set_form);
	del_panel(set_panel);
	delwin(form_win);
	delwin(set_win);
	curs_set(0);
	return retval;
}

static int set_string_value(uint32_t sysex_address) {
	WINDOW *set_win, *form_win;
	PANEL *set_panel;
	FORM *set_form;
	FIELD *set_fields[3];
	int nr_fields, i, c, want_quit = 0;
	char set_string[2][MAX_SET_NAME_SIZE + 1];
	char old_name[MAX_SET_NAME_SIZE + 1];
	int retval = 0;
	char *message[2];

	/* Sanity check */
	midi_address *m_address = libgieditor_match_midi_address(sysex_address);
	if (!strstr(m_address->class->name, "Set Common") ||
			(m_address->sysex_addr & 0xffff) != 0 ) {
	    message[0] = "Cannot set name:";
	    message[1] = "Invalid location.";
	    dialog_box(2, message, dialog_continue);
	    return -1;
	}

	set_fields[0] = new_field(1, MAX_SET_NAME_SIZE, 0, 10, 0, 0);
	set_fields[1] = new_field(1, 8, 1, 10, 0, 0);
	set_fields[2] = NULL;
	nr_fields = ARRAY_SIZE(set_fields) - 1;

	for (i = 0; i < MAX_SET_NAME_SIZE; i++) {
	    old_name[i] = m_address[i].value & 0x7f;
	}
	
	sprintf(set_string[0], "%s", old_name);
	sprintf(set_string[1], "%08X", sysex_address);
	for (i=0; i < nr_fields; i++) {
		set_field_back(set_fields[i], A_UNDERLINE);
		set_field_buffer(set_fields[i], 0, set_string[i]);
	}

	set_field_type(set_fields[0], TYPE_REGEXP, "[A-Za-z0-9 _]+");
	set_field_type(set_fields[1], TYPE_ALNUM, 1);
	
	set_win = newwin(8, 40, LINES/4, natural(COLS/2 - 20));
	form_win = derwin(set_win, 4, 38 , 3, 1);
	set_form = new_form(set_fields);
	set_panel = new_panel(set_win);
	set_form_win(set_form, set_win);
	set_form_sub(set_form, form_win);

	curs_set(1);
	box(set_win, 0, 0);
        mvwaddch(set_win, 2, 0, ACS_LTEE);
        mvwhline(set_win, 2, 1, ACS_HLINE, 38);
        mvwaddch(set_win, 2, 39, ACS_RTEE);
	print_in_middle(set_win, 1, 0, 40, "Set name:"); 
	post_form(set_form);
	mvwprintw(form_win, 0,  1, "Name:");
	mvwprintw(form_win, 1,  1, "Address:");
	mvwprintw(form_win, 3,  1, "Press Enter to confirm, 'q' to exit.");

	do {
		update_panels();
		doupdate();
		c = getch();
		switch(c) {
			case KEY_UP:
				form_driver(set_form, REQ_PREV_FIELD);
				break;
			case KEY_DOWN:
				form_driver(set_form, REQ_NEXT_FIELD);
				break;
			case 10:
				want_quit = 2;
				form_driver(set_form, REQ_NEXT_FIELD);
				break;
			case 'q':
				want_quit = 1;
				break;
			default:
				form_driver(set_form, c);
				break;
		}
	} while (!want_quit);
	
	if (want_quit == 2) {
	    libgieditor_send_sysex(sysex_address, 
			    strlen(field_buffer(set_fields[0], 0)),
			    (uint8_t *) field_buffer(set_fields[0], 0));
	}

	if (want_quit == 1) retval = -1;

	for (i=0; i < nr_fields; i++)
		free_field(set_fields[i]);
	free_form(set_form);
	del_panel(set_panel);
	delwin(form_win);
	delwin(set_win);
	curs_set(0);
	return retval;
}

static int get_layer_and_part(int *layer, int *part) {
	WINDOW *set_win, *form_win;
	PANEL *set_panel;
	FORM *set_form;
	FIELD *set_fields[3];
	int nr_fields, i, c, want_quit = 0;
	char set_string[2][9];
	int retval = 0;

	set_fields[0] = new_field(1, 1, 0, 10, 0, 0);
	set_fields[1] = new_field(1, 2, 1, 10, 0, 0);
	set_fields[2] = NULL;
	nr_fields = ARRAY_SIZE(set_fields) - 1;
	
	sprintf(set_string[0], "1");
	sprintf(set_string[1], "1");
	for (i=0; i < nr_fields; i++) {
		set_field_back(set_fields[i], A_UNDERLINE);
		set_field_buffer(set_fields[i], 0, set_string[i]);
	}

	set_field_type(set_fields[0], TYPE_INTEGER, 0, 1, 4);
	set_field_type(set_fields[1], TYPE_INTEGER, 0, 1, 16);
	
	set_win = newwin(8, 40, LINES/4, natural(COLS/2 - 20));
	form_win = derwin(set_win, 4, 38 , 3, 1);
	set_form = new_form(set_fields);
	set_panel = new_panel(set_win);
	set_form_win(set_form, set_win);
	set_form_sub(set_form, form_win);

	curs_set(1);
	box(set_win, 0, 0);
        mvwaddch(set_win, 2, 0, ACS_LTEE);
        mvwhline(set_win, 2, 1, ACS_HLINE, 38);
        mvwaddch(set_win, 2, 39, ACS_RTEE);
	print_in_middle(set_win, 1, 0, 40, "Transfer Live set to Studio part:");
	post_form(set_form);
	mvwprintw(form_win, 0,  1, "Layer:");
	mvwprintw(form_win, 1,  1, "Part:");
	mvwprintw(form_win, 3,  1, "Press Enter to confirm, 'q' to exit.");

	do {
		update_panels();
		doupdate();
		c = getch();
		switch(c) {
			case KEY_UP:
				form_driver(set_form, REQ_PREV_FIELD);
				break;
			case KEY_DOWN:
				form_driver(set_form, REQ_NEXT_FIELD);
				break;
			case 10:
				want_quit = 2;
				form_driver(set_form, REQ_NEXT_FIELD);
				break;
			case 'q':
				want_quit = 1;
				break;
			default:
				form_driver(set_form, c);
				break;
		}
	} while (!want_quit);
	
	if (want_quit == 2) {
	    *layer = atoi(field_buffer(set_fields[0], 0));
	    *part = atoi(field_buffer(set_fields[1], 0));
	}

	if (want_quit == 1) retval = -1;

	for (i=0; i < nr_fields; i++)
		free_field(set_fields[i]);
	free_form(set_form);
	del_panel(set_panel);
	delwin(form_win);
	delwin(set_win);
	curs_set(0);
	return retval;
}

static int set_value(uint32_t sysex_address) {
	WINDOW *set_win, *form_win;
	PANEL *set_panel;
	FORM *set_form;
	FIELD *set_fields[3];
	int nr_fields, i, c, want_quit = 0;
	char set_string[2][9];
	int retval = 0;

	midi_address *m_address = libgieditor_match_midi_address(sysex_address);
	uint32_t sysex_size = m_address->sysex_size;
	uint32_t sysex_value = m_address->value;

	set_fields[0] = new_field(1, 5, 0, 10, 0, 0);
	set_fields[1] = new_field(1, 8, 1, 10, 0, 0);
	set_fields[2] = NULL;
	nr_fields = ARRAY_SIZE(set_fields) - 1;
	
	sprintf(set_string[0], "%i", sysex_value);
	sprintf(set_string[1], "%08X", sysex_address);
	for (i=0; i < nr_fields; i++) {
		set_field_back(set_fields[i], A_UNDERLINE);
		set_field_buffer(set_fields[i], 0, set_string[i]);
	}

	set_field_type(set_fields[0], TYPE_INTEGER, 0, 1, 65536);
	set_field_type(set_fields[1], TYPE_ALNUM, 1);
	
	set_win = newwin(8, 40, LINES/4, natural(COLS/2 - 20));
	form_win = derwin(set_win, 4, 38 , 3, 1);
	set_form = new_form(set_fields);
	set_panel = new_panel(set_win);
	set_form_win(set_form, set_win);
	set_form_sub(set_form, form_win);

	curs_set(1);
	box(set_win, 0, 0);
        mvwaddch(set_win, 2, 0, ACS_LTEE);
        mvwhline(set_win, 2, 1, ACS_HLINE, 38);
        mvwaddch(set_win, 2, 39, ACS_RTEE);
	print_in_middle(set_win, 1, 0, 40, "Set Value:"); 
	post_form(set_form);
	mvwprintw(form_win, 0,  1, "Value:");
	mvwprintw(form_win, 1,  1, "Address:");
	mvwprintw(form_win, 3,  1, "Press Enter to confirm, 'q' to exit.");

	do {
		update_panels();
		doupdate();
		c = getch();
		switch(c) {
			case KEY_UP:
				form_driver(set_form, REQ_PREV_FIELD);
				break;
			case KEY_DOWN:
				form_driver(set_form, REQ_NEXT_FIELD);
				break;
			case 10:
				want_quit = 2;
				form_driver(set_form, REQ_NEXT_FIELD);
				break;
			case 'q':
				want_quit = 1;
				break;
			default:
				form_driver(set_form, c);
				break;
		}
	} while (!want_quit);
	
	if (want_quit == 2) {
	    sysex_value = atoi(field_buffer(set_fields[0], 0));
	    libgieditor_send_sysex_value(sysex_address, sysex_size,
			    sysex_value);
	    m_address->value = sysex_value;
	}

	if (want_quit == 1) retval = -1;

	for (i=0; i < nr_fields; i++)
		free_field(set_fields[i]);
	free_form(set_form);
	del_panel(set_panel);
	delwin(form_win);
	delwin(set_win);
	curs_set(0);
	return retval;
}

static int refresh_patch_names_wrapper(WINDOW *dialog_win, int lines) {
	mvwprintw(dialog_win, lines + 2, 2, "Please wait ...");
        update_panels();
        doupdate();
	return libgieditor_refresh_patch_names(); 
}

static void peek_value(uint32_t sysex_addr) {
	char *message[3];
	uint8_t *data;
	uint32_t value;
	uint32_t size = libgieditor_get_sysex_size(sysex_addr);
	int retval = libgieditor_get_sysex(sysex_addr, size, &data);
	if (retval < -1) {
	    message[0] = "Error reading address:";
	    message[1] = "Blacklisted address.";
	    dialog_box(2, message, dialog_continue);
	    return;
	}
	if (retval < 0) {
	    message[0] = "Error reading address:";
	    message[1] = "Timeout error.";
	    dialog_box(2, message, dialog_continue);
	    return;
	}
	value = libgieditor_get_sysex_value(data, size);
	message[0] = "Reading value:";
	asprintf(&message[1], "Address =\t0x%X", sysex_addr);
	asprintf(&message[2], "Value = \t0x%X", value); 
	dialog_box(3, message, dialog_continue);
	free(message[1]);
	free(message[2]);
	free(data);
}

/* Return values:
 * -1 = Don't print
 *  0 = Address blacklisted
 *  1 = Print Hex/Int values
 *  2 = Print string name
 */
static int update_value(uint32_t sysex_base_addr, uint32_t *sysex_value,
		MidiClassMember *m_class, int refresh) {
	uint32_t sysex_addr = sysex_base_addr + m_class->sysex_addr_base;
	uint32_t sysex_size;
	uint8_t *sysex_data;
	midi_address *m_address;

	if (strstr(m_class->name, "User Live Set")) return 2;
	if (m_class->class) return -1;

	m_address = libgieditor_match_midi_address(sysex_addr);

	if (refresh) m_address->flags &= ~M_ADDRESS_FETCHED;

	if (m_address->flags & M_ADDRESS_FETCHED) {
	    *sysex_value = m_address->value;
	    return 1;
	}

	sysex_size = m_address->sysex_size;
	if (libgieditor_get_sysex(sysex_addr, sysex_size, &sysex_data) < 0) {
	    return 0;
	}
	*sysex_value = libgieditor_get_sysex_value(sysex_data, sysex_size);
	m_address->value = *sysex_value;
	m_address->flags |= M_ADDRESS_FETCHED;
	free(sysex_data);
	return 1;
}

static void print_rhc(uint32_t sysex_base_addr, MidiClassMember *tmp_member,
		WINDOW *menu_sub_win, int skip, int i) {
	uint32_t print_sysex_value;
	int retval;
	char *print_string;

	retval = update_value(sysex_base_addr, &print_sysex_value,
				tmp_member, 0);

	if (retval == -1) {
		BLANK_LINE(i - skip);
	}
    
	switch (retval) {
	    case 0:
		PRINT_BLACK(i - skip);
		break;
	    case 1:
		if (isalnum(print_sysex_value & 0xff) &&
		     strstr(tmp_member->name, "Name")) {
		    PRINT_CHAR(print_sysex_value, i - skip);
		} else {
		    BLANK_CHAR(i - skip);
		}
		PRINT_BASE10(print_sysex_value, i - skip); 
		PRINT_HEX(print_sysex_value, i - skip); 
		break;
	    case 2:
		print_string = libgieditor_get_patch_name(sysex_base_addr +
				tmp_member->sysex_addr_base);
		if (print_string) {
		    PRINT_STRING(print_string, i - skip);
		}
	}
}

static void do_paste(MidiClassMember *tmp_member, MidiClass *cur_class,
		uint32_t sysex_base_addr, int *copy_depth) {
	static int layer_to_part;
	int layer, part, retval;
	int tried_layer_to_part = 0;

	if (layer_to_part) {
	    layer_to_part = 0;
	    if (get_layer_and_part(&layer, &part) < 0) return;
	    retval = libgieditor_paste_layer_to_part(cur_class,
		sysex_base_addr + tmp_member->sysex_addr_base,
		copy_depth, layer, part);
	    tried_layer_to_part = 1;
	} else {
	    retval = libgieditor_paste_class(cur_class,
		sysex_base_addr + tmp_member->sysex_addr_base,
		copy_depth);
	}

	if (retval == -4) {
	    char *msg[2];
	    msg[0] = "Error pasting.";
	    msg[1] = "I/O Error.";
	    dialog_box(2, msg, dialog_continue);
	}
	if (retval == -3) {
	    char *msg[2];
	    msg[0] = "Error pasting:";
	    msg[1] = "Data not correctly verified.";
	    dialog_box(2, msg, dialog_continue);
	}
	if (retval == -2) {
	    if (!layer_to_part && !tried_layer_to_part) {
		layer_to_part = 1;
		do_paste(tmp_member, cur_class, sysex_base_addr, copy_depth);
		return;
	    }
	    char *msg[2];
	    msg[0] = "Cannot paste here.";
	    msg[1] = "Class is incompatable.";
	    dialog_box(2, msg, dialog_continue);
	}
	if (retval == -1) {
	    char *msg[1];
	    msg[0] = "Error, nothing to paste";
	    dialog_box(1, msg, dialog_continue);
	}
}

static char *make_long_name(const char *orig_name, int len) {
	int i;
	char *long_name;
	char *func_name = "make_long_name()";

	long_name = allocate(char, len + 1, func_name);

	strncpy(long_name, orig_name, len);

	for (i = strlen(orig_name); i < len; i++) {
	    long_name[i] = ' ';
	}
	long_name[len] = '\0';
	
	return long_name;
}

static void sysex_explorer(char **headers, char *footer, MidiClass *cur_class,
		uint32_t sysex_base_addr) {
	PANEL *explorer_panel;
	MENU *explorer_menu;
	WINDOW *explorer_win, *menu_sub_win;
	ITEM **member_items;
	ITEM *cur;
	static int copy_depth = 0;
	int retval;
	int n_members = cur_class->size;
	int n_parents = libgieditor_class_num_parents(cur_class);
	int max_items = LINES - 5 - (2 * n_parents);
	int menu_width = COLS - 23;
	int c, i, want_break = 0, want_refresh = 1;
	int position = 0, skip = 0, damaged = 1;
	char *func_name = "sysex_explorer()";
	char **new_headers;
	char *filename;
	char *long_filename;
	char **long_names;
	MidiClassMember *tmp_member;
	uint32_t print_sysex_value;
	midi_address *first_member_address;
	
	cbreak(); clear();

	long_names = allocate(char *, n_members, func_name);

	member_items = allocate(ITEM *, n_members + 1, func_name);
	for (i = 0; i < n_members; i++) {
	    long_names[i] = make_long_name(cur_class->members[i].name,
			    menu_width);
	    member_items[i] = new_item(long_names[i], NULL);
	    set_item_userptr(member_items[i], (void *) &cur_class->members[i]);
	}
	member_items[n_members] = NULL;

	explorer_win = newwin(LINES - 1, COLS, 0, 0);
	explorer_panel = new_panel(explorer_win);
	explorer_menu = new_menu(member_items);
	box(explorer_win, 0, 0);
	set_menu_mark(explorer_menu, " * ");
	menu_sub_win = derwin(explorer_win, max_items,
			COLS - 2, 3 + (2 * n_parents), 1);
	set_menu_win(explorer_menu, explorer_win);
	set_menu_sub(explorer_menu, menu_sub_win);
	set_menu_format(explorer_menu, max_items, 1);

	for (i = 0; i < n_parents + 1; i++) {
	    print_in_middle(explorer_win, 1 + (2 * i), 0, COLS, headers[i]); 
	    mvwaddch(explorer_win, 2 + (2 * i), 0, ACS_LTEE);
	    mvwhline(explorer_win, 2 + (2 * i), 1, ACS_HLINE, COLS-2);
	    mvwaddch(explorer_win, 2 + (2 * i), COLS-1, ACS_RTEE);
	}

        mvprintw(LINES - 1, 0, footer);
	post_menu(explorer_menu);

	do {
		update_panels();
		doupdate();
		if (damaged) {
		    if (want_refresh) {
			if (!cur_class->members[0].class) {
			    first_member_address = 
				libgieditor_match_midi_address(sysex_base_addr);
			    libgieditor_get_bulk_sysex(first_member_address,
					    n_members);
			}
			want_refresh = 0;
		    }
		    for (i = skip; i < n_members; i++) {
			tmp_member = item_userptr(member_items[i]);
			print_rhc(sysex_base_addr, tmp_member, menu_sub_win,
					skip, i);
		    }
		    mvprintw(LINES - 1, COLS - 9, "Copies %i", copy_depth);
		    damaged = 0;
		    wrefresh(menu_sub_win);
		}
		c = getch();
		switch(c) {
		    case KEY_DOWN:
			if ( position == max_items - 1 ) { 
			    if (skip < (n_members - max_items)) {
				skip++;
				damaged = 1;
			    }
			} else position++;
			menu_driver(explorer_menu, REQ_DOWN_ITEM);
			break;
                    case 'd':
                        if (skip + (max_items * 2) > n_members) {
                            skip = n_members - max_items;
                        } else  skip += max_items;
                        damaged = 1;
                        menu_driver(explorer_menu, REQ_SCR_DPAGE);
                        break;
		    case KEY_UP:
			if (position == 0) {
			    if (skip > 0) {
				skip--;
				damaged = 1;
			    }
			} else position--;
			menu_driver(explorer_menu, REQ_UP_ITEM);
			break;
                    case 'u':
                        if ((skip - max_items) > 0) {
                            skip -= max_items;
                        } else skip = 0;
                        damaged = 1;
                        menu_driver(explorer_menu, REQ_SCR_UPAGE);
                        break;
		    case KEY_RIGHT:
			cur = current_item(explorer_menu);
			tmp_member = item_userptr(cur);
			if (tmp_member->class) break;
			increment_decrement_value(sysex_base_addr +
					tmp_member->sysex_addr_base, 1);
			damaged = 1;
			break;
		    case KEY_LEFT:
			cur = current_item(explorer_menu);
			tmp_member = item_userptr(cur);
			if (tmp_member->class) break;
			increment_decrement_value(sysex_base_addr +
					tmp_member->sysex_addr_base, -1);
			damaged = 1;
			break;
		    case 10:
			cur = current_item(explorer_menu);
			tmp_member = item_userptr(cur);
			if (!tmp_member->class) {
			    peek_value(sysex_base_addr +
					    tmp_member->sysex_addr_base);
			    break;
			}
			new_headers = 
				allocate(char *, n_parents + 2, func_name);
			for (i = 0; i < n_parents + 1; i++)
				new_headers[i] = headers[i];
			new_headers[n_parents + 1] = (char *) tmp_member->name;
			sysex_explorer(new_headers, 
				    footer, tmp_member->class,
				    sysex_base_addr +
				    tmp_member->sysex_addr_base);
			free(new_headers);
			break;
		    case 's':
			cur = current_item(explorer_menu);
			tmp_member = item_userptr(cur);
			if (tmp_member->class) break;
			set_value(sysex_base_addr +
					tmp_member->sysex_addr_base);
			update_value(sysex_base_addr, &print_sysex_value,
					                    tmp_member, 1);
			damaged = 1;
			break;
		    case 'n':
			if (sysex_base_addr == 0) {
			    char *msg[1];
			    msg[0] = "Refreshing patch names";
			    dialog_box(1, msg, refresh_patch_names_wrapper);
			    damaged = 1;
			    break;
			}
			cur = current_item(explorer_menu);
			tmp_member = item_userptr(cur);
			if (tmp_member->class) break;
			set_string_value(sysex_base_addr + 
					tmp_member->sysex_addr_base);
			want_refresh = 1;
			damaged = 1;
			break;
		    case 'a':
			want_refresh = 1;
			damaged = 1;
			break;
		    case 'f':
			cur = current_item(explorer_menu);
			tmp_member = item_userptr(cur);
			increment_decrement_value(sysex_base_addr +
					tmp_member->sysex_addr_base, 0);
			damaged = 1;
			break;
		    case 'c':
			cur = current_item(explorer_menu);
			tmp_member = item_userptr(cur);
			if (libgieditor_copy_class(cur_class,
				sysex_base_addr + tmp_member->sysex_addr_base,
				&copy_depth)) {
			    char *msg[1];
			    msg[0] = "Error retrieving data";
			    dialog_box(1, msg, dialog_continue);
			}
			damaged = 1;
			break;
		    case 'p':
			cur = current_item(explorer_menu);
			tmp_member = item_userptr(cur);
			do_paste(tmp_member, cur_class,
					sysex_base_addr, &copy_depth);
			damaged = 1;
			break;
		    case 'l':
			libgieditor_flush_copy_data(&copy_depth);
			damaged = 1;
			break;
		    case 'w':
			cur = current_item(explorer_menu);
			tmp_member = item_userptr(cur);
			retval = get_string("Filename:", &filename);
			if (retval < 0) break;
			asprintf(&long_filename, "%s/%s", PATCHDIR, filename);
			retval = libgieditor_write_copy_data_to_file(
					    long_filename, &copy_depth);
			free(filename);
			free(long_filename);
			if (retval == -2) {
			    char *msg[1];
                            msg[0] = "Error, could not write file";
                            dialog_box(1, msg, dialog_continue);
			}
			if (retval == -1) {
			    char *msg[1];
                            msg[0] = "Error, nothing to paste";
                            dialog_box(1, msg, dialog_continue);
			}
			damaged = 1;
			break;
		    case 'r':
			cur = current_item(explorer_menu);
			tmp_member = item_userptr(cur);
			retval = get_string("Filename:", &filename);
			if (retval < 0) break;
			asprintf(&long_filename, "%s/%s", PATCHDIR, filename);
			retval = libgieditor_read_copy_data_from_file(
					    long_filename, &copy_depth);
			free(filename);
			free(long_filename);
			if (retval == -2) {
			    char *msg[1];
                            msg[0] = "Error, could not parse file";
                            dialog_box(1, msg, dialog_continue);
			}
			if (retval == -1) {
			    char *msg[1];
                            msg[0] = "Error, could not find file";
                            dialog_box(1, msg, dialog_continue);
			}
			damaged = 1;
			break;
		    case 'q':
			want_break = 1;
			break;
		}
	} while( !want_break );
	
	for (i = 0; i < n_members; i++)
	    free_item(member_items[i]);
	unpost_menu(explorer_menu);
	free_menu(explorer_menu);
	free(member_items);
	for (i = 0; i < n_members; i++)
	    free(long_names[i]);
	free(long_names);
	del_panel(explorer_panel);
	delwin(menu_sub_win);
	delwin(explorer_win);
	clear(); halfdelay(10);
}

static void sysex_explorer_menu_wrapper(void) {
        char *headers[] = { "Sysex Explorer" };
	char *footer =  
	"(S)et value, Re(f)resh value, Refresh (A)ll, (N)ame, (Q)uit, "
	"(C)opy, (P)aste, C(l)ear, (W)rite, (R)ead.";
	sysex_explorer(headers, footer, &libgieditor_top_midi_class, 0);
}

static void view_log(void) {
	char *const argv[] = { "less", "-c", LOGFILE, NULL };
	branch("/usr/bin/less", argv);
}

static void clear_log(void) {
	char *message[4];
	message[0] = "Warning: Clearing the log.";
	message[1] = "         Previous entries will be lost";
	message[2] = "";
	message[3] = "Are you sure you want to do this?";
	if (dialog_box(4, message, dialog_yesno)) {
		unlink(LOGFILE);
	}
}

static void logout(void) {
	global_want_quit = 1;
}

static menu_t main_menu_list[] = {
	{ "Sysex Explorer",		sysex_explorer_menu_wrapper },
	{ "View log",			view_log },
	{ "Clear log",			clear_log },
	{ "Exit",			logout },
};

int main(void) {
        ITEM **main_menu_items;
	ITEM *cur;
	void (*p)(void);
        MENU *main_menu;
	PANEL *main_panel;
        WINDOW *main_win, *menu_sub_win;
        int n_mm_choices, i, c, mm_lines;

        /* Initialize curses */
        initscr();
        noecho();
        keypad(stdscr, TRUE);
        n_mm_choices = ARRAY_SIZE(main_menu_list);
	halfdelay(10);
	curs_set(0);
	
	/* Create the window to be associated with the menu */
	mm_lines = maximum(LINES - 1, n_mm_choices + 4);
        main_win = newwin(mm_lines, 40,
			natural(LINES/2 - (n_mm_choices/2 + 3)),
			natural(COLS/2 - 20));
	main_panel = new_panel(main_win);

        /* Create items */
	main_menu_items = allocate(ITEM *, n_mm_choices + 1, "main()");
        for(i = 0; i < n_mm_choices; i++) {
                main_menu_items[i] = new_item(main_menu_list[i].name, NULL);
		set_item_userptr(main_menu_items[i], main_menu_list[i].func);
	}
	main_menu_items[n_mm_choices] = NULL;

        /* Create menu */
        main_menu = new_menu(main_menu_items);
        set_menu_mark(main_menu, " * ");
        set_menu_win(main_menu, main_win);
	set_menu_format(main_menu, mm_lines - 4, 1);
	menu_sub_win = derwin(main_win, mm_lines - 4, 38, 3, 1);
        set_menu_sub(main_menu, menu_sub_win);
	menu_opts_off(main_menu, O_NONCYCLIC);

        /* Print a border around the main window and print a title */
        box(main_win, 0, 0);
        print_in_middle(main_win, 1, 0, 40, "Main Menu");
        mvwaddch(main_win, 2, 0, ACS_LTEE);
        mvwhline(main_win, 2, 1, ACS_HLINE, 38);
        mvwaddch(main_win, 2, 39, ACS_RTEE);
        
	if (libgieditor_init(CLIENT_NAME,
				LIBGIEDITOR_READ | LIBGIEDITOR_WRITE) < 0) {
	    char *message[2];
	    message[0] = "Could not connect to Jack server.";
	    message[1] = "Please check that jackd is running.";
	    dialog_box(2, message, dialog_continue);
	    global_want_quit = 1;
        }	
	
	/* Post the menu */
        post_menu(main_menu);

        do {
		update_panels();
		doupdate();
		c = getch();
		switch(c) {
			case KEY_DOWN:
			case 'd':
                                menu_driver(main_menu, REQ_DOWN_ITEM);
                                break;
                        case KEY_UP:
			case 'u':
                                menu_driver(main_menu, REQ_UP_ITEM);
                                break;
			case 10:
				cur = current_item(main_menu);
				p = item_userptr(cur);
				if (p) p();
				break;
                }
        } while ( !global_want_quit );

	libgieditor_close();
        /* Unpost and free all the memory taken up */
        unpost_menu(main_menu);
        free_menu(main_menu);
        clear();
	refresh();
        for(i = 0; i < n_mm_choices; ++i)
                free_item(main_menu_items[i]);
	free(main_menu_items);
	del_panel(main_panel);
	delwin(menu_sub_win);
	delwin(main_win);
	endwin();
	return 0;
}
