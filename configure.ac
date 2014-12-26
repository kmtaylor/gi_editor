AC_INIT([gi_editor], [0.1], [github.com/kmtaylor/gi_editor])
AC_CONFIG_AUX_DIR([config])
AM_INIT_AUTOMAKE([-Wall])
AM_SILENT_RULES
AM_SILENT_RULES([yes])
AM_PROG_AR
LT_INIT
AC_PROG_CC
AM_PROG_CC_C_O
AC_PROG_LEX
AC_PROG_YACC
AC_CONFIG_HEADERS([config/config.h])
AC_CONFIG_FILES([
    Makefile
    include/Makefile
    avr/Makefile
    common/Makefile
    manual_parse/Makefile
    libgieditor/Makefile
    src/Makefile
])
AC_CONFIG_MACRO_DIR([m4])

PKG_CHECK_MODULES([GLIB], [glib-2.0])
PKG_CHECK_MODULES([NCURSES], [ncurses])
PKG_CHECK_MODULES([PANEL], [panel])
PKG_CHECK_MODULES([FORM], [form])
PKG_CHECK_MODULES([MENU], [menu])

AC_CHECK_FUNC(pthread_create, [],
    AC_CHECK_LIB(pthread, pthread_create, [],
        AC_MSG_ERROR([*** gi_editor requires POSIX threads support])))

create_shared_lib=""
case "$host_os" in
    cygwin*)
	create_shared_lib="-no-undefined"
	AC_ARG_VAR([JACK_LIBS], [linker flags for jack])
	AC_ARG_VAR([JACK_CFLAGS], [C compiler flags for jack])
	JACK_LIBS="-L../Jack/lib -ljack -lpthread -lrt"
	JACK_CFLAGS="-I\$(top_srcdir)/Jack/includes"
	;;
    *) 
	PKG_CHECK_MODULES([JACK], [jack])
esac

AC_SUBST(create_shared_lib)

AC_ARG_VAR([PDFTOTEXT],
[Required for parsing the midi implementation manual. It seems that there might
be a difference between Xpdf's pdftotext and Poppler's implementation.])
AC_CHECK_PROG([PDFTOTEXT], [pdftotext], [pdftotext], [not found])

if test "x$PDFTOTEXT" = "xnot found"; then
    AC_MSG_ERROR([  pdftotext required for midi manual parsing.
		    Please install either poppler (preferable) or xpdf.])
fi

AC_CHECK_PROG([PATCH], [patch], [patch], [not found])

if test "x$PATCH" = "xnot found"; then
    AC_MSG_ERROR([Could not find patch.])
fi

AC_ARG_WITH(pkgconfdir,
    AS_HELP_STRING([--with-pkgconfdir=dir],
        [path where pkgconfig files are stored]),
    pkgconfdir="$withval", pkgconfdir="")
if test -z "$pkgconfdir"; then
    pkgconfdir="$libdir/pkgconfig"
fi
AC_DEFINE_UNQUOTED(PKGCONF_DIR, "$pkgconfdir",
		[directory containing pkgconfig files])
AC_SUBST(PKGCONF_DIR, "$pkgconfdir")

AC_OUTPUT(libgieditor/libgieditor.pc)
