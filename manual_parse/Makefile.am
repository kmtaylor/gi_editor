include $(top_srcdir)/common/common.am

AM_YFLAGS = -d

noinst_PROGRAMS = manual_parse

manual_parse_CPPFLAGS = $(AM_CPPFLAGS) -DUSE_LOG=1 \
	-DMIDI_ADDRESSES=$(top_srcdir)/manual_parse/midi_addresses.txt
manual_parse_SOURCES = dump_tree.c manual_parser.y manual_scanner.l \
		       class_map.c
manual_parse_LDADD = $(top_srcdir)/common/libcommon.la

PDFFLAGS = -raw -nopgbrk

noinst_DATA = midi_addresses.txt
EXTRA_DIST = addresses.patch addresses_jupiter.patch class_map_jupiter.c \
	     midi_tree.h range_entry.c trim.sh

midi_addresses.txt: $(top_srcdir)/midi_manual.pdf
	$(PDFTOTEXT) $^ tmp.txt $(PDFFLAGS)
	./trim.sh tmp.txt $@
	$(PATCH) midi_addresses.txt addresses.patch
	$(RM) tmp.txt

DISTCLEANFILES = manual_parser.c manual_parser.h manual_scanner.c midi.log \
		 midi_addresses.txt
