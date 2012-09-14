/* read_midi
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
 * $Id: read_midi.c,v 1.9 2012/06/28 08:00:42 kmtaylor Exp $
 */

#include <stdio.h>
#include <stdint.h>
#include <signal.h>
#include <stdlib.h>

#include "libgieditor.h"

#define CLIENT_NAME "read_midi"

static void signal_handler(int unused) {
	if (libgieditor_close()) {
                printf("Error closing libgieditor\n");
        }
	exit(0);
}

int main(int argc, char **argv) {
	char *address_description;
	char **address_parents;
	uint32_t sysex_addr, sysex_size, sysex_value;
	int num_parents;
	int i;
	int bytes;
	uint8_t *sysex_data;
	uint8_t command_id;
	midi_address *m_address;
	
	if (libgieditor_init(CLIENT_NAME, LIBGIEDITOR_READ) < 0) {
	    fprintf(stderr, "Library initialisation failed, aborting\n");
	    fprintf(stderr, "Check that jackd is running.\n");
	    goto failed;
	}

	signal(SIGINT, signal_handler);
	libgieditor_set_timeout(-1);

	while(1) {
		bytes = libgieditor_listen_sysex_event(&command_id,
				&sysex_addr, &sysex_data);

		if (bytes == 0) {
			printf("Received non compliant sysex message\n");
			continue;
		}

		printf("Recevied %u bytes with command id: %x\n",
				bytes, command_id);

		m_address = libgieditor_match_midi_address(sysex_addr);
		
		if (!m_address) {
			printf("Address unknown: %x\n", sysex_addr);
			goto ignore;
		}

		printf("Received address:\t0x%x\n", sysex_addr);

		address_parents = (char **) 
			libgieditor_get_parents(sysex_addr, &num_parents);
		if (address_parents) {
		    printf("Parents:\t\t");
		    printf("1: %s\n", address_parents[0]);
		    for (i = 1; i < num_parents; i++) {
			printf("\t\t\t%i: %s\n", i + 1, address_parents[i]);
		    }
		    free(address_parents);
		}

		i = 0;
		while (bytes > 0) {
		    sysex_size = libgieditor_get_sysex_size(sysex_addr);
		    address_description = (char *)
			    libgieditor_get_desc(sysex_addr);
		    sysex_value = libgieditor_get_sysex_value(sysex_data + i,
				    sysex_size);
		    printf("%8i :\t\t%s\n", sysex_value, address_description);
		    
		    sysex_addr = libgieditor_add_addresses(sysex_addr,
				    sysex_size);
		    i += sysex_size;
		    bytes -= sysex_size;
		}

		printf("\n");
ignore:
		free(sysex_data);
	}

	if (libgieditor_close()) {
		printf("Error closing libgieditor\n");
	}
	return 0;

failed:
	if (libgieditor_close()) {
		printf("Error closing libgieditor\n");
	}
	return -1;
}
