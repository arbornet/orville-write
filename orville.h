/* Standard header for all Orville Write programs */

#ifndef ORVILLE_H
#define ORVILLE_H

#include "config.h"

#include <stdio.h>

#ifdef HAVE_FCNTL_H
# include <fcntl.h>
#endif

#ifdef STDC_HEADERS
# include <string.h>
# include <stdlib.h>
#else
# ifndef HAVE_STRCHR
#  define strchr index
#  define strrchr rindex
# endif
char *strchr(), *strrchr(), *getenv(), *malloc();
#endif

#ifdef HAVE_SYS_TIME_H
# include <sys/time.h>
#else
# include <time.h>
#endif

#ifdef HAVE_CRYPT_H
# include <crypt.h>
#endif

#ifdef HAVE_UNISTD_H
# include <unistd.h>
char *getpass();	/* Some unistd's don't define this - foo */
#else
char *getpass(), *ttyname();
# ifndef HAVE_CRYPT_H
char *crypt();
# endif
#endif

#include "wrttmp.h"
#include "lib_common.h"

typedef char bool;	/* Boolean type */
#define TRUE    1
#define FALSE   0

/* Method to set cbreak/cooked modes */

#ifdef HAVE_TERMIOS_H
# define F_TERMIOS
#else
# ifdef HAVE_TERMIO_H
#  define F_TERMIO
# else
#  ifdef HAVE_SGTTY_H
#   define F_STTY
#  endif /* HAVE_SGTTY_H */
# endif /* HAVE_TERMIO_H */
#endif /* HAVE_TERMIOS_H */

/* Random Number Generator */
#ifdef HAVE_RANDOM
# define RAND() random()
# define SEEDRAND(x) srandom(x)
# ifndef RAND_MAX
#  define RAND_MAX 2147483647
# endif
#else
# define RAND() rand()
# ifndef RAND_MAX
#  define RAND_MAX 32767
# endif
# define SEEDRAND(x) srand(x)
#endif /* HAVE_RANDOM */

#define JOB_CONTROL		/* support job-control */
#define USER_SHELL		/* shell escapes run with user's shell */

/* These are the tty modes to use when permissions are on and off */

#define PERMS_OFF 0600		/* To depermit everything */
#define PERMS_OPEN 0622		/* To allow cats to your device */
#ifdef TTY_OTHERS
# define PERMS_ON  PERMS_OPEN	/* To allow writes and talks and what not */
#else
# define PERMS_ON  0620		/* To allow writes and talks and what not */
#endif /*TTY_OTHERS*/

#endif /* ORVILLE_H */
