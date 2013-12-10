/* Jack transport controller
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
 * $Id: midi2jacksync.c,v 1.2 2012/07/16 15:11:51 kmtaylor Exp $
 */

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <math.h>

#include <jack/jack.h>
#include <jack/midiport.h>

#include "libgieditor.h"
#include "avr_api.h"

#define CLIENT_NAME "JackSyncer"
#define CLIENT_CONTROLLER_NAME "JackSyncerAVR"

#define JITTER_TOLERANCE 30

#define MIDI_CLOCK_CONT	    0xFB
#define MIDI_CLOCK_STOP	    0xFC
#define MIDI_CLOCK_TICK	    0xF8
#define MIDI_SPP	    0xF2
#define MIDI_CTL_MSG	    0xB0
#define MIDI_CTL_CHANNEL    0
#define MIDI_AVR_CHANNEL    1
#define MIDI_CTL_SIZE	    3
#define CONTROL_OFFSET	    1
#define VALUE_OFFSET	    2
#define CONTROL_TO_BUFFER(buf, control, value) \
    buf[0] = MIDI_CTL_MSG | MIDI_CTL_CHANNEL;				    \
    buf[CONTROL_OFFSET] = control;					    \
    buf[VALUE_OFFSET] = value
#define allocate(t, num) __common_allocate(sizeof(t) * num, "midi2jacksync")

typedef struct s_midictl_list *Midictl_list;
struct s_midictl_list {
	uint8_t		control;
	uint8_t		value;
	Midictl_list	next;
};

typedef struct s_midi_rltm_list *Midi_rltm_list;
struct s_midi_rltm_list {
	uint8_t		byte[3];
	int		spp;
	Midi_rltm_list	next;
};

static inline uint8_t *rltm_to_buffer(Midi_rltm_list rltm, int *length) {
	*length = rltm->spp ? 3 : 1;
	return rltm->byte;
}

static int tempo;
static int do_sync;

/* Only access with midi_lock */
static Midictl_list midictl_in_list;
static Midictl_list midictl_led_list;
static Midi_rltm_list midi_rltm_list;
static int midi_clock_counting;
static unsigned long midi_clock_count;
enum e_status {
	TRANSPORT_STOPPED = 0,
	TRANSPORT_RUNNING = 1,
	TRANSPORT_RECORDING = 2,
};
static enum e_status transport_status;

static jack_client_t *jack_client;
static jack_port_t *midi_in_port;
static jack_port_t *midi_led_port;
static jack_port_t *midi_rltm_port;

static pthread_mutex_t midi_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t read_data_ready = PTHREAD_COND_INITIALIZER;
static pthread_cond_t write_data_ready = PTHREAD_COND_INITIALIZER;

static void add_midictl_event(Midictl_list *global_midictl_list, uint8_t *data, 
					    int size) {
	Midictl_list cur_midictl;
	if (!*global_midictl_list) {
	    *global_midictl_list = allocate(struct s_midictl_list, 1);
	    cur_midictl = *global_midictl_list;
	} else {
	    cur_midictl = *global_midictl_list;
	    while (cur_midictl->next) cur_midictl = cur_midictl->next;
	    cur_midictl->next = allocate(struct s_midictl_list, 1);
	    cur_midictl = cur_midictl->next;
	}
	cur_midictl->next = NULL;
	cur_midictl->control = data[CONTROL_OFFSET];
	cur_midictl->value = data[VALUE_OFFSET];
}

static void __add_rltm_event(Midi_rltm_list *global_rltm_list,
		uint8_t b1, uint8_t b2, uint8_t b3, int spp) {
	Midi_rltm_list cur_rltm;
	if (!*global_rltm_list) {
	    *global_rltm_list = allocate(struct s_midi_rltm_list, 1);
	    cur_rltm = *global_rltm_list;
	} else {
	    cur_rltm = *global_rltm_list;
	    while (cur_rltm->next) cur_rltm = cur_rltm->next;
	    cur_rltm->next = allocate(struct s_midi_rltm_list, 1);
	    cur_rltm = cur_rltm->next;
	}
	cur_rltm->next = NULL;
	cur_rltm->spp = spp;
	cur_rltm->byte[0] = b1;
	cur_rltm->byte[1] = b2;
	cur_rltm->byte[2] = b3;
}

static void add_rltm_event(Midi_rltm_list *global_rltm_list, uint8_t byte) {
	__add_rltm_event(global_rltm_list, byte, 0, 0, 0);
}

static void add_spp_event(Midi_rltm_list *global_rltm_list) {
	__add_rltm_event(global_rltm_list, MIDI_SPP,
			(midi_clock_count / 6) & 0x7f,
			((midi_clock_count / 6) >> 7) & 0x7f, 1);
}

static unsigned long midiclk_from_ms(unsigned long ms) {
	return ms * (tempo / (float) 2500);
}

static unsigned long ms_from_midiclk(void) {
	return midi_clock_count * (2500 / (float) tempo);
}

static long midiclk_measures(void) {
	return 1 + lround(midi_clock_count * 25 / (float) 2400);
}

static void set_midiclk_measures(long measures) {
	midi_clock_count = 96 * (measures - 1);	
}

static long measure_from_frame(jack_position_t *pos) {
	return 1 + lround((tempo * pos->frame) /
			(double) (240 * pos->frame_rate));
}

static long frame_from_measure(jack_position_t *pos, int measure) {
	return lround(240 * (measure - 1) * (pos->frame_rate-1) /
			(double) tempo);
}

static int process_callback(jack_nframes_t nframes, void *arg) {
	jack_midi_event_t jack_midi_event;
	jack_nframes_t event_index = 0;
	Midictl_list cur_midictl;
	Midi_rltm_list cur_rltm;
	uint8_t data[MIDI_CTL_SIZE];
	uint8_t *buf;
	int length;

	pthread_mutex_lock(&midi_lock);

	void *midi_in_buf = jack_port_get_buffer(midi_in_port, nframes);
	void *midi_led_buf = jack_port_get_buffer(midi_led_port, nframes);
	void *midi_rltm_buf = jack_port_get_buffer(midi_rltm_port, nframes);

        jack_midi_clear_buffer(midi_led_buf);
        jack_midi_clear_buffer(midi_rltm_buf);

	if (!midictl_led_list)
	    pthread_cond_signal(&write_data_ready);

	while (midi_rltm_list) {
	    cur_rltm = midi_rltm_list;
	    midi_rltm_list = midi_rltm_list->next;
	    buf = rltm_to_buffer(cur_rltm, &length);
	    jack_midi_event_write(midi_rltm_buf, event_index++, buf, length);
	    free(cur_rltm);
	}
	
        event_index = 0;

        while (midictl_led_list) {
            cur_midictl = midictl_led_list;
            midictl_led_list = midictl_led_list->next;
	    CONTROL_TO_BUFFER(data, cur_midictl->control, cur_midictl->value);
            jack_midi_event_write(midi_led_buf, event_index++, data,
                                MIDI_CTL_SIZE);
            free(cur_midictl);
        }

        event_index = 0;

	while (jack_midi_event_get(&jack_midi_event, midi_in_buf, 
					event_index++) == 0) {
	    if ((jack_midi_event.buffer[0] & 0xF0) == MIDI_CTL_MSG) {
		add_midictl_event(&midictl_in_list, jack_midi_event.buffer,
				    jack_midi_event.size);
		pthread_cond_signal(&read_data_ready);
	    }
	    if (jack_midi_event.buffer[0] == MIDI_CLOCK_TICK
			    && midi_clock_counting) {
		midi_clock_count++;
	    }
	    if (jack_midi_event.buffer[0] == MIDI_CLOCK_CONT) {
		transport_status |= TRANSPORT_RUNNING;
		pthread_cond_signal(&read_data_ready);
	    }
	    if (jack_midi_event.buffer[0] == MIDI_CLOCK_STOP) {
		transport_status &= ~TRANSPORT_RUNNING;
		pthread_cond_signal(&read_data_ready);
	    }
			
	}

	pthread_mutex_unlock(&midi_lock);

	return 0;
}

static int sync_callback(jack_transport_state_t state, jack_position_t *pos,
		void *arg) {
	unsigned long jack_time_ms;

	long jack_measure = measure_from_frame(pos);
	long measure_frame = frame_from_measure(pos, jack_measure);
	long delta = (pos->frame > measure_frame ?
			(pos->frame - measure_frame) :
			(measure_frame - pos->frame));

	/* If we are within 300 frames, consider it a measure boundary,
	 * otherwise, just ignore the sync request */
	if (delta > 300) return 1;

	printf("GI measure %li\n", midiclk_measures());
	printf("Jack measure %li\n", jack_measure);
	printf("Adjusting by %li measures\n", jack_measure - midiclk_measures());

	pthread_mutex_lock(&midi_lock);
	avr_delta_measure(jack_measure - midiclk_measures());
	set_midiclk_measures(jack_measure);
	add_spp_event(&midi_rltm_list);
	pthread_mutex_unlock(&midi_lock);

	return 1;
}

static void timebase_callback(jack_transport_state_t state,
		jack_nframes_t nframes, jack_position_t *pos,
		int new_pos, void *arg) {
	unsigned long jack_time_ms;
	unsigned long device_time_ms;
	unsigned long jitter;
	jack_nframes_t new_frame;
	static int begin_counting;

	if (state != JackTransportRolling) {
	    pthread_mutex_lock(&midi_lock);
	    midi_clock_counting = 0;
	    pthread_mutex_unlock(&midi_lock);
	    begin_counting = 1;
	    printf("Transport not rolling, resetting counter\n");
	    return;
	}

	if (begin_counting) {
	    pthread_mutex_lock(&midi_lock);
	    midi_clock_counting = 1;
	    pthread_mutex_unlock(&midi_lock);
	    printf("Transport rolling\n");
	    begin_counting = 0;
	}

	jack_time_ms = pos->frame * (1000 / (float) pos->frame_rate);

	device_time_ms = ms_from_midiclk();
	jitter = (device_time_ms > jack_time_ms ?
			(device_time_ms - jack_time_ms) :
			(jack_time_ms - device_time_ms));

	if (jitter > JITTER_TOLERANCE) {
	    printf("Sync required, jitter time in ms %li\n", jitter);
	    new_frame = device_time_ms * (pos->frame_rate / 1000);
	    jack_transport_locate(jack_client, new_frame);
	}

	if (do_sync)
	    pos->beats_per_minute = tempo;
}

static int jack_init(const char *client_name) {
	jack_status_t jack_status;
	jack_client = 
		jack_client_open(client_name, JackNoStartServer, &jack_status);
	if (!jack_client) return -1;

	midi_in_port = jack_port_register(jack_client, "midi_in",
			JACK_DEFAULT_MIDI_TYPE,
			JackPortIsInput | JackPortIsTerminal, 0);
	
	midi_led_port = jack_port_register(jack_client, "midi_led",
			JACK_DEFAULT_MIDI_TYPE,
			JackPortIsOutput | JackPortIsTerminal, 0);
	
	midi_rltm_port = jack_port_register(jack_client, "midi_rltm",
			JACK_DEFAULT_MIDI_TYPE,
			JackPortIsOutput | JackPortIsTerminal, 0);
	
	if (!(midi_in_port && midi_led_port && midi_rltm_port)) return -1;
	
	if (jack_set_process_callback(jack_client, process_callback, 0) < 0)
		return -1;

	if (do_sync) {
	    if (jack_set_sync_callback(jack_client, sync_callback, 0) < 0)
		return -1;
	}

	if (do_sync) {
	    if (jack_set_timebase_callback(jack_client, 
				0, timebase_callback, 0) < 0) return -1;
	}

	if (jack_activate(jack_client)) return -1;

	return 0;
}

/* Must have midi_lock before calling this function */
static void send_one_midiled(uint8_t control, uint8_t value) {
        uint8_t data[MIDI_CTL_SIZE];
        CONTROL_TO_BUFFER(data, control, value);
        add_midictl_event(&midictl_led_list, data, MIDI_CTL_SIZE);
}

#define MIDI2JACKSYNC
#include "korgnano.c"
static void reset_controller(void) {
	korgnano_reset();
}

static void update_leds(void) {
	korg_update_leds(transport_status);
}

static int jack_close(void) {
	pthread_mutex_lock(&midi_lock);
	reset_controller();
	while (midictl_led_list != NULL) {
	    pthread_cond_wait(&write_data_ready, &midi_lock);
	}
	pthread_mutex_unlock(&midi_lock);
	jack_deactivate(jack_client);
	if (midi_in_port)
	    jack_port_unregister(jack_client, midi_in_port);
	if (midi_led_port)
	    jack_port_unregister(jack_client, midi_led_port);
	if (midi_rltm_port)
	    jack_port_unregister(jack_client, midi_rltm_port);
	return jack_client_close(jack_client);
}

static void signal_handler(int unused) {
	avr_api_close();
	jack_close();
	exit(0);
}

static int process_read_data(void) {
	Midictl_list cur_midictl;
	static enum e_status old_transport_status;
	int transport_change = old_transport_status != transport_status;
	int do_led_update = 0;
	jack_position_t pos;
	long measure, frame;

        pthread_mutex_lock(&midi_lock);

        while (	(midictl_in_list == NULL) && !transport_change ) {
            pthread_cond_wait(&read_data_ready, &midi_lock);
	    transport_change = old_transport_status != transport_status;
        }

	if (midictl_in_list) {
	    cur_midictl = midictl_in_list;
	    midictl_in_list = midictl_in_list->next;

	    KORG_START_BUTTON_PRESSED(cur_midictl) {
		jack_transport_locate(jack_client, 0);
		set_midiclk_measures(1);
		avr_toggle_restart();
		if (transport_status & TRANSPORT_RECORDING) {
		    transport_status &= ~TRANSPORT_RECORDING;
		    transport_change = 1;
		}
	    }

	    KORG_REC_BUTTON_PRESSED(cur_midictl) {
		do_led_update = 1;
		avr_toggle_rec();
		(transport_status & TRANSPORT_RECORDING) ?
			( transport_status &= ~TRANSPORT_RECORDING ) :
			( transport_status |=  TRANSPORT_RECORDING );
	    }

	    KORG_PLAY_BUTTON_PRESSED(cur_midictl) {
		transport_status |= TRANSPORT_RUNNING;
		transport_change = 1;
	    }

	    KORG_STOP_BUTTON_PRESSED(cur_midictl) {
		transport_status &= ~TRANSPORT_RUNNING;
		transport_change = 1;
		if (transport_status & TRANSPORT_RECORDING) {
		    transport_status &= ~TRANSPORT_RECORDING;
		}
	    }

	    KORG_BACK_BUTTON_PRESSED(cur_midictl) {
		jack_transport_query(jack_client, &pos);
		measure = measure_from_frame(&pos);
		if (measure != 1) {
		    frame = frame_from_measure(&pos, --measure);
		    jack_transport_locate(jack_client, frame);
		}
	    }

	    KORG_FORWARD_BUTTON_PRESSED(cur_midictl) {
		jack_transport_query(jack_client, &pos);
		measure = measure_from_frame(&pos);
		frame = frame_from_measure(&pos, ++measure);
		jack_transport_locate(jack_client, frame);
	    }

	    free(cur_midictl);
	}

	if (transport_change) {
	    if (transport_status & TRANSPORT_RUNNING) {
		jack_transport_start(jack_client);
		add_rltm_event(&midi_rltm_list, MIDI_CLOCK_CONT);
		midi_clock_counting = 1;
	    } else {
		jack_transport_stop(jack_client);
		add_rltm_event(&midi_rltm_list, MIDI_CLOCK_STOP);
		midi_clock_counting = 0;
	    }
	    do_led_update = 1;
	}

	if (do_led_update) update_leds();

	old_transport_status = transport_status;

        pthread_mutex_unlock(&midi_lock);

        return 0;
}

int main(int argc, char **argv) {
	if (argc < 2) {
	    printf("Usage: %s tempo\n", argv[0]);
	    printf("To disable midi clock synchronisation, "
			    "enter a negative tempo\n");
	    exit(1);
	}

	tempo = atoi(argv[1]);
	if (tempo > 0) {
	    do_sync = 1;
	    printf("Midi clock tempo: %ibpm\n", tempo);
	}

	if (jack_init(CLIENT_NAME) < 0) {
	    printf("Couldn't initialise jack client.\n"
		    "Check that jackd is running and there is no existing "
		    "timebase master.\n");
	    exit(1);
	}
	if (avr_api_init(CLIENT_CONTROLLER_NAME,
				LIBGIEDITOR_ACK | LIBGIEDITOR_WRITE) < 0 ) {
	    printf("Couldn't initialise avr interface.\n");
	    exit(1);
	}
	signal(SIGINT, signal_handler);
	while (1) process_read_data();
}
