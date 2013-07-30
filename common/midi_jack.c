/* SysEx message handler
 *
 * Juno Gi Editor (AVR interface)
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
 */

#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include <jack/jack.h>
#include <jack/midiport.h>

#include "libgieditor.h"

#define allocate(t, num) __common_allocate(sizeof(t) * num, "libgieditor")

#define MAX_SYSEX_SIZE		    512
#define MIDI_CMD_COMMON_SYSEX       0xf0
#define MIDI_CMD_COMMON_SYSEX_END   0xf7

typedef struct s_sysex_list *Sysex_list;
struct s_sysex_list {
	uint8_t		data[MAX_SYSEX_SIZE];
	int		size;
	Sysex_list	next;
};

/* Only access with midi_lock */
static Sysex_list sysex_in_list;
static Sysex_list sysex_out_list;
static int sysex_timeout;

static int sysex_timeout_loops;

static jack_client_t *jack_client;
static jack_port_t *midi_in_port;
static jack_port_t *midi_out_port;

static pthread_mutex_t midi_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t read_data_ready = PTHREAD_COND_INITIALIZER;
static pthread_cond_t write_data_ready = PTHREAD_COND_INITIALIZER;

static void add_sysex_event(Sysex_list *global_sysex_list, uint8_t *data, 
					    int size) {
	Sysex_list cur_sysex;
	if (!*global_sysex_list) {
	    *global_sysex_list = allocate(struct s_sysex_list, 1);
	    cur_sysex = *global_sysex_list;
	} else {
	    cur_sysex = *global_sysex_list;
	    while (cur_sysex->next) cur_sysex = cur_sysex->next;
	    cur_sysex->next = allocate(struct s_sysex_list, 1);
	    cur_sysex = cur_sysex->next;
	}
	cur_sysex->next = NULL;
	cur_sysex->size = size;
	memcpy(cur_sysex->data, data, size);
}

static void flush_sysex_list(Sysex_list *global_sysex_list) {
	Sysex_list cur_sysex;
	while (*global_sysex_list) {
            cur_sysex = *global_sysex_list;
            *global_sysex_list = (*global_sysex_list)->next;
            free(cur_sysex);
        }
}

void jack_flush_sysex_in_list(void) {
	pthread_mutex_lock(&midi_lock);
	flush_sysex_list(&sysex_in_list);
	pthread_mutex_unlock(&midi_lock);
}

static int jack_callback(jack_nframes_t nframes, void *arg) {
	jack_midi_event_t jack_midi_event;
	jack_nframes_t event_index = 0;
	Sysex_list cur_sysex;

	pthread_mutex_lock(&midi_lock);

	
	if (midi_out_port) {
	    void *midi_out_buf = jack_port_get_buffer(midi_out_port, nframes);
	    jack_midi_clear_buffer(midi_out_buf);

	    if (!sysex_out_list)
		pthread_cond_signal(&write_data_ready);

	    while (sysex_out_list) {
		cur_sysex = sysex_out_list;
		sysex_out_list = sysex_out_list->next;
		jack_midi_event_write(midi_out_buf, event_index++,
				cur_sysex->data,
				cur_sysex->size);
		free(cur_sysex);
	    }
	    event_index = 0;
	}


	if (midi_in_port) {
	    void *midi_in_buf = jack_port_get_buffer(midi_in_port, nframes);

	    while (jack_midi_event_get(&jack_midi_event, midi_in_buf, 
					event_index++) == 0) {
		if (( jack_midi_event.buffer[0] == MIDI_CMD_COMMON_SYSEX ) &&
			( jack_midi_event.buffer[jack_midi_event.size - 1] == 
			  MIDI_CMD_COMMON_SYSEX_END )) {
		    add_sysex_event(&sysex_in_list, jack_midi_event.buffer,
				    jack_midi_event.size);
		    pthread_cond_signal(&read_data_ready);
		}
	    }
	}

	if (sysex_timeout > 0) sysex_timeout--;
	if (sysex_timeout == 0) pthread_cond_signal(&read_data_ready);

	pthread_mutex_unlock(&midi_lock);

	return 0;
}

void jack_sysex_set_timeout(int timeout_time) {
	sysex_timeout_loops = timeout_time;
}

int jack_sysex_init(const char *client_name, int timeout_time,
						enum init_flags flags) {
	jack_status_t jack_status;
	jack_client = 
		jack_client_open(client_name, JackNoStartServer, &jack_status);
	if (!jack_client) return -1;

	sysex_timeout_loops = timeout_time;

	if (flags & LIBGIEDITOR_READ) {
	    midi_in_port = jack_port_register(jack_client, "sysex_midi_in",
			JACK_DEFAULT_MIDI_TYPE,
			JackPortIsInput | JackPortIsTerminal, 0);
	    if (!midi_in_port) return -1;
	}
	
	if (flags & LIBGIEDITOR_WRITE) {
	    midi_out_port = jack_port_register(jack_client, "sysex_midi_out",
			JACK_DEFAULT_MIDI_TYPE,
			JackPortIsOutput | JackPortIsTerminal, 0);
	    if (!midi_out_port) return -1;
	}
	
	jack_set_process_callback(jack_client, jack_callback, 0);

	if (jack_activate(jack_client)) return -1;

	return 0;
}

int jack_sysex_close(void) {
	jack_deactivate(jack_client);
	if (midi_in_port)
	    jack_port_unregister(jack_client, midi_in_port);
	if (midi_out_port)
	    jack_port_unregister(jack_client, midi_out_port);
	return jack_client_close(jack_client);
}

void jack_sysex_send_event(uint32_t sysex_size, uint8_t *data) {
	pthread_mutex_lock(&midi_lock);
	add_sysex_event(&sysex_out_list, data, sysex_size); 
	pthread_mutex_unlock(&midi_lock);
}

void jack_sysex_wait_write(void) {
	pthread_mutex_lock(&midi_lock);

	while (sysex_out_list != NULL) {
	    pthread_cond_wait(&write_data_ready, &midi_lock);
	}

	pthread_mutex_unlock(&midi_lock);
}

int jack_sysex_listen_event(uint8_t **data) {
        Sysex_list cur_sysex;
        int data_bytes;

        pthread_mutex_lock(&midi_lock);

        sysex_timeout = sysex_timeout_loops;
        while (sysex_in_list == NULL && sysex_timeout != 0) {
            pthread_cond_wait(&read_data_ready, &midi_lock);
        }

        *data = NULL;

        if (sysex_timeout == 0) {
            pthread_mutex_unlock(&midi_lock);
            return -1;
        }

        cur_sysex = sysex_in_list;
        sysex_in_list = sysex_in_list->next;

        data_bytes = cur_sysex->size;

        if (data_bytes > 0) {
            *data = allocate(uint8_t, data_bytes);
            memcpy(*data, cur_sysex->data, data_bytes);
        } else data_bytes = 0;

        free(cur_sysex);

        pthread_mutex_unlock(&midi_lock);

        return data_bytes;
}
