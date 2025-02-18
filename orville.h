/* Standard header for all Orville Write programs */

#ifndef ORVILLE_H
#define ORVILLE_H

#include "config.h"

#include <fcntl.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "wrttmp.h"
#include "lib_common.h"

#define TRUE    true
#define FALSE   false

/* Method to set cbreak/cooked modes */

#define F_TERMIOS

/* Random Number Generator */
#define RAND() random()
#define SEEDRAND(x) srandom(x)

#define JOB_CONTROL		/* support job-control */
#define USER_SHELL		/* shell escapes run with user's shell */

/* These are the tty modes to use when permissions are on and off */

#define PERMS_OFF 0600		/* To depermit everything */
#define PERMS_OPEN 0622		/* To allow cats to your device */
#define PERMS_ON  0620		/* To allow writes and talks and what not */

#endif /* ORVILLE_H */
