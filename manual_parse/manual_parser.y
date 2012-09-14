/* Roland manual parser
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
 * $Id: manual_parser.y,v 1.4 2012/06/12 03:05:35 kmtaylor Exp $
 */

%{
#include <stdio.h>
#include "midi_tree.h"
#include <log.h>
#include <libgieditor.h>
#if !USE_LOG
#include <curses.h>
#include "interface.h"
#endif
extern int yylex(void);
%}

%union {
    int			int_val;
    float		float_val;
    char		*str_val;
    Classes		classes_mem;
    Class		class_mem;
    Address_lines	address_lines_mem;
    Address_line	address_line_mem;
}

%error-verbose

%token INVALID_TOKEN
%token START_TOKEN
%token BANG_TOKEN
%token HASH_TOKEN
%token NEWLINE
%token <int_val> INT_TOKEN
%token <int_val> HEX_TOKEN
%token <str_val> FLOAT_TOKEN
%token <str_val> IDENT_TOKEN
%token <str_val> STRING_TOKEN
%token <str_val> CLASS_TOKEN

%type <classes_mem>		midi_tree
%type <classes_mem>		classes
%type <class_mem>		class
%type <class_mem>		toplevel_class
%type <address_lines_mem>	address_lines
%type <address_line_mem>	address_line

%start midi_tree

%%
/*---------------------------------------------------------------------------*/
midi_tree
    : toplevel_class classes
	{
	    midi_tree = new_class_list($1, $2);
	}
    ;

classes
    : class classes
	{
	    $$ = new_class_list($1, $2);
	}
    | class
	{
	    $$ = new_class_list($1, NULL);
	}
    ;

class
    : CLASS_TOKEN address_lines
	{
	    $$ = new_class($1, $2);
	}
    ;

toplevel_class
    : START_TOKEN address_lines
	{
	    $$ = new_class("TOPLEVEL", $2);
	}
    ;

address_lines
    : address_line address_lines
	{
	    $$ = new_address_list($1, $2);
	}
    | address_line
	{
	    $$ = new_address_list($1, NULL);
	}
    | error NEWLINE address_lines
	{
	    $$ = $3;
	}
    | error NEWLINE
	{
	    $$ = NULL;
	}
    ;

address_line
    : BANG_TOKEN HEX_TOKEN HEX_TOKEN HEX_TOKEN HEX_TOKEN STRING_TOKEN NEWLINE
	{
	    $$ = new_toplevel_address($2, $3, $4, $5, $6);
	}
    | BANG_TOKEN HEX_TOKEN HEX_TOKEN HEX_TOKEN STRING_TOKEN NEWLINE
	{
	    $$ = new_midlevel_address($2, $3, $4, $5);
	}
    | BANG_TOKEN HEX_TOKEN HEX_TOKEN BANG_TOKEN HEX_TOKEN HEX_TOKEN
	    STRING_TOKEN NEWLINE
	{
	    $$ = new_address($2, $3, $5, $6, $7);
	}
    | HASH_TOKEN HEX_TOKEN HEX_TOKEN BANG_TOKEN HEX_TOKEN HEX_TOKEN
	    BANG_TOKEN BANG_TOKEN NEWLINE
	{
	    $$ = new_hash_address($2, $3, $5, $6);
	}
    | BANG_TOKEN BANG_TOKEN HEX_TOKEN HEX_TOKEN BANG_TOKEN BANG_TOKEN NEWLINE
	{
	    if (currently_doing_hash) {
		(*current_sysex_hash_address_size)++;
		$$ = &dummy_address_line;
	    }
	}
    | BANG_TOKEN BANG_TOKEN HEX_TOKEN HEX_TOKEN STRING_TOKEN NEWLINE
	{
	    if (currently_doing_hash) {
		*current_sysex_hash_address_desc = $5;
		(*current_sysex_hash_address_size)++;
		currently_doing_hash = 0;
		$$ = &dummy_address_line;
	    }
	}
    ;

%%

#if YYDEBUG
#if USE_LOG
void yyerror(const char *msg) {
    char *strings[2];
    asprintf(&strings[0], "Problem: %s", msg);
    asprintf(&strings[1], "%i", ln + 1);
    common_log(4, "Error parsing midi specification\n" , strings[0],
		    ", line: ", strings[1]);
    contains_error = 1;
    free(strings[0]); free(strings[1]);
}
#else
void yyerror(const char *msg) {
	char *message[6];
	char *strings[2];
	asprintf(&strings[0], "Problem: %s", msg);
	asprintf(&strings[1], "at line number: %i", ln + 1);
	message[0] = "Warning:";
	message[1] = "There is an error in the midi specification file:";
	message[2] = CONFIG_FILE;
	message[3] = "";
	message[4] = strings[0];
	message[5] = strings[1];
	dialog_box(6, message, dialog_continue);
	contains_error = 1;
	free(strings[0]); free(strings[1]);
}
#endif
#else
void yyerror(const char *msg) {
}
#endif
