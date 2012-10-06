/* Common functions
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
 * $Id: common.c,v 1.1.1.1 2012/06/06 03:01:54 kmtaylor Exp $
 */

#include <malloc.h>
#include <stdio.h>
#include <stdlib.h>
#include "log.h"

void *__common_allocate(size_t size, char *func_name) {
	void *temp = malloc(size);
	if (temp == NULL) {
		common_log(2, func_name, ": Out of memory");
		exit(1);
	}
	return temp;
}
