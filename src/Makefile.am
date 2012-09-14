include $(top_srcdir)/common/common.am

bin_PROGRAMS = read_midi sysex_explorer translator midi2jacksync

read_midi_SOURCES = read_midi.c
read_midi_LDADD = $(top_srcdir)/libgieditor/libgieditor.la \
		  $(top_srcdir)/common/libcommon.la \
		  $(GLIB_LIBS) $(JACK_LIBS)

sysex_explorer_SOURCES = sysex_explorer.c
sysex_explorer_LDADD = $(top_srcdir)/libgieditor/libgieditor.la \
		  $(top_srcdir)/common/libcommon.la \
		  $(GLIB_LIBS) $(JACK_LIBS) $(NCURSES_LIBS) \
		  $(PANEL_LIBS) $(FORM_LIBS) $(MENU_LIBS)

translator_SOURCES = translator.c
translator_LDADD = $(top_srcdir)/libgieditor/libgieditor.la \
		  $(top_srcdir)/common/libcommon.la \
		  $(GLIB_LIBS) $(JACK_LIBS)

midi2jacksync_SOURCES = midi2jacksync.c
midi2jacksync_LDADD = $(top_srcdir)/common/libcommon.la $(JACK_LIBS)

EXTRA_DIST = sysex_explorer.h korgnano.c
