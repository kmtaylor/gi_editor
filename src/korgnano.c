static void korgnano_reset(void) {
	int j, i;
	for (j = 0x20; j < 0x50; j += 0x10) {
	    for (i = 0; i < 8; i++) {
		send_one_midiled(i + j, 0x00);
	    }
	}
	send_one_midiled(0x29, 0x00);
	send_one_midiled(0x2a, 0x00);
	send_one_midiled(0x2b, 0x00);
	send_one_midiled(0x2c, 0x00);
	send_one_midiled(0x2d, 0x00);
}

#ifdef MIDI2JACKSYNC
static void korg_update_leds(enum e_status cur_status) {
	if (cur_status & TRANSPORT_RECORDING) {
	    send_one_midiled(0x2d, 0x7f);
	} else {
	    send_one_midiled(0x2d, 0x00);
	}
	if (cur_status & TRANSPORT_RUNNING) {
	    send_one_midiled(0x2a, 0x00);
	    send_one_midiled(0x29, 0x7f);
	} else {
	    send_one_midiled(0x29, 0x00);
	    send_one_midiled(0x2a, 0x7f);
	}
}

#define KORG_START_BUTTON_PRESSED(cur_midictl)	\
	if (cur_midictl->control == 0x2B && cur_midictl->value == 0x7F)
#define KORG_REC_BUTTON_PRESSED(cur_midictl)	\
	if (cur_midictl->control == 0x2D && cur_midictl->value == 0x7F)

#endif
