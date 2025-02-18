/* config.h.  Generated automatically by configure.  */
#ifndef CONFIG_H
#define CONFIG_H

/* Path name of configuration file. */
#define ORVILLE_CONF "/arbornet/etc/orville.conf"

/* Default location of wrttmp file. */
#define D_WRTTMP "/arbornet/etc/wrttmp"

/* Default location of wrthist file. */
#define D_WRTHIST "/arbornet/etc/wrthist"

/* Define if we want to be talk compatible. */
/* #undef WITH_TALK */

/* Define if password file lookups are extremely slow on your system */
/* #undef SLOWPASSWD */

/* Define if tty devices are normally permitted only to group */
#define TTY_GROUP 1

/* Define if tty devices are normally permitted to others */
/* #undef TTY_OTHERS */

/* Define to empty if the keyword does not work.  */
/* #undef const */

/* Define if you have <sys/wait.h> that is POSIX.1 compatible.  */
#define HAVE_SYS_WAIT_H 1

/* Define as the return type of signal handlers (int or void).  */
#define RETSIGTYPE void

/* Define if you have the ANSI C header files.  */
#define STDC_HEADERS 1

/* Define if you do password lookups with the getuserpw() function.  */
/* #undef HAVE_GETUSERPW */

/* Define if passwords in your shadow file are encrypted by pw_encrypt() */
/* #undef HAVE_PW_ENCRYPT */

/* Define if passwords in your shadow file are encrypted by kg_pwhash() */
/* #undef HAVE_KG_PWHASH */

/* Define if your <sys/time.h> declares struct tm.  */
/* #undef TM_IN_SYS_TIME */

/* Define if you do password lookups with the getspnam() function.  */
/* #undef HAVE_GETSPNAM */

/* Define if you have the random() function.  */
#define HAVE_RANDOM 1

/* Define if you have the strchr() function.  */
#define HAVE_STRCHR 1

/* Define if you have the getutent() function.  */
/* #undef HAVE_GETUTENT */

/* Define if you have the <crypt.h> header file.  */
/* #undef HAVE_CRYPT_H */

/* Define if you have the <fcntl.h> header file.  */
#define HAVE_FCNTL_H 1

/* Define if you have the <paths.h> header file.  */
#define HAVE_PATHS_H 1

/* Define if you have the <sgtty.h> header file.  */
/* #undef HAVE_SGTTY_H */

/* Define if you have the <sys/time.h> header file.  */
#define HAVE_SYS_TIME_H 1

/* Define if you may include both time.h and sys/time.h */
#define TIME_WITH_SYS_TIME 1

/* Define if you have the <termio.h> header file.  */
/* #undef HAVE_TERMIO_H */

/* Define if you have the <termios.h> header file.  */
#define HAVE_TERMIOS_H 1

/* Define if you have the <unistd.h> header file.  */
#define HAVE_UNISTD_H 1

/* Define if you have the crypt library (-lcrypt).  */
#define HAVE_LIBCRYPT 1

/* Define if 'struct tm' has a tm_zone member */
#define HAVE_TM_ZONE 1

/* Define if we have external array 'tzname' */
/* #undef HAVE_TZNAME */

#endif /* CONFIG_H */
