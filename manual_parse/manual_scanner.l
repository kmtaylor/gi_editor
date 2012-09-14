/* Roland manual lexer
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
 * $Id: manual_scanner.l,v 1.3 2012/06/07 14:58:55 kmtaylor Exp $
 */

HEX     [0-9a-fA-F]+
FLOAT   "-"?[0-9]+\.[0-9]+
ID      [a-zA-Z_][A-Za-z_0-9]*
nl      [\n\f]
nonl    [^\n\f]
STR     \|\ [a-zA-Z()][A-Za-z_0-9()\-\ <>\*#/]*+\|

%{
#include "midi_tree.h"
#include "manual_parser.h"

int yywrap(void);
%}

%%

\|\ Start\ \|   { return START_TOKEN; }

\|		{ return BANG_TOKEN; }

^\*.*		{ 
		    yylval.str_val = strdup(yytext + 2);
                    return CLASS_TOKEN;
		}

\|#		{ return HASH_TOKEN; }

{HEX}           {
                    sscanf(yytext, "%x", &yylval.int_val);
                    return HEX_TOKEN;
                }

{FLOAT}         {
		    yylval.str_val = strdup(yytext);
                    return FLOAT_TOKEN;
                }


{STR}           {
                    yytext[strlen(yytext) - 2] = '\0';
		    yylval.str_val = strdup(yytext + 2);
                    return STRING_TOKEN;
                }

{ID}            {
                    yylval.str_val = strdup(yytext);
                    return IDENT_TOKEN;
                }

"#"{nonl}*{nl}  { ln++; }

{nl}            { ln++; return NEWLINE; }

[ \t]+          ; /* skip whitespace */

.               { return INVALID_TOKEN; }   /* will cause a syntax error */

%%

int yywrap(void) {
	return 1;
}
