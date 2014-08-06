/* Studio Explorer
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
 * $Id: studio_explorer.c,v 1.16 2012/07/18 05:42:11 kmtaylor Exp $
 */

#include <stdlib.h>
#include <ctype.h>
#include <stdint.h>
#include <unistd.h>
#include <string.h>
#include <malloc.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <dirent.h>
#include <menu.h>
#include <panel.h>
#include <form.h>

#include "log.h"
#include "libgieditor.h"
#include "midi_addresses.h"
#include "sysex_explorer.h"

#define CLIENT_NAME "studio_explorer"

#define allocate(type, num, func_name) \
	__interface_allocate(((num) * sizeof(type)), func_name)

typedef struct s_name_list name_list_t;
struct s_name_list {
	struct dirent *file_dirent;
	char *name;
	name_list_t *next;
};

static int global_want_quit;
static name_list_t *patch_names;

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

static char *full_filename(const char *file) {
	char *file_path;
	asprintf(&file_path, "%s/%s", STUDIO_DIR, file);
	return file_path;
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

static int parse_file(struct dirent *cur_file, int *copy_depth) {
	char *file_path;
	int retval;

	file_path = full_filename(cur_file->d_name);
	retval = libgieditor_read_copy_data_from_file(
			file_path, copy_depth);
	free(file_path);

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
	return retval;
}

static void write_file(char *filename, int *copy_depth) {
	char *file_path;
	int retval;

	file_path = full_filename(filename);
	retval = libgieditor_write_copy_data_to_file(file_path, copy_depth);
	free(file_path);

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
}

static int read_studio_set(int *copy_depth) {
	int retval;
	retval = libgieditor_copy_class(&libgieditor_top_midi_class,
		libgieditor_studio_address, copy_depth);
	if (retval) {
	    char *msg[1];
	    msg[0] = "Error retrieving data";
	    dialog_box(1, msg, dialog_continue);
	}
	return retval;
}

static void print_rhc(struct dirent *cur_file, int *copy_depth,
		WINDOW *menu_sub_win, int skip, int i) {
	char *print_string = NULL;
	char *func_name = "print_rhc()";
	name_list_t *cur_patch;

	/* First check if we've cached this patch */
	cur_patch = patch_names;
	while (cur_patch) {
	    if (cur_patch->file_dirent == cur_file) {
		print_string = cur_patch->name;
		goto found;
	    }
	    cur_patch = cur_patch->next;
	}

	/* Not found, so add it to the cache */
	if (!parse_file(cur_file, copy_depth))
	    print_string = libgieditor_get_copy_patch_name();

	if (!patch_names) {
	    cur_patch = allocate(name_list_t, 1, func_name);
	    patch_names = cur_patch;
	} else {
	    cur_patch = patch_names;
	    while (cur_patch->next) cur_patch = cur_patch->next;
	    cur_patch->next = allocate(name_list_t, 1, func_name);
	    cur_patch = cur_patch->next;
	}

	cur_patch->next = NULL;
	cur_patch->name = print_string;
	cur_patch->file_dirent = cur_file;

found:
	if (print_string) PRINT_STRING(print_string, i - skip)
	else PRINT_STRING("Invalid file", i - skip);

	libgieditor_flush_copy_data(copy_depth);
}

static void flush_patches(void) {
	name_list_t *cur_patch;
	while (patch_names) {
	    cur_patch = patch_names;
	    patch_names = patch_names->next;
	    if (cur_patch->name) free(cur_patch->name);
	    free(cur_patch);
	}
}

static void do_paste(int *copy_depth) {
	int layer, part, retval;

	retval = libgieditor_paste_class(&libgieditor_top_midi_class,
			    libgieditor_studio_address, copy_depth);

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

static int file_filter(const struct dirent *entry) {
	struct stat file_info;
	char *filename;
	if (entry->d_name[0] == '.') return 0;
	filename = full_filename(entry->d_name);
	if (stat(filename, &file_info) < 0) return 0;
	free(filename);
	if (!S_ISREG(file_info.st_mode)) return 0;
	return 1;
}

static char *studio_explorer(char **headers, char *footer) {
	PANEL *explorer_panel;
	MENU *explorer_menu;
	WINDOW *explorer_win, *menu_sub_win;
	ITEM **member_items;
	ITEM *cur;
	int copy_depth = 0;
	int retval;
	int n_members;
	int n_parents = 0;
	int menu_width = COLS - 23;
	int c, i, want_break = 0, want_restart = 0;
	int position = 0, skip = 0, damaged = 1, first_draw = 1;
	char *func_name = "studio_explorer()";
	char *filename;
	char *file_path;
	char **long_names;
	char *loaded_patch_name = NULL;
	struct dirent **dir_contents;
	struct dirent *cur_file;

	n_members = scandir(STUDIO_DIR, &dir_contents,
			    file_filter, versionsort);
	
	cbreak(); clear();

	long_names = allocate(char *, n_members, func_name);

	member_items = allocate(ITEM *, n_members + 1, func_name);
	for (i = 0; i < n_members; i++) {
	    long_names[i] = make_long_name(dir_contents[i]->d_name,
			    menu_width);
	    member_items[i] = new_item(long_names[i], NULL);
	    set_item_userptr(member_items[i], (void *) dir_contents[i]);
	}
	member_items[n_members] = NULL;

	explorer_win = newwin(LINES - 1, COLS, 0, 0);
	explorer_panel = new_panel(explorer_win);
	explorer_menu = new_menu(member_items);
	box(explorer_win, 0, 0);
	set_menu_mark(explorer_menu, " * ");
	menu_sub_win = derwin(explorer_win, LINES - 5 - (2 * n_parents),
			COLS - 2, 3 + (2 * n_parents), 1);
	set_menu_win(explorer_menu, explorer_win);
	set_menu_sub(explorer_menu, menu_sub_win);
	set_menu_format(explorer_menu, LINES - 5 - (2 * n_parents), 1);

	for (i = 0; i < n_parents + 1; i++) {
	    print_in_middle(explorer_win, 1 + (2 * i), 0, COLS, headers[i]); 
	    mvwaddch(explorer_win, 2 + (2 * i), 0, ACS_LTEE);
	    mvwhline(explorer_win, 2 + (2 * i), 1, ACS_HLINE, COLS-2);
	    mvwaddch(explorer_win, 2 + (2 * i), COLS-1, ACS_RTEE);
	}

        mvprintw(LINES - 1, 0, footer);
	post_menu(explorer_menu);

	do {
		if (damaged) {
		    for (i = skip; i < n_members; i++) {
			cur_file = item_userptr(member_items[i]);
			print_rhc(cur_file, &copy_depth, menu_sub_win, skip, i);
		    }
		    damaged = 0;
		    if (!first_draw) wrefresh(menu_sub_win);
		    first_draw = 0;
		}
		update_panels();
		doupdate();
		c = getch();
		switch(c) {
		    case KEY_DOWN:
		    case '/':
			if ( position == LINES - 3 - (2 * n_parents) ) { 
			    if (skip <
				    (n_members - LINES + 2 + (2 * n_parents))) {
				skip++;
				damaged = 1;
			    }
			} else position++;
			menu_driver(explorer_menu, REQ_DOWN_ITEM);
			break;
		    case KEY_UP:
		    case '.':
			if (position == 0) {
			    if (skip > 0) {
				skip--;
				damaged = 1;
			    }
			} else position--;
			menu_driver(explorer_menu, REQ_UP_ITEM);
			break;
		    case 's':
			{   char *message[1];
			    message[0] = "Warning: Overwrite current file?";
			    if (!dialog_box(1, message, dialog_yesno)) break;
			}
			cur = current_item(explorer_menu);
			cur_file = item_userptr(cur);
			mvprintw(LINES - 1, COLS - 12, " Reading...");
			update_panels();
			doupdate();
			if (!read_studio_set(&copy_depth))
			    write_file(cur_file->d_name, &copy_depth);
			mvprintw(LINES - 1, COLS - 12, " Done      ");
			break;
		    case 'l':
			cur = current_item(explorer_menu);
			cur_file = item_userptr(cur);
			mvprintw(LINES - 1, COLS - 12, " Writing...");
			update_panels();
			doupdate();
			parse_file(cur_file, &copy_depth);
			if (loaded_patch_name) free(loaded_patch_name);
			loaded_patch_name = libgieditor_get_copy_patch_name(); 
			do_paste(&copy_depth);
			mvprintw(LINES - 1, COLS - 12, " Done      ");
			break;
		    case 'n':
			retval = get_string("Filename:", &filename);
			if (retval < 0) break;
			mvprintw(LINES - 1, COLS - 12, " Reading...");
			update_panels();
			doupdate();
			if (!read_studio_set(&copy_depth))
			    write_file(filename, &copy_depth);
			free(filename);
		    case 'r':
			if (!loaded_patch_name) {
			    if (!read_studio_set(&copy_depth)) {
				loaded_patch_name = 
					libgieditor_get_copy_patch_name();
				libgieditor_flush_copy_data(&copy_depth);
			    }
			}
			want_break = 1;
			want_restart = 1;
			break;
		    case 'q':
			want_break = 1;
			break;
		}
	} while( !want_break );
	
	flush_patches();
	for (i = 0; i < n_members; i++)
	    free(dir_contents[i]);
	free(dir_contents);
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
	if (want_restart) return loaded_patch_name;
	return NULL;
}

static void studio_explorer_menu_wrapper(void) {
        char *headers[1];
	char *init_header = " Studio Explorer ";
	char *footer =  
	" (R)efresh, (L)oad, (N)ew, (S)ave, (Q)uit. ";
	int init = 1;
	char *patch_name = "";
	while(patch_name) {
	    if (init) {
		headers[0] = init_header;
		init = 0;
	    } else {
		asprintf(&headers[0], " Studio Explorer (%s) ", patch_name);
		free(patch_name);
	    }
	    patch_name = studio_explorer(headers, footer);
	    if (headers[0] != init_header) free(headers[0]);
	}
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
	{ "Studio Explorer",		studio_explorer_menu_wrapper },
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
				unpost_menu(main_menu);
				if (p) p();
				post_menu(main_menu);
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
