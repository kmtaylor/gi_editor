/* libgieditor
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
 * $Id: libgieditor.c,v 1.19 2012/07/18 05:42:11 kmtaylor Exp $
 */

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <glib.h>

#define LIBGIEDITOR_PRIVATE
#include <libgieditor.h>
#include "midi_addresses.h"
#include "sysex.h"

#if LIBGIEDITOR_DEBUG
#include "log.h"
#include <malloc.h>
#endif

#define allocate(t, num) __common_allocate(sizeof(t) * num, "libgieditor")
#define NUM_ADDRESSES libgieditor_num_addresses
#define NUM_CLASSES libgieditor_num_classes

static uint8_t device_id = DEFAULT_DEVICE_ID;
static uint32_t model_id = DEFAULT_MODEL_ID;

static GiPatch libgieditor_gi_patches[NUM_USER_PATCHES];

static Class_data *copy_paste_data;

#ifdef BLACKLISTING
static int match_member_entry(MidiClass *class, uint32_t address) {
	int i;
	for (i = 0; i < class->size; i++) {
            if (class->members[i].sysex_addr_base > address)
                return i - 1;
        }
	return (i - 1);
}

#define BLACKLIST_CLASS_MEMBER(class, address) { \
	i = match_member_entry(&class, address);    \
	class.members[i].blacklisted = 1;	    \
}

static void blacklist_class_members(void) {
	int i;
	BLACKLIST_CLASS_MEMBER(libgieditor_midi_class_0,  0x00);
}

#define BLACKLIST_ADDRESS(address) { \
	m_address = libgieditor_match_midi_address(address);	\
	m_address->flags |= M_ADDRESS_BLACKLISTED;		\
}

static void blacklist_addresses(void) {
	midi_address *m_address;
	BLACKLIST_ADDRESS(0x10000000);
}
#endif

int libgieditor_init(const char *client_name, enum init_flags flags) {
	int retval;

	retval = sysex_init(client_name, TIMEOUT_TIME, flags);

#ifdef BLACKLISTING
	blacklist_class_members();
	blacklist_addresses();
#endif

	return retval;
}

int libgieditor_close(void) {
	int retval;

	retval = sysex_close();

	return retval;
}

uint32_t libgieditor_add_addresses(uint32_t address1, uint32_t address2) {
	uint8_t a1, a2, a3, a4, b1, b2, b3, b4, c1, c2, c3, c4;
	a1 = (address1 & 0xff000000) >> 24; b1 = (address2 & 0xff000000) >> 24;
	a2 = (address1 & 0x00ff0000) >> 16; b2 = (address2 & 0x00ff0000) >> 16;
	a3 = (address1 & 0x0000ff00) >> 8;  b3 = (address2 & 0x0000ff00) >> 8;
	a4 = (address1 & 0x000000ff);	    b4 = (address2 & 0x000000ff);

	c4 = a4 + b4;
	c3 = a3 + b3 + ((c4 & 0x80)>>7);
	c2 = a2 + b2 + ((c3 & 0x80)>>7);
	c1 = a1 + b1 + ((c2 & 0x80)>>7);

	c4 &= 0x7f;
	c3 &= 0x7f;
	c2 &= 0x7f;
	c1 &= 0x7f;

	return ( c1<<24 | c2<<16 | c3<<8 | c4 );
}

midi_address *libgieditor_match_midi_address(uint32_t sysex_addr) {
	int i;
        for (i = 0; i < NUM_ADDRESSES; i++) {
                if (libgieditor_midi_addresses[i].sysex_addr == sysex_addr) {
                        return &libgieditor_midi_addresses[i];
                }
        }
        return NULL;
}

MidiClass *libgieditor_match_class_name(char *class_name) {
	int i;
        for (i = 0; i < NUM_CLASSES; i++) {
                if (!strcmp(libgieditor_midi_classes[i]->name, class_name)) {
                        return libgieditor_midi_classes[i];
                }
        }
        return NULL;
}

static uint32_t class_member_base(MidiClass *class, uint32_t sysex_addr) {
	int i;
	for (i = 0; i < class->size; i++) {
            if (class->members[i].sysex_addr_base > sysex_addr)
                return class->members[i - 1].sysex_addr_base;
        }
	return class->members[i - 1].sysex_addr_base;
}

int libgieditor_class_num_parents(MidiClass *class) {
	int num_parents = 0;
	MidiClass *cur_parent;
	
	if (!class->parents) return 0;

	cur_parent = class->parents[0];
	while(cur_parent) {
	    cur_parent = class->parents[++num_parents];
	}

	return num_parents;
}

/* Once each match is made, we can subtract its base address */
static uint32_t match_class_member(uint32_t sysex_addr, MidiClass *class,
		int depth) {
	int i;
	uint32_t sub_addr = 0;
	int num_parents;
	MidiClass *cur_parent = NULL;
	
	if (!class)
	    class = libgieditor_match_midi_address(sysex_addr)->class;

	if (depth)
	    class = class->parents[depth - 1];

	num_parents = libgieditor_class_num_parents(class);

	for (i = 0; i < num_parents; i++) {
	    cur_parent = class->parents[num_parents - 1 - i];
	    sub_addr += class_member_base(cur_parent, sysex_addr - sub_addr);
	}

	sysex_addr -= sub_addr;

	for (i = 0; i < class->size; i++) {
            if (class->members[i].sysex_addr_base > sysex_addr)
                return i - 1;
        }
	return (i - 1);
}

const char *libgieditor_get_desc(uint32_t sysex_addr) {
	midi_address *m_address = libgieditor_match_midi_address(sysex_addr);
	if (!m_address) return NULL;
	int i = match_class_member(sysex_addr, m_address->class, 0);
	if (i < 0) return NULL;
	return (m_address->class->members[i].name);
}

char **libgieditor_get_parents(uint32_t sysex_addr, int *num_parents) {
	midi_address *m_address = libgieditor_match_midi_address(sysex_addr);
	int depth, i, j;
	char **parents_array;
	
	if (!m_address) return NULL;

	*num_parents = libgieditor_class_num_parents(m_address->class);

	if (*num_parents == 0) return NULL;

	parents_array = allocate(char *, *num_parents);

	for (depth = *num_parents, j = 0; depth > 0; depth--, j++) {
	    i = match_class_member(sysex_addr, m_address->class, depth);
	    asprintf(&parents_array[j], "%s",
		    m_address->class->parents[depth - 1]->members[i].name);
	}

	return parents_array;
}

uint32_t libgieditor_get_sysex_size(uint32_t sysex_addr) {
	midi_address *m_address = libgieditor_match_midi_address(sysex_addr);
	if (!m_address) return -1;
	return m_address->sysex_size;
}

uint32_t libgieditor_get_sysex_value(uint8_t *data, uint32_t size) {
	int i, j;
	uint32_t retval = 0;
	j = size - 1;
	for (i = 0; i < size; i++, j--) {
		retval |= data[j] << (i*4);
	}
	return retval;
}

static void libgieditor_write_sysex_value(uint32_t sysex_value,
			    uint32_t sysex_size, uint8_t *data) {
	int i, j;
	if (sysex_size == 1) {
	    data[0] = sysex_value & 0x7f;
	} else {
	    for (i = 0, j = sysex_size - 1; i < sysex_size; i++, j--) {
		data[j] = (sysex_value >> (i*4)) & 0xf;
	    }
	}
}

void libgieditor_set_device_id(uint8_t id) {
	device_id = id;
}

void libgieditor_set_model_id(uint32_t id) {
	model_id = id;
}

static int address_sort(const void *va, const void *vb) {
	const midi_address **a = (const midi_address **) va;
	const midi_address **b = (const midi_address **) vb;
	if ((*a)->sysex_addr == (*b)->sysex_addr) return 0;
	if ((*a)->sysex_addr > (*b)->sysex_addr) return 1;
	else return -1;
}

static int build_blocks(uint32_t block_addresses[], uint32_t block_sizes[],
		int block_offsets[], int *total_size, const int num,
		midi_address **s_addresses) {
	int i, blocks;
	block_addresses[0] = s_addresses[0]->sysex_addr;
	block_offsets[0] = 0;
	for (i = 0; i < num; i++) block_sizes[i] = 0;
	*total_size = 0;
	for (i = 1, blocks = 1; i < num; i++) {
	    if (s_addresses[i - 1]->sysex_addr + 
		    s_addresses[i - 1]->sysex_size ==
			s_addresses[i]->sysex_addr) {
		block_sizes[blocks - 1] += s_addresses[i - 1]->sysex_size;
		*total_size += s_addresses[i - 1]->sysex_size;
		if (block_sizes[blocks - 1] < MAX_SYSEX_PACKET_SIZE) {
		    continue;
		} else {
		    block_sizes[blocks - 1] -= s_addresses[i - 1]->sysex_size;
		    *total_size -= s_addresses[i - 1]->sysex_size;
		}
	    }
	    block_sizes[blocks - 1] += s_addresses[i - 1]->sysex_size;
	    *total_size += s_addresses[i - 1]->sysex_size;
	    if (block_sizes[blocks - 1] > MAX_SYSEX_PACKET_SIZE) {
		block_sizes[blocks - 1] -= s_addresses[i - 1]->sysex_size;
		*total_size -= s_addresses[i - 1]->sysex_size;
		i--;
	    }
	    block_offsets[blocks] = i;
	    block_addresses[blocks++] = s_addresses[i]->sysex_addr;
	}
	block_sizes[blocks - 1] += s_addresses[i - 1]->sysex_size;
	*total_size += s_addresses[i - 1]->sysex_size;
	if (block_sizes[blocks - 1] > MAX_SYSEX_PACKET_SIZE) {
	    block_sizes[blocks - 1] -= s_addresses[i - 1]->sysex_size;
	    block_sizes[blocks] += s_addresses[i - 1]->sysex_size;
	    block_offsets[blocks] = i - 1;
	    block_addresses[blocks++] = s_addresses[i - 1]->sysex_addr;
	}
	return blocks;
}

int libgieditor_get_bulk_sysex(midi_address m_addresses[], const int num) {
	int i, j, blocks;
	int retval = 0;
	int total_size;
	uint32_t block_addresses[num];
	uint32_t block_sizes[num];
	int block_offsets[num];
	uint8_t **data;
	int data_offset;
	midi_address *s_address;

	midi_address **s_addresses = allocate(midi_address *, num);
	for (i = 0; i < num; i++) {
		s_addresses[i] = &m_addresses[i];
	}
	qsort(s_addresses, num, sizeof(midi_address *), address_sort);

	blocks = build_blocks(block_addresses, block_sizes, block_offsets, 
			&total_size, num, s_addresses);

	data = allocate(uint8_t *, blocks);
	for (i = 0; i < blocks; i++) data[i] = NULL;
	for (i = 0; i < blocks; i++) {
	    retval = libgieditor_get_sysex(
			    block_addresses[i], block_sizes[i], &data[i]);
	    if (retval < 0) goto cleanup;
	}
	
	for (i = 0; i < blocks; i++) {
	    data_offset = 0;
	    for (j = 0; j < block_sizes[i]; j++) {
		s_address = s_addresses[block_offsets[i] + j];
		s_address->value = libgieditor_get_sysex_value(
			    &data[i][data_offset],
			    s_address->sysex_size);
		s_address->flags |= M_ADDRESS_FETCHED;
		data_offset += s_address->sysex_size;
		if (data_offset >= block_sizes[i]) break;
	    }
	}

cleanup:
	for (i = 0; i < blocks; i++) {
	    if (data[i]) free(data[i]);
	}
	free(data);
	free(s_addresses);
	return retval;
}

void libgieditor_send_bulk_sysex(midi_address m_addresses[], const int num) {
	int i, blocks;
	int total_size;
	uint32_t block_addresses[num];
	uint32_t block_sizes[num];
	int block_offsets[num];
	uint8_t *data;
	int data_offset = 0;

	midi_address **s_addresses = allocate(midi_address *, num);
	for (i = 0; i < num; i++) {
		s_addresses[i] = &m_addresses[i];
	}
	qsort(s_addresses, num, sizeof(midi_address *), address_sort);

	blocks = build_blocks(block_addresses, block_sizes, block_offsets, 
			&total_size, num, s_addresses);

	data = allocate(uint8_t *, total_size);
	for (i = 0; i < num; i++) {
	    libgieditor_write_sysex_value(s_addresses[i]->value,
			s_addresses[i]->sysex_size, &data[data_offset]);
	    data_offset += s_addresses[i]->sysex_size;
	}

	data_offset = 0;
	for (i = 0; i < blocks; i++) {
	    libgieditor_send_sysex(block_addresses[i], block_sizes[i],
			    data + data_offset);
	    data_offset += block_sizes[i];
	}
	free(data);
	free(s_addresses);
}

void libgieditor_send_sysex(uint32_t sysex_addr,
			    uint32_t sysex_size, uint8_t *data) {
	sysex_send(device_id, model_id, sysex_addr, sysex_size, data);
}

void libgieditor_send_sysex_value(uint32_t sysex_addr,
			    uint32_t sysex_size, uint32_t sysex_value) {
	uint8_t data[4];
	libgieditor_write_sysex_value(sysex_value, sysex_size, data);
	libgieditor_send_sysex(sysex_addr, sysex_size, data);
}

int libgieditor_get_sysex(uint32_t sysex_addr,
		                uint32_t sysex_size, uint8_t **data) {
	int retval;
	
#ifdef BLACKLISTING
	*data = NULL;
	midi_address *m_address = libgieditor_match_midi_address(sysex_addr);
	if (!m_address) return -2;
	int i = match_class_member(sysex_addr, m_address->class, 0);
	if (m_address->flags & M_ADDRESS_BLACKLISTED) return -2;
	if (m_address->class->members[i].blacklisted) return -2;
#endif

	retval = sysex_recv(device_id, model_id, sysex_addr, sysex_size, data);

	if (retval < 0) {
#ifdef BLACKLISTING
	    m_address->flags |= M_ADDRESS_BLACKLISTED;
#endif
#if LIBGIEDITOR_DEBUG
	    char *msg;
	    asprintf(&msg,
		    "Timeout while attempting to read from address 0x%08X",
		    sysex_addr);
	    common_log(1, msg);
	    free(msg);
#endif
	}

	return retval;
}

void libgieditor_set_timeout(int timeout_time) {
	sysex_set_timeout(timeout_time);
}

/* This function will block, returns the number of data bytes collected */
int libgieditor_listen_sysex_event(uint8_t *command_id, 
		uint32_t *address, uint8_t **data) {
	int sum;
	return sysex_listen_event(command_id, address, data, &sum);
}

char *libgieditor_get_patch_name(uint32_t sysex_addr) {
	int i;
        for (i = 0; i < NUM_USER_PATCHES; i++) {
                if (libgieditor_gi_patches[i].sysex_base_addr == sysex_addr) {
			break;
                }
        }
	if (libgieditor_gi_patches[i].flags & GI_PATCH_NAME_KNOWN)
	    return libgieditor_gi_patches[i].name;
        return NULL;
}

char *libgieditor_get_copy_patch_name(void) {
	int i;
	char patch_name[MAX_SET_NAME_SIZE + 1];

	if ((copy_paste_data->class != &libgieditor_studio_class) &&
	    (copy_paste_data->class != &libgieditor_liveset_class))
	    return NULL;

	for (i = 0; i < MAX_SET_NAME_SIZE; i++) {
	    patch_name[i] = copy_paste_data->m_addresses[i].value;
	}
	patch_name[i] = '\0';

	return strdup(patch_name);
}

int libgieditor_refresh_patch_names(void) {
	int i;
	char *data;
	uint32_t cur_sysex_base = FIRST_USER_PATCH_ADDR;

	for (i = 0; i < NUM_USER_PATCHES; i++) {
	    libgieditor_gi_patches[i].sysex_base_addr = cur_sysex_base;
	    if (libgieditor_get_sysex(cur_sysex_base, 
			    MAX_SET_NAME_SIZE,
			    (uint8_t **) &data) < 0) return -1;
	    memcpy(libgieditor_gi_patches[i].name, data, MAX_SET_NAME_SIZE);
	    libgieditor_gi_patches[i].name[MAX_SET_NAME_SIZE] = '\0';
	    free(data);
	    libgieditor_gi_patches[i].flags |= GI_PATCH_NAME_KNOWN;
	    cur_sysex_base = libgieditor_add_addresses(cur_sysex_base,
			    USER_PATCH_DELTA);
	}
	return 0;
}

static int transfer_addresses_under_member(MidiClassMember *class_member,
		uint32_t sysex_addr, int pasting) {
	int i, retval;
	MidiClass *class = class_member->class;

	if (!class) return 0;

	if (!class->members[0].class && pasting) {
	    libgieditor_send_bulk_sysex(
		    libgieditor_match_midi_address(sysex_addr),
		    class->size);
	}

	if (!class->members[0].class && !pasting) {
	    retval = libgieditor_get_bulk_sysex(
		    libgieditor_match_midi_address(sysex_addr),
		    class->size);
	    if (retval) return retval;
	}

	for (i = 0; i < class->size; i++) {
	    retval = transfer_addresses_under_member(&class->members[i],
			    sysex_addr + class->members[i].sysex_addr_base,
			    pasting);
	    if (retval) return retval;
	}
	return retval;
}

static int count_addresses_under_member(MidiClassMember *class_member,
		uint32_t sysex_addr) {
	int i, total = 0;
	MidiClass *class = class_member->class;

	if (!class) return 1;

	for (i = 0; i < class->size; i++) {
	    total += count_addresses_under_member(&class->members[i],
			    sysex_addr + class->members[i].sysex_addr_base);
	}
	return total;
}

static void cp_addresses_under_member(MidiClassMember *class_member,
		Class_data *cur_class_data,
		int *index, uint32_t sysex_addr, int pasting) {
	int i;
	MidiClass *class = class_member->class;
	midi_address *to, *from;

	/* Copy a single address */
	if (!class) {
	    if (pasting) {
		from = &cur_class_data->m_addresses[*index];
		to = libgieditor_match_midi_address(sysex_addr);
	    } else {
		to = &cur_class_data->m_addresses[*index];
		from = libgieditor_match_midi_address(sysex_addr);
	    }

	    memcpy(to, from, sizeof(midi_address));

	    if (pasting) {
		to->sysex_addr += cur_class_data->sysex_addr_base;
	    } else {
		to->sysex_addr -= cur_class_data->sysex_addr_base;
	    }

	    (*index)++;
	    return;
	}

	for (i = 0; i < class->size; i++) {
	    cp_addresses_under_member(&class->members[i],
			    cur_class_data, index,
			    sysex_addr + class->members[i].sysex_addr_base,
			    pasting);
	}
	return;
}

int libgieditor_copy_class(MidiClass *class, uint32_t sysex_addr, int *depth) {
	int num_addresses, retval;
	int index = 0;
	MidiClassMember *class_member;
        Class_data *cur_class_data, *last_class_data = NULL;

        if (!copy_paste_data) {
            copy_paste_data = allocate(struct s_class_data, 1);
            cur_class_data = copy_paste_data;
	    *depth = 1;
        } else {
            cur_class_data = copy_paste_data;
            while (cur_class_data->next) cur_class_data = cur_class_data->next;
            cur_class_data->next = allocate(struct s_class_data, 1);
	    last_class_data = cur_class_data;
            cur_class_data = cur_class_data->next;
	    (*depth)++;
        }
        cur_class_data->next = NULL;
	cur_class_data->m_addresses = NULL;

	if (!libgieditor_match_midi_address(sysex_addr)) {
	    retval = -1;
	    goto failed;
	}

	class_member = &class->members[match_class_member(sysex_addr,
								class, 0)];
	num_addresses = count_addresses_under_member(class_member, sysex_addr);
	retval = transfer_addresses_under_member(class_member, sysex_addr, 0);
	if (retval) goto failed;

	cur_class_data->size = num_addresses;
	cur_class_data->m_addresses = allocate(midi_address, num_addresses);
	cur_class_data->class = class_member->class;
	cur_class_data->sysex_addr_base = sysex_addr;

	cp_addresses_under_member(class_member,
					cur_class_data, &index, sysex_addr, 0);
	return 0;

failed:
	if (copy_paste_data == cur_class_data)
	    copy_paste_data = NULL;
	else
	    last_class_data->next = NULL;
	if (cur_class_data->m_addresses)
	    free(cur_class_data->m_addresses);
	free(cur_class_data);
	(*depth)--;
	return retval;
}

int libgieditor_paste_class(MidiClass *class, uint32_t sysex_addr,
		int *depth) {
	int dummy, i, retval = 0;
	Class_data *cur_class_data;
	Class_data *verify_class_data;
	Class_data *last_class_data;
	MidiClassMember *class_member;
	int index = 0;
	int *broken_address_data;
	int broken_addresses = 0, loops;
	midi_address *cur_broken_addr;
	uint8_t *data;

	cur_class_data = copy_paste_data;
	if (!cur_class_data) return -1;

	if (!libgieditor_match_midi_address(sysex_addr)) return -1;

	class_member = &class->members[match_class_member(sysex_addr,
								class, 0)];
	/* Sanity check */
	/* Live set chorus and reverb are compatible with studio set, as long
	 * as it's GM2 */
	if ((cur_class_data->class == &libgieditor_liveset_chorus_class &&
		class_member->class != &libgieditor_studio_chorus_class)    ||
	    (cur_class_data->class == &libgieditor_liveset_reverb_class &&
		class_member->class != &libgieditor_studio_reverb_class)    ||
	    (cur_class_data->class == &libgieditor_studio_chorus_class  &&
		class_member->class != &libgieditor_liveset_chorus_class)   ||
	    (cur_class_data->class == &libgieditor_studio_reverb_class  &&
		class_member->class != &libgieditor_liveset_reverb_class)   ||
	    (class_member->class != cur_class_data->class))
		return -2;

	cur_class_data->sysex_addr_base = sysex_addr;
	cp_addresses_under_member(class_member,
			                cur_class_data, &index, sysex_addr, 1);
	transfer_addresses_under_member(class_member, sysex_addr, 1);

	/* Verify that copy was perfect */
	sysex_wait_write();
	retval = libgieditor_copy_class(class,
			cur_class_data->sysex_addr_base, &dummy);

	if (retval < 0) return -4;

	broken_address_data = allocate(int, cur_class_data->size);

	verify_class_data = copy_paste_data;
	last_class_data = copy_paste_data;
	while (last_class_data->next->next)
		last_class_data = last_class_data->next;
	while (verify_class_data->next)
		verify_class_data = verify_class_data->next;
	last_class_data->next = NULL;

	for (i = 0; i < cur_class_data->size; i++) {
	    if (verify_class_data->m_addresses[i].value !=
			cur_class_data->m_addresses[i].value) {
		broken_address_data[broken_addresses] = i; 
		broken_addresses ++;
	    }
	}

	loops = broken_addresses;
	/* Attempt one by one copy of any addresses that failed */
	for (i = 0; i < loops; i++) {
	    cur_broken_addr =
		    &cur_class_data->m_addresses[broken_address_data[i]];
	    libgieditor_send_sysex_value(
				    cur_broken_addr->sysex_addr + 
				    cur_class_data->sysex_addr_base,
				    cur_broken_addr->sysex_size,
				    cur_broken_addr->value);
	    if (libgieditor_get_sysex(
				    cur_broken_addr->sysex_addr +
				    cur_class_data->sysex_addr_base,
				    cur_broken_addr->sysex_size, &data) < 0) {
		continue;
	    }
	    if (cur_broken_addr->value == libgieditor_get_sysex_value(data,
						cur_broken_addr->sysex_size)) {
		broken_addresses--;
	    }
	    free(data);
	}
	
	if (broken_addresses) retval = -3;

	copy_paste_data = copy_paste_data->next;
	if (cur_class_data->m_addresses)
		free(cur_class_data->m_addresses);
	if (verify_class_data->m_addresses)
		free(verify_class_data->m_addresses);
	free(verify_class_data);
	free(cur_class_data);
	free(broken_address_data);
	(*depth) -= 1;
	return retval;
}

/* Careful! These might change */
static uint32_t studio_part_address_offset(int part) {
	return 0x2000 + (0x100 * (part - 1));
}

static uint32_t studio_offset_address_offset(int part) {
	return 0x3000 + (0x100 * (part - 1));
}

static int live_layer_address_offset(int part) {
	return 206 + (65 * (part - 1));
}

static int live_offset_address_offset(int part) {
	return 466 + (71 * (part - 1));
}

#define BLOCK1_SKIP	1
#define BLOCK1_SIZE	41
#define BLOCK1_OFFSET	0
#define BLOCK2_SKIP	42
#define BLOCK2_SIZE	4
#define BLOCK2_OFFSET	15
#define BLOCK3_SKIP	48
#define BLOCK3_SIZE	3
#define BLOCK3_OFFSET	15
#define BLOCK_COPY(num) {\
	for (i = BLOCK##num##_SKIP;					\
		    i < BLOCK##num##_SIZE + BLOCK##num##_SKIP; i++) {	\
	    to->m_addresses[i + BLOCK##num##_OFFSET].value =		\
		from->m_addresses[i + offset].value;			\
	}}
static void libgieditor_copy_layer_data(Class_data *to, Class_data *from,
		int layer) {
	int i;
	uint32_t offset = live_layer_address_offset(layer);
	BLOCK_COPY(1);
	BLOCK_COPY(2);
	BLOCK_COPY(3);
}

static void libgieditor_copy_offset_data(Class_data *to, Class_data *from,
		int layer) {
	int i;
	uint32_t offset = live_offset_address_offset(layer);
	for (i = 0; i < to->size; i++) {
	    to->m_addresses[i].value = from->m_addresses[i + offset].value;
	}
}

int libgieditor_paste_layer_to_part(MidiClass *class, uint32_t sysex_addr,
		int *depth, int layer, int part) {
	int retval = 0, dummy;
	Class_data *cur_class_data, *studio_part_data, *studio_offset_data,
		   *last_class_data, *first_class_data;
	MidiClassMember *class_member;

	cur_class_data = copy_paste_data;
	if (!cur_class_data) return -1;

	if (!libgieditor_match_midi_address(sysex_addr)) return -1;

	class_member = &class->members[match_class_member(sysex_addr,
								class, 0)];
	/* Sanity check */
	if (cur_class_data->class != &libgieditor_liveset_class) return -2;
	if (class_member->class != &libgieditor_studio_class) return -2;
	if (layer < 1 || layer > 4) return -2;
	if (part  < 1 || part  > 16) return -2;

	/* Trick copy/paste routines */
	first_class_data = copy_paste_data;
	last_class_data = copy_paste_data;
	while (last_class_data->next)
	    last_class_data = last_class_data->next;
	copy_paste_data = NULL;

	/* Plan: 1. Copy studio part and offsets
	 *	 2. Copy cur_class_data layer and offsets into copied data
	 *	 3. Paste studio part and offsets
	 */

	retval = libgieditor_copy_class(class_member->class,
			sysex_addr + studio_part_address_offset(part),
			&dummy);

	if (retval < 0) {
	    retval = -4;
	    goto copy_failed;
	}

	studio_part_data = copy_paste_data;

	sysex_wait_write();
	retval = libgieditor_copy_class(class_member->class,
			sysex_addr + studio_offset_address_offset(part),
			&dummy);

	if (retval < 0) {
	    retval = -4;
	    goto copy_failed;
	}

	studio_offset_data = copy_paste_data;
	while (studio_offset_data->next)
		studio_offset_data = studio_offset_data->next;

	/* The hard work is done here */
	libgieditor_copy_layer_data(studio_part_data, cur_class_data, layer);
	libgieditor_copy_offset_data(studio_offset_data, cur_class_data, layer);

	retval = libgieditor_paste_class(class_member->class,
			sysex_addr + studio_part_address_offset(part),
			&dummy);

	if (retval < 0) goto paste_failed;

	retval = libgieditor_paste_class(class_member->class,
			sysex_addr + studio_offset_address_offset(part),
			&dummy);

	if (retval < 0) goto paste_failed;

	copy_paste_data = first_class_data;
	copy_paste_data = copy_paste_data->next;
	if (cur_class_data->m_addresses)
		free(cur_class_data->m_addresses);
	free(cur_class_data);
	(*depth) -= 1;
	return retval;

paste_failed:
	libgieditor_flush_copy_data(&dummy);

copy_failed:
	copy_paste_data = first_class_data;
	return retval;
}

void libgieditor_flush_copy_data(int *depth) {
        Class_data *cur_class_data;
        while (copy_paste_data) {
            cur_class_data = copy_paste_data;
            copy_paste_data = copy_paste_data->next;
	    if (cur_class_data->m_addresses) free(cur_class_data->m_addresses);
            free(cur_class_data);
        }
	*depth = 0;
}

#define CHECK_ERROR(val) \
	if (!val) goto parse_error;					    \
	p_error = 0;							    \
	retval = g_error_matches(error, G_KEY_FILE_ERROR,		    \
				G_KEY_FILE_ERROR_KEY_NOT_FOUND);	    \
	if (retval) p_error = 1;					    \
	retval = g_error_matches(error, G_KEY_FILE_ERROR,		    \
				G_KEY_FILE_ERROR_INVALID_VALUE);	    \
	if (retval) p_error = 1;					    \
	g_clear_error(&error);						    \
	if (p_error) goto parse_error

int libgieditor_read_copy_data_from_file(char *filename, int *depth) {
	int retval, p_error, i;
	GKeyFile *key_file;
	GError *error;
	gsize length;
	gchar *class_name;
	gchar **address_keys;
	gchar *cur_key;
        Class_data *cur_class_data;
	int num_addresses;
	uint32_t addr_base, sysex_addr;
	MidiClass *class;
	midi_address *cur_address;
	midi_address *lib_address;

	key_file = g_key_file_new();
	if (g_key_file_load_from_file(key_file, filename, 
				G_KEY_FILE_NONE, NULL) == FALSE) return -1;
	error = NULL;

	num_addresses = g_key_file_get_uint64(key_file, GENERAL_GROUP,
			SIZE_KEY, &error);
	CHECK_ERROR(num_addresses);

	addr_base = g_key_file_get_uint64(key_file, GENERAL_GROUP,
			ADDRESS_BASE_KEY, &error);
	CHECK_ERROR(addr_base);

	class_name = g_key_file_get_string(key_file, GENERAL_GROUP,
			CLASS_KEY, &error);
	CHECK_ERROR(class_name);

	class = libgieditor_match_class_name(class_name);
	free(class_name);
	if (!class) goto parse_error;

	address_keys = g_key_file_get_keys(key_file, ADDRESS_GROUP,
			&length, &error);
        if (g_error_matches(error, G_KEY_FILE_ERROR,
			        G_KEY_FILE_ERROR_GROUP_NOT_FOUND)) {
	    g_clear_error(&error);
	    goto parse_error;
	}

	if (length != num_addresses) {
	    g_strfreev(address_keys);
	    goto parse_error;
	}

        if (!copy_paste_data) {
            copy_paste_data = allocate(struct s_class_data, 1);
            cur_class_data = copy_paste_data;
	    *depth = 1;
        } else {
            cur_class_data = copy_paste_data;
            while (cur_class_data->next) cur_class_data = cur_class_data->next;
            cur_class_data->next = allocate(struct s_class_data, 1);
            cur_class_data = cur_class_data->next;
	    (*depth)++;
        }
        cur_class_data->next = NULL;
	cur_class_data->m_addresses = NULL;

	cur_class_data->size = num_addresses;
	cur_class_data->m_addresses = allocate(midi_address, num_addresses);
	cur_class_data->class = class;
	cur_class_data->sysex_addr_base = addr_base;

	for (i = 0; i < num_addresses; i++) {
	    cur_key = address_keys[i];
	    if (sscanf(cur_key, "0x%08X", &sysex_addr) != 1) continue;
	    lib_address = libgieditor_match_midi_address(
					    sysex_addr + addr_base);
	    cur_address = &cur_class_data->m_addresses[i];

	    cur_address->sysex_addr = sysex_addr;
	    cur_address->value = g_key_file_get_uint64(key_file,
			    ADDRESS_GROUP, cur_key, NULL);
	    cur_address->class = lib_address->class;
	    cur_address->sysex_size = lib_address->sysex_size;
	    cur_address->flags = 0;
	}

	g_strfreev(address_keys);
	g_key_file_free(key_file);
	return 0;

parse_error:
	return -2;
}

int libgieditor_write_copy_data_to_file(char *filename, int *depth) {
	int i;
	FILE *fp;
	GKeyFile *key_file;
	gsize length;
	gchar key_name[11];
	gchar *data;
        Class_data *cur_class_data;
	midi_address *m_address;

	cur_class_data = copy_paste_data;
	if (!cur_class_data) return -1;
	
	fp = fopen(filename, "w");
	if (!fp) return -2;

	key_file = g_key_file_new();
	
	g_key_file_set_string(key_file, GENERAL_GROUP,
			    CLASS_KEY, cur_class_data->class->name);
	g_key_file_set_uint64(key_file, GENERAL_GROUP,
			    ADDRESS_BASE_KEY, cur_class_data->sysex_addr_base);
	g_key_file_set_uint64(key_file, GENERAL_GROUP,
			    SIZE_KEY, cur_class_data->size);

	for (i = 0; i < cur_class_data->size; i++) {
	    m_address = &cur_class_data->m_addresses[i];
	    sprintf(key_name, "0x%08X", m_address->sysex_addr);
	    g_key_file_set_uint64(key_file, ADDRESS_GROUP,
					key_name, m_address->value);
	}

	data = g_key_file_to_data(key_file, &length, NULL);
	size_t s = fwrite(data, sizeof(gchar), length, fp);

	fclose(fp);
	free(data);
	g_key_file_free(key_file);
	copy_paste_data = copy_paste_data->next;
	if (cur_class_data->m_addresses)
		free(cur_class_data->m_addresses);
	free(cur_class_data);
	(*depth)--;
	return 0;
}
