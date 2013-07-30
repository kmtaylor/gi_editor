include $(top_srcdir)/common/common.am

noinst_LTLIBRARIES = libavr.la

libavr_la_SOURCES = avr_api.c node.c
libavr_la_LIBADD = $(top_srcdir)/common/libmidi.la

EXTRA_DIST = Makefile.avr bit_bang.c bit_bang.h control_node.c node.h \
	     per_node.h ATmega48-controller.sh
