/* Main API
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
 * $Id: libgieditor.h,v 1.18 2012/07/18 05:42:11 kmtaylor Exp $
 */

#define libgieditor_top_midi_class libgieditor_midi_class_0
#define libgieditor_studio_class libgieditor_midi_class_3
#define libgieditor_liveset_class libgieditor_midi_class_2
#define DEFAULT_DEVICE_ID 0x10
#define DEFAULT_MODEL_ID 0x4C

#ifndef LIBGIEDITOR_DEBUG
#define LIBGIEDITOR_DEBUG 1
#endif

#define TIMEOUT_TIME 200
#define MAX_SET_NAME_SIZE 16
#define NUM_USER_PATCHES 256

extern void *__common_allocate(size_t size, char *func_name);

typedef const struct s_midi_class MidiClass;
typedef struct s_midi_class_member MidiClassMember;

struct s_midi_class_member {
	const char	    *name;
	const uint32_t	    sysex_addr_base;
	const MidiClass	    *class;
	int		    blacklisted;
};

struct s_midi_class {
	const char		*name;
	MidiClassMember		*members;
	const int		size;
	const MidiClass		**parents;
};

enum midi_address_flags {
	M_ADDRESS_FETCHED		= 0x01,
	M_ADDRESS_BLACKLISTED		= 0x10,
};

typedef struct s_midi_address {
	uint32_t		sysex_addr;
	uint32_t		sysex_size;
	const MidiClass		*class;
	enum midi_address_flags	flags;
	uint32_t		value;
} midi_address;

enum gi_patch_flags {
	GI_PATCH_NAME_KNOWN		= 0x01,
};

typedef struct s_gi_patch {
	char			name[MAX_SET_NAME_SIZE + 1];
	enum gi_patch_flags	flags;
	uint32_t		sysex_base_addr;
} GiPatch;

enum init_flags {
	LIBGIEDITOR_READ		= 0x01,
	LIBGIEDITOR_WRITE		= 0x02,
	LIBGIEDITOR_ACK			= 0x04,
};

extern int libgieditor_init(const char *client_name, enum init_flags flags);
extern int libgieditor_close(void);

extern void libgieditor_set_timeout(int timeout_time);

extern midi_address *libgieditor_match_midi_address(uint32_t sysex_addr);
extern MidiClass *libgieditor_match_class_name(char *class_name);

extern const char *libgieditor_get_desc(uint32_t sysex_addr);

extern uint32_t libgieditor_add_addresses(uint32_t address1,
				uint32_t address2);

extern char **libgieditor_get_parents(uint32_t sysex_addr, int *num);

extern uint32_t libgieditor_get_sysex_size(uint32_t sysex_addr);

extern int libgieditor_class_num_parents(MidiClass *class);
extern uint32_t libgieditor_get_sysex_value(uint8_t *data, uint32_t size);

/* Block, waiting for a new incoming sysex event
 * If successful, DATA contains a newly allocated buffer of the received
 * sysex data, and the return value is the number of bytes received.
 * If unsuccessful, DATA is set to NULL
 * On timeout, libgieditor_listen_sysex_event returns -1 */
extern int libgieditor_listen_sysex_event(uint8_t *command_id, 
		                uint32_t *address, uint8_t **data);

extern void libgieditor_send_bulk_sysex(midi_address m_addresses[],
		const int num);

extern void libgieditor_send_sysex(uint32_t sysex_addr,
		                uint32_t sysex_size, uint8_t *data);

extern void libgieditor_send_sysex_value(uint32_t sysex_addr,
				uint32_t sysex_size, uint32_t sysex_value);

extern int libgieditor_get_bulk_sysex(midi_address m_addresses[],
		const int num);

/* Requests sysex data and blocks waiting for a response.
 * If a response is received, DATA points to a newly allocated buffer
 * containing the raw sysex data.
 * If a timeout occurs, an unrecognised response arrives or a checksum
 * error occurs, libgieditor_get_sysex returns -1 with DATA set to NULL.
 * If BLACKLISTING is enabled, an attempt to read a blacklisted SYSEX_ADDR
 * will result in a return value of -2 with DATA set to NULL */
extern int libgieditor_get_sysex(uint32_t sysex_addr,
		                uint32_t sysex_size, uint8_t **data);

extern char *libgieditor_get_patch_name(uint32_t sysex_addr);
extern int libgieditor_refresh_patch_names(void);

/* Class refers to the parent class */
extern int libgieditor_copy_class(MidiClass *class, uint32_t sysex_addr,
				int *depth);
extern int libgieditor_paste_class(MidiClass *class, uint32_t sysex_addr,
		                int *depth);
extern int libgieditor_paste_layer_to_part(MidiClass *class,
		uint32_t sysex_addr, int *depth, int layer, int part);
extern void libgieditor_flush_copy_data(int *depth);
extern int libgieditor_write_copy_data_to_file(char *filename, int *depth);
extern int libgieditor_read_copy_data_from_file(char *filename, int *depth);

#ifdef LIBGIEDITOR_PRIVATE

#define MAX_SYSEX_PACKET_SIZE 120

#define FIRST_USER_PATCH_ADDR 0x20000000
#define USER_PATCH_DELTA 0x10000 

#define ADDRESS_GROUP	    "Addresses"
#define GENERAL_GROUP	    "General"
#define SIZE_KEY	    "Size"
#define CLASS_KEY	    "Class"
#define ADDRESS_BASE_KEY    "BaseAddress"

typedef struct s_class_data Class_data;
struct s_class_data {
	midi_address	    *m_addresses;
	uint32_t	    sysex_addr_base;
	MidiClass	    *class;
	Class_data	    *next;
	unsigned int	    size;
};

#endif
