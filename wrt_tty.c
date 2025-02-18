/* WRITE TTY ROUTINES -- Routine to get user's tty in and out of cbreak mode.
 */

#include "write.h"

#include <termios.h>

#if !defined(TCFLSH) && !defined(TCIFLUSH)
#include <sys/file.h>
#endif


char eof_char;		/* User's end of file character */
char bs_char;		/* User's backspace character */
char kill_char;		/* User's kill character */


/* CBREAK:  cbreak(TRUE) turns on cbreak mode, cbreak(FALSE) restores the
 * original tty modes.
 */

void cbreak(bool flag)
{
static struct termios tio;
static int tfg;
static char eol_char;

    if (flag)
    {
	/* Get current modes */
	tcgetattr(0,&tio);
	tfg= tio.c_lflag;
	eof_char= tio.c_cc[VEOF];
	eol_char= tio.c_cc[VEOL];
	bs_char= tio.c_cc[VERASE];
	kill_char= tio.c_cc[VKILL];

	/* Remember that we are in cbreak mode */
	in_cbreak= TRUE;

	/* Turn on cbreak mode - that is turn off ICANON */
	tio.c_lflag= tfg & ~ICANON;
	tio.c_lflag &= ~ECHO;
	tio.c_cc[VEOF]= 1;
	tio.c_cc[VEOL]= 0;
	tcsetattr(0,TCSANOW,&tio);
    }
    else
    {
	/* Turn off cbreak mode - that is restore original modes */
	tio.c_lflag= tfg;
	tio.c_cc[VEOF]= eof_char;
	tio.c_cc[VEOL]= eol_char;
	tcsetattr(0,TCSANOW,&tio);
	in_cbreak= FALSE;
    }
}

/* FLUSHINPUT:  Flush input on tty on filedescriptor dev.
 */

void flushinput(int dev)
{
#ifdef TCIFLUSH
	tcflush(dev, TCIFLUSH);		/* TERMIOS way to flush input */
#else
#ifdef TCFLSH
	ioctl(dev,TCFLSH,0);      /* SysIII way to flush input */
#else
int flushcode= FREAD;

	ioctl(dev,TIOCFLUSH,&flushcode);  /* BSD way to flush input */
#endif /*TCFLSH*/
#endif /*TCIFLUSH*/
}


/* GET_COLS:  Return number of columns on user's terminal.  Sort of.  Since
 * this is going to be used for wrapping on both my terminal and his, we
 * won't let it be more than 80, since his terminal probably doesn't have
 * more than 80 columns even if ours does.
 */

int get_cols()
{
	struct winsize x;

	ioctl(2,TIOCGWINSZ,&x);
	if (x.ws_col == 0 || x.ws_col > 80)
		return(80);
	else
		return(x.ws_col);
}
