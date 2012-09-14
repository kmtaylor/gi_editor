/* Midi address dumper - requires roland manual
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
 * $Id: class_map_jupiter.c,v 1.1 2012/06/07 10:54:48 kmtaylor Exp $
 */


#include <stdio.h>
#include "midi_tree.h"

struct s_class_map class_map[] = {
        { .parent_name = "TOPLEVEL",
          .child_names = (const char * [])
		{ "Setup",				"Setup",
		  "System",				"System",
		  "Temporary Live Set (UPPER)",		"Live Set",
		  "Temporary Tone (UPPER Layer 1)",	"Temporary Tone",
		  "Temporary Tone (UPPER Layer 2)",     "Temporary Tone",
		  "Temporary Tone (UPPER Layer 3)",     "Temporary Tone",
		  "Temporary Tone (UPPER Layer 4)",     "Temporary Tone",
		  "Temporary Live Set (LOWER)",         "Live Set",
		  "Temporary Tone (LOWER Layer 1)",     "Temporary Tone",
		  "Temporary Tone (LOWER Layer 2)",     "Temporary Tone",
		  "Temporary Tone (LOWER Layer 3)",     "Temporary Tone",
		  "Temporary Tone (LOWER Layer 4)",     "Temporary Tone",
		  "Temporary Registration",		"Registration",
		  NULL
		}
        },
        { .parent_name = "System",
          .child_names = (const char * [])
		{ "System Common",			"System Common",
		  "System EQ",				"System Mastering",
		  "System Controller",			"System Controller",
		  NULL
		}
        },
        { .parent_name = "Temporary Tone",
          .child_names = (const char * [])
		{ "Temporary Synth Tone",		"Synth Tone",
		  NULL
		}
        },
        { .parent_name = "Live Set",
          .child_names = (const char * [])
                { "Live Set Common",			"Live Set Common",
                  "Live Set MFX1",			"Live Set MFX",
                  "Live Set Reverb",			"Live Set Reverb",
                  "Live Set MFX2",                      "Live Set MFX",
                  "Live Set MFX3",                      "Live Set MFX",
                  "Live Set MFX4",                      "Live Set MFX",
                  "Live Set Layer (Layer 1)",		"Live Set Layer",
                  "Live Set Layer (Layer 2)",           "Live Set Layer",
                  "Live Set Layer (Layer 3)",           "Live Set Layer",
                  "Live Set Layer (Layer 4)",           "Live Set Layer",
                  "Live Set Tone Offset (Layer 1)",	"Live Set Tone Offset",
                  "Live Set Tone Offset (Layer 2)",     "Live Set Tone Offset",
                  "Live Set Tone Offset (Layer 3)",     "Live Set Tone Offset",
                  "Live Set Tone Offset (Layer 4)",     "Live Set Tone Offset",
                  "Live Set Tone Modify (Layer 1)",     "Live Set Tone Modify",
                  "Live Set Tone Modify (Layer 2)",     "Live Set Tone Modify",
                  "Live Set Tone Modify (Layer 3)",     "Live Set Tone Modify",
                  "Live Set Tone Modify (Layer 4)",     "Live Set Tone Modify",
                  "Live Set Layer 2 (Layer 1)",		"Live Set Layer 2",
                  "Live Set Layer 2 (Layer 2)",         "Live Set Layer 2",
                  "Live Set Layer 2 (Layer 3)",         "Live Set Layer 2",
                  "Live Set Layer 2 (Layer 4)",         "Live Set Layer 2",
		  NULL
		}
        },
        { .parent_name = "Registration",
          .child_names = (const char * [])
                { "Registration Common",		"Registration Common",
                  "Registration Part (UPPER)",		"Registration Part",
                  "Registration Part (LOWER)",		"Registration Part",
                  "Registration Part (SOLO)",		"Registration Part",
                  "Registration Part (PERC)",		"Registration Part",
                  "Registration Ext Part (Part 1)",	"Registration Ext Part",
                  "Registration Ext Part (Part 2)",     "Registration Ext Part",
                  "Registration Ext Part (Part 3)",     "Registration Ext Part",
                  "Registration Ext Part (Part 4)",     "Registration Ext Part",
                  "Registration Ext Part (Part 5)",     "Registration Ext Part",
                  "Registration Ext Part (Part 6)",     "Registration Ext Part",
                  "Registration Ext Part (Part 7)",     "Registration Ext Part",
                  "Registration Ext Part (Part 8)",     "Registration Ext Part",
                  "Registration Ext Part (Part 9)",     "Registration Ext Part",
                  "Registration Ext Part (Part 10)",    "Registration Ext Part",
                  "Registration Ext Part (Part 11)",    "Registration Ext Part",
                  "Registration Ext Part (Part 12)",    "Registration Ext Part",
                  "Registration Ext Part (Part 12)",    "Registration Ext Part",
                  "Registration Ext Part (Part 14)",    "Registration Ext Part",
                  "Registration Ext Part (Part 15)",    "Registration Ext Part",
                  "Registration Ext Part (Part 16)",    "Registration Ext Part",
                  "Registration Controller",	      "Registration Controller",
                  "Registration Sub Part (SOLO)",	"Registration Sub Part",
                  "Registration Sub Part (PERC)",       "Registration Sub Part",
                  "Registration Sub Modify (SOLO)",   "Registration Sub Modify",
                  "Registration Sub Modify (PERC)",   "Registration Sub Modify",
                  "Registration Sub Effect (SOLO)",   "Registration Sub Effect",
                  "Registration Sub Effect (PERC)",   "Registration Sub Effect",
                  "Registration Sub Reverb",	      "Registration Sub Reverb",
		  NULL
		}
        },
        { .parent_name = "Synth Tone",
          .child_names = (const char * [])
		{ "Synth Tone Common",			"Tone Common",
		  "Synth Tone Partial (1)",		"Tone Partial",
		  "Synth Tone Partial (2)",             "Tone Partial",
		  "Synth Tone Partial (3)",             "Tone Partial",
		  NULL
		}
        },
        { .parent_name = "Setup",
          .child_names = (const char * []) { NULL }
        },
        { .parent_name = "System Common",
          .child_names = (const char * []) { NULL }
        },
        { .parent_name = "System Mastering",
          .child_names = (const char * []) { NULL }
        },
        { .parent_name = "System Controller",
          .child_names = (const char * []) { NULL }
        },
        { .parent_name = "Live Set Common",
          .child_names = (const char * []) { NULL }
        },
        { .parent_name = "Live Set MFX",
          .child_names = (const char * []) { NULL }
        },
        { .parent_name = "Live Set Reverb",
          .child_names = (const char * []) { NULL }
        },
        { .parent_name = "Live Set Layer",
          .child_names = (const char * []) { NULL }
        },
        { .parent_name = "Live Set Layer 2",
          .child_names = (const char * []) { NULL }
        },
        { .parent_name = "Live Set Tone Offset",
          .child_names = (const char * []) { NULL }
        },
        { .parent_name = "Live Set Tone Modify",
          .child_names = (const char * []) { NULL }
        },
        { .parent_name = "Registration Common",
          .child_names = (const char * []) { NULL }
        },
        { .parent_name = "Registration Part",
          .child_names = (const char * []) { NULL }
        },
        { .parent_name = "Registration Ext Part",
          .child_names = (const char * []) { NULL }
        },
        { .parent_name = "Registration Controller",
          .child_names = (const char * []) { NULL }
        },
        { .parent_name = "Registration Sub Part",
          .child_names = (const char * []) { NULL }
        },
        { .parent_name = "Registration Sub Modify",
          .child_names = (const char * []) { NULL }
        },
        { .parent_name = "Registration Sub Effect",
          .child_names = (const char * []) { NULL }
        },
        { .parent_name = "Registration Sub Reverb",
          .child_names = (const char * []) { NULL }
        },
        { .parent_name = "Tone Common",
          .child_names = (const char * []) { NULL }
        },
        { .parent_name = "Tone Partial",
          .child_names = (const char * []) { NULL }
        },
	{ NULL }
};
