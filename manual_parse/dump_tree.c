/* Midi address dumper - requires roland manual
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
 * $Id: dump_tree.c,v 1.11 2012/06/27 07:46:06 kmtaylor Exp $
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include "midi_tree.h"

#ifdef USE_LOG
#include <log.h>
#include <libgieditor.h>
#define allocate(t) __common_allocate(sizeof(t), "manual_parser")
#else
#include <curses.h>
//#include "interface.h"
#define allocate(t) __interface_allocate(sizeof(t), "manual_parser")
#endif

#define header(...) fprintf(stderr, __VA_ARGS__)

int contains_error;
int ln;
static int num_addresses;
static int num_classes;

Midi_tree midi_tree;

static inline uint32_t parse_address(int b1, int b2, int b3, int b4) {
	return b1<<24 | b2<<16 | b3<<8 | b4;
}

static inline uint32_t parse_size(int size1, int size2) {
	return 1;
}

Classes new_class_list(Class first, Classes rest) {
        Classes classes = allocate(struct s_class_list);
        classes->first = first;
        classes->rest = rest;
        return classes;
}

Class new_class(char *name, Address_lines address_list) {
	if (address_list == NULL) exit(1);
	Class class = allocate(struct s_class);
	class->type = address_list->first->type;
	switch (class->type) {
	    case TOPLEVEL_CLASS :
		class->info.toplevel_class.name = name;
		class->info.toplevel_class.address_list = address_list;
		break;
	    case MIDLEVEL_CLASS :
		class->info.midlevel_class.name = name;
		class->info.midlevel_class.address_list = address_list;
		break;
	    case LOWLEVEL_CLASS :
		class->info.lowlevel_class.name = name;
		class->info.lowlevel_class.address_list = address_list;
		break;
	}
	return class;
}

Address_lines new_address_list(Address_line first, Address_lines rest) {
        Address_lines list = allocate(struct s_address_line_list);
        list->first = first;
        list->rest = rest;
        return list;
}

Address_line new_toplevel_address(int b1, int b2, int b3, int b4, char *desc) {
	Address_line address = allocate(struct s_address_line);
	address->type = FOURBYTE;
	address->parent = NULL;
	address->info.four_byte_address.description = desc;
	address->info.four_byte_address.sysex_addr = 
				    parse_address(b1, b2, b3, b4);
	return address;
}

Address_line new_midlevel_address(int b2, int b3, int b4, char *desc) {
	Address_line address = allocate(struct s_address_line);
	address->type = THREEBYTE;
	address->info.three_byte_address.description = desc;
	address->info.three_byte_address.sysex_addr = 
				    parse_address(0, b2, b3, b4);
	return address;
}

Address_line new_address(int b3, int b4, int size1, int size2, char *desc) {
	Address_line address = allocate(struct s_address_line);
	address->type = TWOBYTE;
	address->info.two_byte_address.description = desc;
	address->info.two_byte_address.sysex_addr = 
				    parse_address(0, 0, b3, b4);
	address->info.two_byte_address.sysex_size = 
				    parse_size(size1, size2);
	return address;
}

uint32_t *current_sysex_hash_address_size;
char **current_sysex_hash_address_desc;
int currently_doing_hash = 0;
struct s_address_line dummy_address_line = { 
	.type = IGNORE, 
};

Address_line new_hash_address(int b3, int b4, int size1, int size2) {
	currently_doing_hash = 1;
	Address_line address = allocate(struct s_address_line);
	address->type = TWOBYTE;
	address->info.two_byte_address.sysex_size = 1;
	current_sysex_hash_address_desc = 
		&address->info.two_byte_address.description;
	current_sysex_hash_address_size = 
		&address->info.two_byte_address.sysex_size;
	address->info.two_byte_address.sysex_addr = 
				    parse_address(0, 0, b3, b4);
	return address;
}

static const char *get_class_name_from_desc(char *parent, char *desc) {
	int i = 0, j = 0;
	while (class_map + i) {
	    if (!strncmp(parent, class_map[i].parent_name,
				    strlen(class_map[i].parent_name))) {
		while (class_map[i].child_names[j]) {
		    if (!strncmp(desc, class_map[i].child_names[j],
					strlen(class_map[i].child_names[j]))) {
			return class_map[i].child_names[j+1];
		    }
		    j+=2;
		}
	    }
	    i++;
	}
	return NULL;
}

#define GET_CLASS_PARAM(partype, name) \
static partype get_class_##name(Class class) {				\
	partype retval;							\
	switch (class->type) {						\
            case TOPLEVEL_CLASS :					\
                retval = class->info.toplevel_class.name;		\
                break;							\
            case MIDLEVEL_CLASS :					\
                retval = class->info.midlevel_class.name;		\
                break;							\
            case LOWLEVEL_CLASS :					\
                retval = class->info.lowlevel_class.name;		\
                break;							\
        }								\
	return retval;							\
}

GET_CLASS_PARAM(char *, name)
GET_CLASS_PARAM(Address_lines, address_list)

#define GET_ADDRESS_PARAM(partype, name) \
static partype get_address_##name(Address_line address) {		\
	partype retval;							\
	switch (address->type) {					\
            case FOURBYTE :						\
                retval = address->info.four_byte_address.name;		\
                break;							\
            case THREEBYTE :						\
                retval = address->info.three_byte_address.name;		\
                break;							\
            case TWOBYTE :						\
                retval = address->info.two_byte_address.name;		\
                break;							\
	    case IGNORE :						\
		retval = 0;						\
		break;							\
        }								\
	return retval;							\
}

GET_ADDRESS_PARAM(uint32_t, sysex_addr)
GET_ADDRESS_PARAM(char *, description)

static Class get_class_by_name(const char *name) {
	Classes classes = midi_tree;
	char *comp_name;
	if (!name) return NULL;
	while (classes) {
	    comp_name = get_class_name(classes->first);
	    if (!strncmp(name, comp_name, strlen(name)))
		    return classes->first;
	    classes = classes->rest;
	}
	return NULL;
}

static Class get_class(Address_line address) {
	const char *class_name;
	char *parent_name = "TOPLEVEL";

	if (address->type != FOURBYTE) {
		/* Parent has already been processed, use class name */
		parent_name = get_class_name(address->parent->private_class);
	}

	class_name = get_class_name_from_desc(parent_name,
					    get_address_description(address));

	return get_class_by_name(class_name);
}

static void span_addresses(Address_line address);

static void span_class(Class class, Address_line parent) {
	Address_lines addresses = get_class_address_list(class);
	while (addresses) {
	    addresses->first->parent = parent;
	    span_addresses(addresses->first);
	    addresses = addresses->rest;
	}
}

static void print_address_entry(Address_line address) {
	Address_line parents = address;
	uint32_t sysex_addr = 0;
	while (parents) {
	    sysex_addr |= get_address_sysex_addr(parents);
	    parents = parents->parent;
	}

	printf("\t{\n");
	printf("\t\t.sysex_addr = 0x%x,\n", sysex_addr);
	printf("\t\t.sysex_size = 0x%x,\n", 
			address->info.two_byte_address.sysex_size);
	printf("\t\t.class = &%s,\n", address->parent->private_class->cname);
	printf("\t},\n");
	num_addresses++;
}
	
/* Recursive */
static void span_addresses(Address_line address) {
	Class class;
	switch (address->type) {
	    case FOURBYTE :
	    case THREEBYTE:
		class = get_class(address);
		if (address->parent)
		    class->parent = address->parent->private_class;
		address->private_class = class;
		span_class(class, address);
		break;
	    case TWOBYTE :
		print_address_entry(address);
		return;
	    case IGNORE :
		return;
	}
}

static int make_class_c_names(Classes classes) {
	int class_c_name_iterator = 0;
	Class class;
	while (classes) {
	    class = classes->first;
	    asprintf(&class->cname, "libgieditor_midi_class_%i",
			    class_c_name_iterator);
	    printf("\t&%s,\n", class->cname);
	    class_c_name_iterator++;
	    classes = classes->rest;
	}
	return class_c_name_iterator;
}

static void dump_class(Class class) {
	Class parents;
	int num_members = 0;
	int have_parents = (class != midi_tree->first);
	
	header("/* %s */\n", get_class_name(class));
	header("extern const struct s_midi_class %s;\n", class->cname);

	printf("const struct s_midi_class %s = {\n", class->cname);
	printf("\t.name = \"%s\",\n", get_class_name(class));
	printf("\t.members = (struct s_midi_class_member []) {\n");

	Address_lines addresses = get_class_address_list(class);
	Address_line address;
	while (addresses) {
	    address = addresses->first;
	    if (address->type != IGNORE) {
		printf("\t\t{\n");
		printf("\t\t\t.name = \"%s\",\n",
				get_address_description(address));
		printf("\t\t\t.sysex_addr_base = 0x%x,\n",
			    get_address_sysex_addr(address));
		if (address->type != TWOBYTE)
		    printf("\t\t\t.class = &%s,\n",
				    address->private_class->cname);
		printf("\t\t},\n");
		num_members++;
	    }
	    addresses = addresses->rest;
	}

	printf("\t},\n");

	if (have_parents) {
	    printf("\t.parents = (const struct s_midi_class  * []) {\n");
	}
	parents = class->parent;
	while(parents) {
	    printf("\t\t&%s,\n", parents->cname);
	    parents = parents->parent;
	}
	if (have_parents) {
	    printf("\t\t&libgieditor_midi_class_0,\n");
	    printf("\t\tNULL\n");
	    printf("\t},\n");
	}

	if (!have_parents)
	    printf("\t.parents = NULL,\n");

	printf("\t.size = %i,\n", num_members);
	printf("};\n");
}

static void span_classes_down(Classes classes) {
	Class class;
	while (classes) {
	    class = classes->first;
	    dump_class(class);
	    classes = classes->rest;
	}
}

int main(void) {
	Address_lines toplevel_addresses;
#define xstr(s) str(s)
#define str(s) #s
	yyin = fopen(xstr(MIDI_ADDRESSES), "r");
	if (yyin == NULL) {
                common_log(1, "Error: could not open midi specification");
                exit(1);
        }
	if (yyparse() != 0 || contains_error) {
                common_log(2, "Errors encountered reading midispecification\n",
				"These can probably be safely ignored");
	}

	header("/*  Parsed midi addresses.\n");
	header(" *  This file is automatically generated, DO NOT MODIFY\n");
	header(" */\n\n");

	printf("/*  Parsed midi addresses.\n");
	printf(" *  This file is automatically generated, DO NOT MODIFY\n");
	printf(" */\n\n");
	printf("#include <stdio.h>\n");
	printf("#include <stdint.h>\n\n");
	printf("#include \"libgieditor.h\"\n\n");
	printf("#include \"midi_addresses.h\"\n\n");

	/* Build classes array */
	header("extern MidiClass *libgieditor_midi_classes[];\n");
	printf("MidiClass *libgieditor_midi_classes[] = {\n");

	num_classes = make_class_c_names(midi_tree);

	printf("};\n");

	/* Build a bottom up array mapping addresses to classes */
	header("extern midi_address libgieditor_midi_addresses[];\n");
	printf("midi_address libgieditor_midi_addresses[] = {\n");

	toplevel_addresses = get_class_address_list(midi_tree->first);
	while (toplevel_addresses) {
	    span_addresses(toplevel_addresses->first);
	    toplevel_addresses = toplevel_addresses->rest;
	}

	printf("};\n");

	/* Build a top down linked list mapping classes */
	span_classes_down(midi_tree);

	printf("\nconst unsigned int libgieditor_num_addresses = %u;\n", 
			num_addresses);
	header("extern const unsigned int libgieditor_num_addresses;\n");
	printf("\nconst unsigned int libgieditor_num_classes = %u;\n", 
			num_classes);
	header("extern const unsigned int libgieditor_num_classes;\n");

        fclose(yyin);
        yylex_destroy();	
	
	return 0;
}
