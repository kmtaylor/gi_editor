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
 * $Id: log.h,v 1.2 2012/06/10 23:26:14 kmtaylor Exp $
 */

#ifndef LOGFILE
#define LOGFILE "/var/log/gi_log"
#endif

#ifndef USE_LOG
#define USE_LOG 0
#endif

extern void print_date_message(FILE *logfile);
extern void common_log(int lines, ...);
