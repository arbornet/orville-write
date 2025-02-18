/* WRITE TTY ROUTINES -- Routine to get user's tty in and out of cbreak mode.
 */

#include "write.h"

#ifdef F_TERMIO
#include <termio.h>
#endif /*F_TERMIO*/
#ifdef F_TERMIOS
#include <termios.h>
#endif /*F_TERMIOS*/
#ifdef F_STTY
#include <sgtty.h>
#endif /*F_STTY*/

#if !defined(TCFLSH) && !defined(TCIFLUSH)
#include <sys/file.h>
#endif


char eof_char;		/* User's end of file character */
char bs_char;		/* User's backspace character */
char kill_char;		/* User's kill character */


/* CBREAK:  cbreak(TRUE) turns on cbreak mode, cbreak(FALSE) restores the
 * original tty modes.
 */

#ifdef F_STTY
void cbreak(bool flag)
{
struct tchars ctrlchars;
static struct sgttyb sgtty;
static tfg;

    if (flag)
    {
	/* Get current modes */
	ioctl(0,TIOCGETP,&sgtty);
	tfg= sgtty.sg_flags;
	bs_char= sgtty.sg_erase;
	kill_char= sgtty.sg_kill;
	ioctl(0,TIOCGETC,&ctrlchars);
	eof_char= ctrlchars.t_eofc;

	/* Remember that we are in cbreak mode */
	in_cbreak= TRUE;

	/* Turn on cbreak mode */
	sgtty.sg_flags &= ~ECHO;
	sgtty.sg_flags |= CBREAK;
	ioctl(0,TIOCSETN,&sgtty);
    }
    else
    {
	/* Turn off cbreak mode - that is restore original modes */
	sgtty.sg_flags= tfg;
	ioctl(0,TIOCSETN,&sgtty);
	in_cbreak= FALSE;
    }
}
#endif /*F_STTY*/

#ifdef F_TERMIO
void cbreak(bool flag)
{
static struct termio tio;
static tfg;
static char eol_char;

    if (flag)
    {
	/* Get current modes */
	ioctl(0,TCGETA,&tio);
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
	ioctl(0,TCSETA,&tio);
    }
    else
    {
	/* Turn off cbreak mode - that is restore original modes */
	tio.c_lflag= tfg;
	tio.c_cc[VEOF]= eof_char;
	tio.c_cc[VEOL]= eol_char;
	ioctl(0,TCSETA,&tio);
	in_cbreak= FALSE;
    }
}
#endif /*F_TERMIO*/


#ifdef F_TERMIOS
void cbreak(bool flag)
{
static struct termios tio;
static tfg;
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
#endif /*F_TERMIOS*/

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
#ifdef TIOCGWINSZ
struct winsize x;

	ioctl(2,TIOCGWINSZ,&x);
	if (x.ws_col == 0 || x.ws_col > 80)
		return(80);
	else
		return(x.ws_col);
#else
	return(80);
#endif
}
