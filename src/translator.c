/* SysEx message translator
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
 * $Id: translator.c,v 1.6 2012/07/16 15:11:51 kmtaylor Exp $
 */

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>

#include <jack/jack.h>
#include <jack/midiport.h>

#include "libgieditor.h"

#define CLIENT_OUT_NAME "ControlTranslatorSysex"
#define CLIENT_IN_NAME "ControlTranslatorCTL"

#define MIDI_CTL_MSG	    0xB0
#define MIDI_NOTE_MSG	    0x90
#define MIDI_CTL_CHANNEL    0
#define MIDI_CTL_SIZE	    3
#define CONTROL_OFFSET	    1
#define VALUE_OFFSET	    2
#define CONTROL_TO_BUFFER(buf, control, value) \
    buf[0] = MIDI_CTL_MSG | MIDI_CTL_CHANNEL;				    \
    buf[CONTROL_OFFSET] = control;					    \
    buf[VALUE_OFFSET] = value

#define allocate(t, num) __common_allocate(sizeof(t) * num, "translator")

static uint8_t current_control_set;

typedef struct s_midictl_list *Midictl_list;
struct s_midictl_list {
	uint8_t		control;
	uint8_t		value;
	Midictl_list	next;
};

/* Only access with midi_lock */
static Midictl_list midictl_in_list;
static Midictl_list midictl_note_list;
static Midictl_list midictl_led_list;
static Midictl_list midictl_ctl_list;

static jack_client_t *jack_client;
static jack_port_t *midi_in_port;
static jack_port_t *midi_led_port;
static jack_port_t *midi_ctl_port;

static pthread_mutex_t midi_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t read_data_ready = PTHREAD_COND_INITIALIZER;
static pthread_cond_t note_data_ready = PTHREAD_COND_INITIALIZER;
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

static int jack_callback(jack_nframes_t nframes, void *arg) {
	jack_midi_event_t jack_midi_event;
	jack_nframes_t event_index = 0;
	Midictl_list cur_midictl;
	uint8_t data[MIDI_CTL_SIZE];

	pthread_mutex_lock(&midi_lock);

	void *midi_in_buf = jack_port_get_buffer(midi_in_port, nframes);
	void *midi_led_buf = jack_port_get_buffer(midi_led_port, nframes);
	void *midi_ctl_buf = jack_port_get_buffer(midi_ctl_port, nframes);

        jack_midi_clear_buffer(midi_led_buf);
        jack_midi_clear_buffer(midi_ctl_buf);

	if (!midictl_led_list)
	    pthread_cond_signal(&write_data_ready);

        while (midictl_led_list) {
            cur_midictl = midictl_led_list;
            midictl_led_list = midictl_led_list->next;
	    CONTROL_TO_BUFFER(data, cur_midictl->control, cur_midictl->value);
            jack_midi_event_write(midi_led_buf, event_index++, data,
                                MIDI_CTL_SIZE);
            free(cur_midictl);
        }

        event_index = 0;

        while (midictl_ctl_list) {
            cur_midictl = midictl_ctl_list;
            midictl_ctl_list = midictl_ctl_list->next;
	    CONTROL_TO_BUFFER(data, cur_midictl->control, cur_midictl->value);
            jack_midi_event_write(midi_ctl_buf, event_index++, data,
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
	    if ((jack_midi_event.buffer[0] & 0xF0) == MIDI_NOTE_MSG) {
		add_midictl_event(&midictl_note_list, jack_midi_event.buffer,
				    jack_midi_event.size);
		pthread_cond_signal(&note_data_ready);
	    }
	}

	pthread_mutex_unlock(&midi_lock);

	return 0;
}

static int jack_init(const char *client_name) {
	jack_status_t jack_status;
	jack_client = 
		jack_client_open(client_name, JackNoStartServer, &jack_status);
	if (!jack_client) return -1;

	midi_in_port = jack_port_register(jack_client, "midi_in",
			JACK_DEFAULT_MIDI_TYPE,
			JackPortIsInput | JackPortIsTerminal, 0);
	
	midi_led_port = jack_port_register(jack_client, "midi_led_out",
			JACK_DEFAULT_MIDI_TYPE,
			JackPortIsOutput | JackPortIsTerminal, 0);
	
	midi_ctl_port = jack_port_register(jack_client, "midi_ctl_out",
			JACK_DEFAULT_MIDI_TYPE,
			JackPortIsOutput | JackPortIsTerminal, 0);
	
	if (!(midi_in_port && midi_led_port && midi_ctl_port)) return -1;
	
	jack_set_process_callback(jack_client, jack_callback, 0);

	if (jack_activate(jack_client)) return -1;

	return 0;
}

/* Must have midi_lock before calling this function */
static void send_one_midictl(uint8_t control, uint8_t value) {
	uint8_t data[MIDI_CTL_SIZE];
	CONTROL_TO_BUFFER(data, control, value);
        add_midictl_event(&midictl_ctl_list, data, MIDI_CTL_SIZE);
}

/* Must have midi_lock before calling this function */
static void send_one_midiled(uint8_t control, uint8_t value) {
	uint8_t data[MIDI_CTL_SIZE];
	CONTROL_TO_BUFFER(data, control, value);
        add_midictl_event(&midictl_led_list, data, MIDI_CTL_SIZE);
}

#include "korgnano.c"
static void reset_controller(void) {
	korgnano_reset();
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
	if (midi_ctl_port)
	    jack_port_unregister(jack_client, midi_ctl_port);
	return jack_client_close(jack_client);
}

enum controller_types { SET_BUTTON, CTL_BUTTON, SLIDER, KNOB };
/* This is specific to my Korg nanoKONTROL2 */
static enum controller_types control_type[] = {
	SLIDER,
	KNOB,
	SET_BUTTON,
	CTL_BUTTON,
	CTL_BUTTON,
};
#define CONTROL_TYPES sizeof(control_type)/sizeof(control_type[0])

struct controller {
	int	    respond_to;
	int	    is_sysex;
	int	    latch;
	uint32_t    sysex_addr;
	uint8_t	    ctl_addr;
	uint32_t    min_value;
	uint32_t    max_value;
	void	    (*callback)(struct controller *);
	uint32_t    state;
};

typedef void (*note_forwarder)(unsigned char note);

static void update_states(void);
static void update_states_cb(struct controller *dummy) {
	update_states();
}

#define NUM_TO_COPY 8
static void juno_adsr_callback(struct controller *cur_controller) {
	int i;
	static int called;
	static int prev_respond_to;
	midi_address *m_addresses[NUM_TO_COPY];
	static midi_address t_addresses[NUM_TO_COPY];

	if (!called) {
	    m_addresses[0] = libgieditor_match_midi_address(0x10003033);
	    m_addresses[1] = libgieditor_match_midi_address(0x10003036);
	    m_addresses[2] = libgieditor_match_midi_address(0x10003037);
	    m_addresses[3] = libgieditor_match_midi_address(0x10003021);
	    m_addresses[4] = libgieditor_match_midi_address(0x10003024);
	    m_addresses[5] = libgieditor_match_midi_address(0x10003025);
	    m_addresses[6] = libgieditor_match_midi_address(0x10003026);
	    m_addresses[7] = libgieditor_match_midi_address(0x10003028);
	    for (i = 0; i < NUM_TO_COPY; i++) {
		memcpy(&t_addresses[i], m_addresses[i], sizeof(midi_address));
	    }
	    t_addresses[0].value = 0;
	    t_addresses[1].value = 127;
	    t_addresses[2].value = 127;
	    t_addresses[3].value = 0;
	    t_addresses[4].value = 0;
	    t_addresses[5].value = 127;
	    t_addresses[6].value = 127;
	    t_addresses[7].value = 0;

	    called = 1;
	}

	if (prev_respond_to != cur_controller->respond_to)
	    libgieditor_send_bulk_sysex(t_addresses, NUM_TO_COPY);

	prev_respond_to = cur_controller->respond_to;
};

static struct controller juno_106_0[] = {
	/* R	S   L	Sysex Add    CTL Add	    Min		Max */
	// LFO1 Rate
	{ 0x00,	1,  0, 0x10003039,	    0,		0,	    127	},
	// Vibrato Rate
	{ 0x10,	1,  0, 0x10002018,	    0,		0,	    127	},
	// Vibrato Delay
	{ 0x01,	1,  0, 0x1000201a,	    0,		0,	    127	},
	// Vibrato Depth
	{ 0x11,	1,  0, 0x10002019,	    0,		0,	    127	},
	// LFO1 TVF Depth Offset
	{ 0x02,	1,  0, 0x1000303d,	    0,		1,	    127	},
	// TVF Env Depth Offset
	{ 0x03,	1,  0, 0x1000301a,	    0,		1,	    127	},
	// TVF Cutoff Keyfolow Offset
	{ 0x12,	1,  0, 0x10003015,	    0,	       44,	     84	},
	// Filter Type
	{ 0x13,	1,  0, 0x10003013,	    0,	        0,	      4	},
	// ADSR
	{ 0x04,	1,  0, 0x10003032,	    0,		0,	    127,
							juno_adsr_callback},
	{ 0x05,	1,  0, 0x10003034,	    0,		0,	    127,
							juno_adsr_callback},
	{ 0x06,	1,  0, 0x10003038,	    0,		0,	    127,
							juno_adsr_callback},
	{ 0x07,	1,  0, 0x10003035,	    0,		0,	    127,
							juno_adsr_callback},
	{ 0x04,	1,  0, 0x10003020,	    0,		0,	    127,
							juno_adsr_callback},
	{ 0x05,	1,  0, 0x10003022,	    0,		0,	    127,
							juno_adsr_callback},
	{ 0x06,	1,  0, 0x10003027,	    0,		0,	    127,
							juno_adsr_callback},
	{ 0x07,	1,  0, 0x10003023,	    0,		0,	    127,
							juno_adsr_callback},
	// Cutoff
	{ 0x14,	1,  0, 0x10003014,	    0,	        1,	    127	},
	// Resonance
	{ 0x15,	1,  0, 0x10003018,	    0,	        0,	    127	},
	// Pitch Bend Sens
	{ 0x16,	1,  0, 0x1000200f,	    0,	        0,	     24	},
	// Portamento Time
	{ 0x17,	1,  0, 0x10002011,	    0,	        0,	    127	},
	// Portamento Switch
	{ 0x37,	1,  1, 0x10002010,	    0,		0,	      1	},
	// Mono/Poly Switch
	{ 0x36,	1,  1, 0x1000200d,	    0,		0,	      1	},
	// Legato Switch
	{ 0x35,	1,  1, 0x1000200e,	    0,		0,	      1	},
	// LFO1 Key Trigger
	{ 0x30,	1,  1, 0x1000303b,	    0,		0,	      1	},
	{ -1 }
};

static struct controller juno_106_1[] = {
	/* R	S   L	Sysex Add    CTL Add	    Min		Max */
	// LFO1 Rate
	{ 0x00,	1,  0, 0x10003139,	    0,		0,	    127	},
	// Vibrato Rate
	{ 0x10,	1,  0, 0x10002118,	    0,		0,	    127	},
	// Vibrato Delay
	{ 0x01,	1,  0, 0x1000211a,	    0,		0,	    127	},
	// Vibrato Depth
	{ 0x11,	1,  0, 0x10002119,	    0,		0,	    127	},
	// LFO1 TVF Depth Offset
	{ 0x02,	1,  0, 0x1000313d,	    0,		1,	    127	},
	// TVF Env Depth Offset
	{ 0x03,	1,  0, 0x1000311a,	    0,		1,	    127	},
	// TVF Cutoff Keyfolow Offset
	{ 0x12,	1,  0, 0x10003115,	    0,	       44,	     84	},
	// Filter Type
	{ 0x13,	1,  0, 0x10003113,	    0,	        0,	      4	},
	// ADSR
	{ 0x04,	1,  0, 0x10003132,	    0,		0,	    127,
							juno_adsr_callback},
	{ 0x05,	1,  0, 0x10003134,	    0,		0,	    127,
							juno_adsr_callback},
	{ 0x06,	1,  0, 0x10003138,	    0,		0,	    127,
							juno_adsr_callback},
	{ 0x07,	1,  0, 0x10003135,	    0,		0,	    127,
							juno_adsr_callback},
	{ 0x04,	1,  0, 0x10003120,	    0,		0,	    127,
							juno_adsr_callback},
	{ 0x05,	1,  0, 0x10003122,	    0,		0,	    127,
							juno_adsr_callback},
	{ 0x06,	1,  0, 0x10003127,	    0,		0,	    127,
							juno_adsr_callback},
	{ 0x07,	1,  0, 0x10003123,	    0,		0,	    127,
							juno_adsr_callback},
	// Cutoff
	{ 0x14,	1,  0, 0x10003114,	    0,	        1,	    127	},
	// Resonance
	{ 0x15,	1,  0, 0x10003118,	    0,	        0,	    127	},
	// Pitch Bend Sens
	{ 0x16,	1,  0, 0x1000210f,	    0,	        0,	     24	},
	// Portamento Time
	{ 0x17,	1,  0, 0x10002111,	    0,	        0,	    127	},
	// Portamento Switch
	{ 0x37,	1,  1, 0x10002110,	    0,		0,	      1	},
	// Mono/Poly Switch
	{ 0x36,	1,  1, 0x1000210d,	    0,		0,	      1	},
	// Legato Switch
	{ 0x35,	1,  1, 0x1000210e,	    0,		0,	      1	},
	// LFO1 Key Trigger
	{ 0x30,	1,  1, 0x1000313b,	    0,		0,	      1	},
	{ -1 }
};

static struct controller juno_106_2[] = {
	/* R	S   L	Sysex Add    CTL Add	    Min		Max */
	// LFO1 Rate
	{ 0x00,	1,  0, 0x10003239,	    0,		0,	    127	},
	// Vibrato Rate
	{ 0x10,	1,  0, 0x10002218,	    0,		0,	    127	},
	// Vibrato Delay
	{ 0x01,	1,  0, 0x1000221a,	    0,		0,	    127	},
	// Vibrato Depth
	{ 0x11,	1,  0, 0x10002219,	    0,		0,	    127	},
	// LFO1 TVF Depth Offset
	{ 0x02,	1,  0, 0x1000323d,	    0,		1,	    127	},
	// TVF Env Depth Offset
	{ 0x03,	1,  0, 0x1000321a,	    0,		1,	    127	},
	// TVF Cutoff Keyfolow Offset
	{ 0x12,	1,  0, 0x10003215,	    0,	       44,	     84	},
	// Filter Type
	{ 0x13,	1,  0, 0x10003213,	    0,	        0,	      4	},
	// ADSR
	{ 0x04,	1,  0, 0x10003232,	    0,		0,	    127,
							juno_adsr_callback},
	{ 0x05,	1,  0, 0x10003234,	    0,		0,	    127,
							juno_adsr_callback},
	{ 0x06,	1,  0, 0x10003238,	    0,		0,	    127,
							juno_adsr_callback},
	{ 0x07,	1,  0, 0x10003235,	    0,		0,	    127,
							juno_adsr_callback},
	{ 0x04,	1,  0, 0x10003220,	    0,		0,	    127,
							juno_adsr_callback},
	{ 0x05,	1,  0, 0x10003222,	    0,		0,	    127,
							juno_adsr_callback},
	{ 0x06,	1,  0, 0x10003227,	    0,		0,	    127,
							juno_adsr_callback},
	{ 0x07,	1,  0, 0x10003223,	    0,		0,	    127,
							juno_adsr_callback},
	// Cutoff
	{ 0x14,	1,  0, 0x10003214,	    0,	        1,	    127	},
	// Resonance
	{ 0x15,	1,  0, 0x10003218,	    0,	        0,	    127	},
	// Pitch Bend Sens
	{ 0x16,	1,  0, 0x1000220f,	    0,	        0,	     24	},
	// Portamento Time
	{ 0x17,	1,  0, 0x10002211,	    0,	        0,	    127	},
	// Portamento Switch
	{ 0x37,	1,  1, 0x10002210,	    0,		0,	      1	},
	// Mono/Poly Switch
	{ 0x36,	1,  1, 0x1000220d,	    0,		0,	      1	},
	// Legato Switch
	{ 0x35,	1,  1, 0x1000220e,	    0,		0,	      1	},
	// LFO1 Key Trigger
	{ 0x30,	1,  1, 0x1000323b,	    0,		0,	      1	},
	{ -1 }
};

static struct controller juno_106_3[] = {
	/* R	S   L	Sysex Add    CTL Add	    Min		Max */
	// LFO1 Rate
	{ 0x00,	1,  0, 0x10003339,	    0,		0,	    127	},
	// Vibrato Rate
	{ 0x10,	1,  0, 0x10002318,	    0,		0,	    127	},
	// Vibrato Delay
	{ 0x01,	1,  0, 0x1000231a,	    0,		0,	    127	},
	// Vibrato Depth
	{ 0x11,	1,  0, 0x10002319,	    0,		0,	    127	},
	// LFO1 TVF Depth Offset
	{ 0x02,	1,  0, 0x1000333d,	    0,		1,	    127	},
	// TVF Env Depth Offset
	{ 0x03,	1,  0, 0x1000331a,	    0,		1,	    127	},
	// TVF Cutoff Keyfolow Offset
	{ 0x12,	1,  0, 0x10003315,	    0,	       44,	     84	},
	// Filter Type
	{ 0x13,	1,  0, 0x10003313,	    0,	        0,	      4	},
	// ADSR
	{ 0x04,	1,  0, 0x10003332,	    0,		0,	    127,
							juno_adsr_callback},
	{ 0x05,	1,  0, 0x10003334,	    0,		0,	    127,
							juno_adsr_callback},
	{ 0x06,	1,  0, 0x10003338,	    0,		0,	    127,
							juno_adsr_callback},
	{ 0x07,	1,  0, 0x10003335,	    0,		0,	    127,
							juno_adsr_callback},
	{ 0x04,	1,  0, 0x10003320,	    0,		0,	    127,
							juno_adsr_callback},
	{ 0x05,	1,  0, 0x10003322,	    0,		0,	    127,
							juno_adsr_callback},
	{ 0x06,	1,  0, 0x10003327,	    0,		0,	    127,
							juno_adsr_callback},
	{ 0x07,	1,  0, 0x10003323,	    0,		0,	    127,
							juno_adsr_callback},
	// Cutoff
	{ 0x14,	1,  0, 0x10003314,	    0,	        1,	    127	},
	// Resonance
	{ 0x15,	1,  0, 0x10003318,	    0,	        0,	    127	},
	// Pitch Bend Sens
	{ 0x16,	1,  0, 0x1000230f,	    0,	        0,	     24	},
	// Portamento Time
	{ 0x17,	1,  0, 0x10002311,	    0,	        0,	    127	},
	// Portamento Switch
	{ 0x37,	1,  1, 0x10002310,	    0,		0,	      1	},
	// Mono/Poly Switch
	{ 0x36,	1,  1, 0x1000230d,	    0,		0,	      1	},
	// Legato Switch
	{ 0x35,	1,  1, 0x1000230e,	    0,		0,	      1	},
	// LFO1 Key Trigger
	{ 0x30,	1,  1, 0x1000333b,	    0,		0,	      1	},
	{ -1 }
};

static struct controller super_filter[] = {
	/* R	S   L	Sysex Add    CTL Add	    Min		Max */
	// LFO Rate
	{ 0x00,	1,  0, 0x10000835,	    0,	    32768,	  32789 },
	// Filter type
	{ 0x44,	1,  0, 0x10000811,	    0,	    32768,	  32768,
							    update_states_cb },
	{ 0x46,	1,  1, 0x10000811,	    0,	    32769,	  32769,
							    update_states_cb },
	{ 0x45,	1,  1, 0x10000811,	    0,	    32770,	  32770,
							    update_states_cb },
	{ 0x47,	1,  1, 0x10000811,	    0,	    32771,	  32771,
							    update_states_cb },
	// Filter slope
	{ 0x33,	1,  0, 0x10000815,	    0,	    32768,	  32768,
							    update_states_cb },
	{ 0x43,	1,  0, 0x10000815,	    0,	    32769,	  32769,
							    update_states_cb },
	{ 0x34,	1,  0, 0x10000815,	    0,	    32770,	  32770,
							    update_states_cb },
	// Filter Cutoff
	{ 0x14,	1,  0, 0x10000819,	    0,	    32768,	  32895 },
	// Filter Resonance
	{ 0x15,	1,  0, 0x1000081d,	    0,	    32768,	  32895 },
	// Filter Gain
	{ 0x10,	1,  0, 0x10000821,	    0,	    32768,	  32780 },
	// Modulation Switch
	{ 0x30,	1,  1, 0x10000825,	    0,	    32768,	  32769 },
	// LFO Wave
	{ 0x32,	1,  0, 0x10000829,	    0,	    32768,	  32768,
							    update_states_cb },
	{ 0x31,	1,  0, 0x10000829,	    0,	    32769,	  32769,
							    update_states_cb },
	{ 0x40,	1,  0, 0x10000829,	    0,	    32770,	  32770,
							    update_states_cb },
	{ 0x41,	1,  0, 0x10000829,	    0,	    32771,	  32771,
							    update_states_cb },
	{ 0x42,	1,  0, 0x10000829,	    0,	    32772,	  32772,
							    update_states_cb },
	// LFO Depth
	{ 0x02,	1,  0, 0x10000839,	    0,	    32768,	  32895 },
	// LFO attack
	{ 0x04,	1,  0, 0x1000083d,	    0,	    32768,	  32895 },
	{ -1 }
};

static struct controller solo_synth1[] = {
	/* R	S   L	Sysex Add    CTL Add	    Min		Max */
	// OSC1 Waveform
	{ 0x30,	1,  1, 0x4000000,	    0,		0,	      1	},
	// OSC1 Pulse Width
	{ 0x00,	1,  0, 0x4000001,	    0,		0,	    127	},
	// OSC1, Coarse Tune
	{ 0x10,	1,  0, 0x4000002,	    0,	       16,	    112	},
	// OSC2, Fine Tune
	{ 0x11,	1,  0, 0x4000003,	    0,	       14,	    114	},
	// OSC2 Waveform
	{ 0x32,	1,  1, 0x4000004,	    0,		0,	      1	},
	// OSC2 Pulse Width
	{ 0x02,	1,  0, 0x4000005,	    0,		0,	    127	},
	// OSC2 Coarse Tune
	{ 0x12,	1,  0, 0x4000006,	    0,	       16,	    112	},
	// OSC2 Fine Tune
	{ 0x13,	1,  0, 0x4000007,	    0,	       14,	    114	},
	// OSC2 Level
	{ 0x03,	1,  0, 0x4000008,	    0,		0,	    127	},
	// OSC Sync Switch
	{ 0x40,	1,  1, 0x4000009,	    0,		0,	      1	},
	// Filter Type
	{ 0x15,	1,  0, 0x400000a,	    0,		0,	      4	},
	// Cutoff
	{ 0x17,	1,  0, 0x400000b,	    0,		0,	    127	},
	// Resonance
	{ 0x16,	1,  0, 0x400000c,	    0,		0,	    127	},
	// Level
	{ 0x07,	1,  0, 0x400000d,	    0,		0,	    127	},
	// Chorus Send Level
	{ 0x06,	1,  0, 0x400000e,	    0,		0,	    127	},
	// Reverb Send Level
	{ 0x05,	1,  0, 0x400000f,	    0,		0,	    127	},
	{ -1 }
};

static struct controller solo_synth2[] = {
	/* R	S   L	Sysex Add    CTL Add	    Min		Max */
	// LFO Rate
	{ 0x00,	1,  0, 0x4000010,	    0,		0,	    127	},
	// LFO OSC1 Pitch Depth
	{ 0x01,	1,  0, 0x4000011,	    0,		0,	    127	},
	// LFO OSC2 Pitch Depth
	{ 0x02,	1,  0, 0x4000012,	    0,		0,	    127	},
	// LFO OSC1 Pulse Width Depth
	{ 0x11,	1,  0, 0x4000013,	    0,		0,	    127	},
	// LFO OSC2 Pulse Width Depth
	{ 0x12,	1,  0, 0x4000014,	    0,		0,	    127	},
	// Range
	{ 0x10,	1,  0, 0x4000015,	    0,		0,	      2	},
	// Filter Type
	{ 0x15,	1,  0, 0x400000a,	    0,		0,	      4	},
	// Cutoff
	{ 0x17,	1,  0, 0x400000b,	    0,		0,	    127	},
	// Resonance
	{ 0x16,	1,  0, 0x400000c,	    0,		0,	    127	},
	// Level
	{ 0x07,	1,  0, 0x400000d,	    0,		0,	    127	},
	// Chorus Send Level
	{ 0x06,	1,  0, 0x400000e,	    0,		0,	    127	},
	// Reveb Send Level
	{ 0x05,	1,  0, 0x400000f,	    0,		0,	    127	},
	{ -1 }
};

void solo_synth_note1(unsigned char note) {
	// OSC1, Coarse Tune
	libgieditor_send_sysex_value(0x4000002,
		libgieditor_get_sysex_size(0x4000002), note);
}

void solo_synth_note2(unsigned char note) {
	// OSC2, Coarse Tune
	libgieditor_send_sysex_value(0x4000006,
		libgieditor_get_sysex_size(0x4000006), note);
}

static note_forwarder get_current_note_forwarder(void) {
	switch (current_control_set) {
	    case 0x26:
		return solo_synth_note1;
	    case 0x27:
		return solo_synth_note2;
	    default:
		return NULL;
	}
}

static struct controller *get_current_controller(void) {
	switch (current_control_set) {
	    case 0x26:
		return solo_synth1;
	    case 0x27:
		return solo_synth2;
	    case 0x20:
		return juno_106_0;
	    case 0x21:
		return juno_106_1;
	    case 0x22:
		return juno_106_2;
	    case 0x23:
		return juno_106_3;
	    case 0x24:
		return super_filter;
	    default:
		return NULL;
	}
}

static void translate_control(uint8_t control, uint8_t value) {
	struct controller *cur_controller;
	uint32_t send_value;

	cur_controller = get_current_controller();
	if (!cur_controller) return;

	while (cur_controller->respond_to != -1) {
	    if (cur_controller->respond_to != control) goto ignore;

	    if (control_type[control/16] == CTL_BUTTON) {
		if (cur_controller->latch && value == 0x00) goto ignore;
		if (cur_controller->latch && value == 0x7F) {
		    if (cur_controller->state == 0x00)
			cur_controller->state = 0x7F;
		    else
			cur_controller->state = 0x00;
		    value = cur_controller->state;
		}
		send_one_midiled(control, value);
	    }

	    send_value = (value * ((cur_controller->max_value -
			    cur_controller->min_value) / 127.0)) +
			    cur_controller->min_value;

	    if (cur_controller->is_sysex) {
		libgieditor_send_sysex_value(cur_controller->sysex_addr,
			libgieditor_get_sysex_size(cur_controller->sysex_addr),
			send_value);
	    } else {
		send_one_midictl(cur_controller->ctl_addr, send_value);
	    }

	    if (cur_controller->callback)
		    cur_controller->callback(cur_controller);

ignore:
	    cur_controller++;
	}
}

static void update_states(void) {
	uint8_t *data;
	uint32_t sysex_size;
	uint32_t sysex_val;
	int retval;
	struct controller *cur_controller;
	
	cur_controller = get_current_controller();
	if (!cur_controller) return;

	while (cur_controller->respond_to != -1) {
	    if (control_type[cur_controller->respond_to/16] != CTL_BUTTON)
		goto ignore;

	    if (cur_controller->is_sysex) {
		sysex_size = libgieditor_get_sysex_size(
				cur_controller->sysex_addr);
		/* Give up the lock otherwise we might wait too long */
		pthread_mutex_unlock(&midi_lock);
		retval = libgieditor_get_sysex(cur_controller->sysex_addr,
			sysex_size, &data);
		pthread_mutex_lock(&midi_lock);
		if (retval < 0) goto ignore;
		sysex_val = libgieditor_get_sysex_value(data, sysex_size);

		if (cur_controller->max_value == cur_controller->min_value) {
		    cur_controller->state = 
			    (sysex_val - cur_controller->min_value) ? 0 : 127;
		} else {
		    cur_controller->state = 127 *
			(sysex_val - cur_controller->min_value) / (float)
			(cur_controller->max_value - cur_controller->min_value);
		}

		free(data);
	    }
ignore:
	    cur_controller++;
	}
}

static int process_one_control(void) {
	Midictl_list cur_midictl;
	enum controller_types type;
	int changed = 0;

	pthread_mutex_lock(&midi_lock);

	while (midictl_in_list == NULL) {
	    pthread_cond_wait(&read_data_ready, &midi_lock);
	}

	cur_midictl = midictl_in_list;
	midictl_in_list = midictl_in_list->next;

	if (cur_midictl->control/16 > CONTROL_TYPES) goto unknown_type;

	type = control_type[cur_midictl->control/16];
	switch(type) {
	    case SET_BUTTON:
		if ((cur_midictl->control & 0x0f) > 0x7) break;
		send_one_midiled(current_control_set, 0x00);
		if (current_control_set != cur_midictl->control) changed = 1;
		current_control_set = cur_midictl->control;
		send_one_midiled(current_control_set, 0x7f);
		if (changed) {
		    reset_controller();
		    update_states();
		    printf("Current control set %i\n", current_control_set);
		}
		break;
	    case CTL_BUTTON:
	    case SLIDER:
	    case KNOB:
		translate_control(cur_midictl->control, cur_midictl->value);
		break;
	}

unknown_type:
	free(cur_midictl);

        pthread_mutex_unlock(&midi_lock);

	return 0;
}

static void *process_one_note(void *dummy) {
	Midictl_list cur_midictl;
	note_forwarder cur_forwarder;

	while(1) {
	    pthread_mutex_lock(&midi_lock);

	    while (midictl_note_list == NULL) {
		pthread_cond_wait(&note_data_ready, &midi_lock);
	    }

	    cur_midictl = midictl_note_list;
	    midictl_note_list = midictl_note_list->next;

	    pthread_mutex_unlock(&midi_lock);

	    cur_forwarder = get_current_note_forwarder();

	    if (cur_forwarder) cur_forwarder(cur_midictl->control);

	    free(cur_midictl);
	}

	return dummy;
}

static void signal_handler(int unused) {
	if (libgieditor_close()) {
		printf("Error closing libgieditor\n");
	}
	jack_close();
	exit(0);
}

static void *blinker(void *dummy) {
	static uint8_t val;
	struct controller *cur_controller;
	while (1) {
	    if (val) val = 0;
	    else val = 0x7f;
	    cur_controller = get_current_controller();
	    if (!cur_controller) goto wait;
	    while (cur_controller->respond_to != -1) {
		if (control_type[cur_controller->respond_to/16] != CTL_BUTTON)
		    goto next_controller;
		if (cur_controller->state > 0x40) {
		    send_one_midiled(cur_controller->respond_to,
				    cur_controller->state);
		    goto next_controller;
		}
		pthread_mutex_lock(&midi_lock);
		send_one_midiled(cur_controller->respond_to, val);
		pthread_mutex_unlock(&midi_lock);
next_controller:
		cur_controller++;
	    }
wait:
	    usleep(300000);
	}
	return dummy;
}

int main(void) {
	pthread_t blink_thread;
	pthread_t note_thread;
	if (libgieditor_init(CLIENT_OUT_NAME,
				LIBGIEDITOR_WRITE | LIBGIEDITOR_READ) < 0) {
            fprintf(stderr, "Library initialisation failed, aborting\n");
            fprintf(stderr, "Check that jackd is running.\n");
            exit(1);
        }
	jack_init(CLIENT_IN_NAME);
	signal(SIGINT, signal_handler);
	pthread_create(&blink_thread, NULL, blinker, NULL);
	pthread_create(&note_thread, NULL, process_one_note, NULL);
	while (1) process_one_control();
}
