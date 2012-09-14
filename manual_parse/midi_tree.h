/* Midi tree structure
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
 * $Id: midi_tree.h,v 1.2 2012/06/08 04:22:16 kmtaylor Exp $
 */

#include <stdint.h>
struct s_class_map {
        const char *parent_name;
        const char **child_names;
};
extern struct s_class_map class_map[];

extern FILE *yyin;
extern int yyparse(void);
extern void yyerror(const char *msg);
extern int yylex_destroy(void);
extern int yydebug;
extern int contains_error;
extern int ln;

typedef struct s_class_list		*Classes;
typedef struct s_class			*Class;
typedef struct s_address_line_list	*Address_lines;
typedef struct s_address_line   	*Address_line;

typedef Classes Midi_tree;

extern Midi_tree midi_tree;

struct s_lowlevel_class {
	char		    *name;
	Classes		    children;
	Address_lines	    address_list;
};

struct s_midlevel_class {
	char		    *name;
	Classes		    children;
	Address_lines	    address_list;
};

struct s_toplevel_class {
	char		    *name;
	Classes		    children;
	Address_lines	    address_list;
};

typedef union {
	struct s_lowlevel_class		lowlevel_class;
	struct s_midlevel_class		midlevel_class;
	struct s_toplevel_class		toplevel_class;
} Class_info;

/* Aligns with ALineType */
typedef enum {
	TOPLEVEL_CLASS, MIDLEVEL_CLASS, LOWLEVEL_CLASS
} ClassType;

struct s_class_list {
	Class		first;
	Classes		rest;
};

struct s_class {
	ClassType	type;
	Class_info	info;
	char		*cname;
	Class		parent;
};

struct s_four_byte_address {
	char		*description;
	uint32_t	sysex_addr;
};

struct s_three_byte_address {
	char		*description;
	uint32_t	sysex_addr;
};

struct s_two_byte_address {
	char		*description;
	uint32_t	sysex_addr;
	uint32_t	sysex_size;
};

typedef union {
	struct s_four_byte_address	four_byte_address;
	struct s_three_byte_address	three_byte_address;
	struct s_two_byte_address	two_byte_address;
} ALine_info;

typedef enum {
	FOURBYTE, THREEBYTE, TWOBYTE, IGNORE
} ALineType;

struct s_address_line_list {
	Address_line	first;
	Address_lines	rest;
};

struct s_address_line {
	ALineType	type;
	ALine_info	info;
	Address_line	parent;
	Class		private_class;
};

extern struct s_address_line dummy_address_line;
extern uint32_t *current_sysex_hash_address_size;
extern char **current_sysex_hash_address_desc;
extern int currently_doing_hash;
extern Address_line new_toplevel_address(int b1, int b2, int b3, int b4,
		char *desc);
extern Address_line new_midlevel_address(int b2, int b3, int b4, char *desc);
extern Address_line new_address(int b3, int b4, int size1, int size2,
		char *desc);
extern Address_line new_hash_address(int b3, int b4, int size1, int size2);
extern Address_lines new_address_list(Address_line first, Address_lines rest);
extern Class new_class(char *name, Address_lines address_list);
extern Classes new_class_list(Class first, Classes rest);
