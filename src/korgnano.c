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

static void korgnano_func_reset(void) {
	int j, i;
        for (j = 0x30; j < 0x50; j += 0x10) {
            for (i = 0; i < 8; i++) {
                send_one_midiled(i + j, 0x00);
            }
        }
}

int korg_val_from_buttons(uint8_t button) {
	if (button >= 0x30 && button < 0x38) return button - 0x30;
	if (button >= 0x40 && button < 0x48) return button - 0x40 + 8;
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
	if (cur_midictl->control == 0x2E && cur_midictl->value == 0x7F)
#define KORG_REC_BUTTON_PRESSED(cur_midictl)	\
	if (cur_midictl->control == 0x2D && cur_midictl->value == 0x7F)
#define KORG_PLAY_BUTTON_PRESSED(cur_midictl)	\
	if (cur_midictl->control == 0x29 && cur_midictl->value == 0x7F)
#define KORG_STOP_BUTTON_PRESSED(cur_midictl)	\
	if (cur_midictl->control == 0x2A && cur_midictl->value == 0x7F)
#define KORG_BACK_BUTTON_PRESSED(cur_midictl)	\
	if (cur_midictl->control == 0x2B && cur_midictl->value == 0x7F)
#define KORG_FORWARD_BUTTON_PRESSED(cur_midictl)	\
	if (cur_midictl->control == 0x2C && cur_midictl->value == 0x7F)
#define KORG_BACK8_BUTTON_PRESSED(cur_midictl)	\
	if (cur_midictl->control == 0x3A && cur_midictl->value == 0x7F)
#define KORG_FORWARD8_BUTTON_PRESSED(cur_midictl)	\
	if (cur_midictl->control == 0x3B && cur_midictl->value == 0x7F)

#endif
