/* Functions for writing to log file
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
 * $Id: log.c,v 1.1.1.1 2012/06/06 03:01:54 kmtaylor Exp $
 */

#include <stdio.h>
#include <time.h>
#include <assert.h>
#include <unistd.h>
#include <stdarg.h>

#include "log.h"

void print_date_message(FILE *logfile) {
	time_t now;
	time(&now);
	fprintf(logfile, "%s", ctime(&now));
}

void common_log(int lines, ...) {
	FILE *logfile;
	va_list va;
	char *message;
	va_start(va, lines);
	logfile = fopen(LOGFILE, "a");
	assert( logfile != NULL );
	print_date_message(logfile);
	while (lines > 0) {
		message = va_arg(va, char *);
		fprintf(logfile, "%s", message);
		lines--;
	}
	fprintf(logfile, "\n\n");
	fclose(logfile);
	va_end(va);
}
