/* Header file for routines common between write, amin, mesg, etc */

#ifndef LIB_COMMON_H
#define LIB_COMMON_H

#include "getutent.h"

int init_wstream(int mode);
struct utmpx *find_utmp(char *tty);
void find_wrttmp(char *tty, time_t time,struct wrttmp *wbuf, long *pos);
void dflt_wrttmp(struct wrttmp *wbuf, char *tty, time_t time);
char *leafname(char *fullpath);
int mydevtty(void);

extern char *f_wrttmp, *f_wrthist, *f_novicehelp, *f_log,
	*f_helperlist, *f_nohelp, *f_helpername;
extern int f_disconnect, f_exceptions, f_helpers, f_loglevel, f_answertel,
	f_pipes, f_fromhost;
extern char *progname;
extern char mydevname[];
extern int wstream;
extern struct wrthdr wt_head;

#ifdef TTYPERMS
int saveperms(void);
void setperms(char perm);
void resetperms(void);
#endif /*TTYPERMS*/

#endif /* LIB_COMMON_H */
