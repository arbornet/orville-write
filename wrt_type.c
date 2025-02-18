/* WRITE TYPING ROUTINES -- This does the actual write conversation, letting
 * me type in characters and sending them to him.  This whole module is too
 * kludgy and needs a rewrite.
 */

#include "write.h"
#include <ctype.h>
#include <pwd.h>
#include <sys/stat.h>

#if TIME_WITH_SYS_TIME
#include <sys/time.h>
#include <time.h>
#else
#if HAVE_SYS_TIME_H
#include <sys/time.h>
#else
#include <time.h>
#endif /* HAVE_SYS_TIME_H */
#endif /* TIME_WITH_SYS_TIME */

extern char eof_char, bs_char, kill_char;
extern bool helpseeker;
bool holdline;		/* True if we don't want to send line till it is done */
struct passwd *getpwnam();

#ifndef HOTZONE
#define HOTZONE 9
#endif

#ifndef MAXLASTMESG
#define MAXLASTMESG 16000
#endif

FILE *recfp= NULL;
struct passwd *hispwd= NULL;

#define putb(ch) {putc(ch,histerm); if (recfp) putc(ch,recfp);}
#define putsb(st) {fputs(st,histerm); if (recfp) fputs(st,recfp);}

void backcol(int *col,int to);
void sendchar(int ch);
void sendline(char *ln, int start, int ind);
void sendbanner(void);
int isexcept(char *dir, int yesfile, char *login);


/* GETHISPWD - This routine ensures that the hispwd variable has been set.
 * Returns true if it fails.
 */

int gethispwd()
{
	if (hispwd != NULL) return (0);
	return ((hispwd= getpwnam(hisname)) == NULL);
}


/* The following routines are for fooling with tabs and other multi-column
 * characters.  Column numbers are from 0 to 79.
 *
 * TABCOL(c) - If the cursor was in column c, this returns the column it would
 * be in after hitting a tab.
 *
 * OUTCOL(str,i) - What column would the cursor appear in after typing the
 * string from str[0] through str[n-1]?
 */

#define tabcol(c) (((c)+8)&(-8))

int outcol(char *str, int n)
{
int i,c= 0;

	for (i= 0; i < n && str[i] != '\0'; i++)
        {
		if (!isascii(str[i]))
			c+= isprint(toascii(str[i])) ? 3 : 4;
		else if (str[i] == '\t')
			c= tabcol(c);
		else if (str[i] == '\b')
			c--;
		else if (!isprint(str[i]))
			c+= 2;
		else
			c++;
	}
	return(c);
}


/* DOWRITE -- Do the actual write conversation */

void dowrite()
{
int ch;
int ind= 0;		/* Current index into linebuf */
int start;		/* Index of 1st unprinted char (during holdline only) */
int col= 0;		/* Current column on screen */
int maxcol= get_cols();	/* Number of columns on screen (sort of) */
bool command= FALSE;	/* True if we are doing a shell escape */
bool connected;		/* True if the other person has replied (during
			   char_by_char mode only) */

    /* send him a banner, if we weren't writing him already */
    if (!nested_write())
	    sendbanner();

    connected= !char_by_char;	/* only character mode cares if connected */
    is_writing= TRUE;
    holdline= FALSE;

    /* Go into cbreak mode, if that's what is desired */
    if (char_by_char) cbreak(TRUE);

    /* Start copying text from my terminal to his */
    while((ch= getchar()) != EOF)
    {
	ch &= 0377;
	if (char_by_char)
	{
		/* Fix odd backspace */
		if (ch == bs_char)
			ch= '\b';

		/* low-grade word-wrap */
		if ((!holdline || !connected) &&
		    (ch == ' ' || ch == '\t') &&
		    col > maxcol-HOTZONE && col < maxcol)
			ch= '\n';
	}

	/* Echo the character in the line buffer */
	switch(ch)		
	{
	case '\n':
		if (char_by_char) putchar(ch);
		if (holdline)
		{
			if (command && ind > 0)
			{
				linebuf[ind]= '\000';
				dosystem(linebuf);
			}
			else
			{
				linebuf[ind]= '\n';
				sendline(linebuf,start,ind+1);
				start= 1;
			}
			col= ind= 0;
			continue;
		}
		col= ind= 0;
		break;
	case '\b':	/* BACKSPACE */
		if (char_by_char)
		{
			if (col == 0) continue;
			if (ind > 0) ind--;
			backcol(&col,outcol(linebuf,ind));
			continue;
		}
		break;
	case '\027':	/* ^W */
		if (char_by_char)
		{
			if (col == 0 || ind == 0) continue;

			while (ind > 0 && isspace(linebuf[ind-1]))
				ind--;

			while (ind > 0 && !isspace(linebuf[ind-1]))
				ind--;

			backcol(&col,outcol(linebuf,ind));
			continue;
		}
	case '\022':	/* ^R */
		if (char_by_char)
		{
			linebuf[ind]= '\000';
			printf("\n%s",linebuf);
			continue;
		}
	default:
		if (ind == 0)
		{
			if (!connected && char_by_char && he_replied())
				connected= TRUE;
			command= (ch == '!' || ch == '|' || ch == '&');
			holdline= (!connected || command || ch == ')');
			start= (ch == ')') ? 1 : 0;
		}
		/* Simulate function of ^D for char_by_char mode */
		if (ch == eof_char)
		{
			if (ind == 0)
				return;
			else
			{
				if (holdline)
				{
					linebuf[ind]= '\000';
					if (command && ind > 0)
					{
						putchar('\n');
						dosystem(linebuf);
						ind= 0;
					}
					else
					{
						sendline(linebuf,start,ind);
						start= ind;
					}
				}
				continue;
			}
		}
		if (ch == kill_char)
		{
			ind= 0;
			backcol(&col,0);
			continue;
		}
		if (ind < LBSIZE) linebuf[ind++]= ch;
		if (char_by_char)
		{
		int tch= ch;

			if (tch == '\t')
			{
				putchar(tch);
				col= tabcol(col);
				break;
			}
			if (!isascii(tch))
			{
				putchar('M');
				putchar('-');
				col+= 2;
				tch= toascii(tch);
			}
			if (!isprint(tch))
			{
				putchar('^');
				col++;
				if (tch == '\177')
					tch= '?';
				else
					tch+= '@';
			}
			putchar(tch);
			col++;
		}
		break;
	}
	
	if (!holdline)
		sendchar(ch);
    }
}

/* DOTELEGRAM -- Send a telegram */

void dotelegram(bool nl)
{
int ch;

    /* Ask the user for a message if we don't already have one */
    if (telmsg == NULL || telmsg[0] == '\0' || telmsg[0] == '\n')
    {
	if (nl) putchar('\n');
	printf("Msg: ");
	fgets(telmsg= linebuf, LBSIZE, stdin);
	if (telmsg[0] == '\n') done(0);
    }

    /* slow down if we are sending stuff too fast */
    if (f_wrthist != NULL)
    {
	sleep(check_flood());
	register_tel();
    }

    /* send him a banner, if we weren't writing him already */
    if (!nested_write())
	    sendbanner();

    /* Send the telegram */
    is_writing= TRUE;
    while (*telmsg != '\0')
	sendchar(*(telmsg++));
    if (rec_only)
	printf("SAVED\n");
    else
	printf("SENT\n");
}


/* BACKCOL - Erase back until the current column col is to */

void backcol(int *col,int to)
{
	for ( ; *col > to; (*col)--)
	{
	    putchar('\b');
	    putchar(' ');
	    putchar('\b');
	    if (!holdline)
		putsb("\b \b");
	}
	fflush(histerm);
}


/* SENDCHAR:  Send a character to his tty, expanding out control characters,
 * and storing a copy in the record file, if open.
 */

void sendchar(int ch)
{
	if (ch == '\n' || ch == '\b' || ch == '\t' || ch == '\r')
		putb(ch)
	else
	{
		if (!isascii(ch))
		{
			putsb("M-");
			ch= toascii(ch);
		}
		if (!isprint(ch))
		{
			putb('^');
			ch= ((ch == '\177') ? '?' : ch + '@');
		}
		putb(ch);
	}
	fflush(histerm);
}


/* SENDLINE:  Send a string to his tty, expanding out control characters.
 * Start at start and go to ind.  If ind is less than start, backspace to it.
 */

void sendline(char *ln, int start, int ind)
{
int i;

	if (start < ind)
		for (i= start; i < ind; i++)
			sendchar(ln[i]);
	else
	{
		for (i= start; i > ind; i--)
			putsb("\b \b");
		fflush(histerm);
	}
}


/* SENDBANNER:  Send a banner to him, notifying him that we want to talk
 */

void sendbanner()
{
time_t tock;
struct tm *tm;
char bf[300];
char *bell, *zone;
extern struct wrttmp hiswrt;
char host[1026];
#ifdef HAVE_TZNAME
extern char *tzname[2];
#endif

    host[0]= '@';
    if (f_fromhost && gethostname(host+1,1024))
    	f_fromhost= 0;

    tock= time((time_t *)0);
    tm= localtime(&tock);
#ifdef HAVE_TZNAME
    if (tm->tm_isdst >= 0)
    	zone= tzname[tm->tm_isdst];
    else
    	zone= NULL;
#else
#ifdef HAVE_TM_ZONE
    zone= (char *)tm->tm_zone;
#endif
#endif

    bell= (hiswrt.wrt_bells == 'y') ? "\007\007\007":"";
    if (myuidname[0] == '\0')
        sprintf(bf,"%s from %s%s%.90s on %s at %d:%02d %.20s ...\n%s",
	    telegram ? "Telegram" : "Message",
	    helpseeker ? "help-seeker " : "",
	    myname,
	    f_fromhost ? host : "",
	    mytty,
	    tm->tm_hour, tm->tm_min,
	    zone == NULL ? "" : zone,
	    bell);
    else
        sprintf(bf,
	    "%s from %s%s%.90s (%s) on %s at %d:%02d %.20s ...\n%s",
	    telegram ? "Telegram" : "Message",
	    helpseeker ? "help-seeker " : "",
	    myname,
	    f_fromhost ? host : "",
	    myuidname,
	    mytty,
	    tm->tm_hour, tm->tm_min,
	    zone == NULL ? "" : zone,
	    bell);

    fputs(bf,histerm);
    if (recfp) fputs(bf,recfp);
    fflush(histerm);
}


/* OPEN_RECORD - Open a record file in the target users' home directory */

void open_record()
{
char fname[LBSIZE], *dir, *his_dir();
extern struct wrttmp hiswrt;
int fd;
struct stat st;

    recfp= NULL;

    if (hiswrt.wrt_record == 'n')
	return;

    if (gethispwd())
    {
	printf("%s: cannot find %s in passwd file -- not recording.\n",
		progname,hisname);
	if (rec_only) done(1);
	return;
    }

    sprintf(fname,"%.400s/.lastmesg",hispwd->pw_dir);

    /* First try to open an existing file */
    if ((fd= open(fname, O_WRONLY|O_APPEND)) >= 0)
    {
	/* Check that .lastmesg file is owned by the right person */
	if (fstat(fd,&st) ||
	    st.st_uid != hispwd->pw_uid ||
	    !(st.st_mode & 0200))
	{
	    close(fd);
	    printf("%s: improper ownership of %s -- not recording.\n",
		    progname,fname);
	    if (rec_only) done(1);
	    return;
	}

	/* If we are recorded only most recent message, truncate file */
	if (hiswrt.wrt_record == 'y')
	{
	    ftruncate(fd,0L);
	    lseek(fd,0L,0);		/* rewind (probably unnecessary) */
	}
	else if (lseek(fd,0L,1) > MAXLASTMESG)	/* not a seek -- just tell */
	{
	    printf("%s: %s overfull -- not recording.\n",progname,fname);
	    if (rec_only) done(1);
	    return;
	}
    }
    else
    {
	/* Could not open existing file - try creating one */
	if ((fd= open(fname,O_WRONLY|O_CREAT|O_EXCL|O_APPEND,0600)) < 0)
	{
	    printf("%s: cannot open %s -- not recording.\n",
	    progname,fname);
	    if (rec_only) done(1);
	    return;
	}

	/* Set ownership of file to target user */
	fchown(fd,hispwd->pw_uid,hispwd->pw_gid);
    }

    fcntl(fd, F_SETFD, 1);	/* Close over execs */
    recfp= fdopen(fd,"a");
}


/* ISHISEXCEPTION - is <login> listed in the .yeswrite/.nowrite in HIS home
 * directory?
 *
 * ISUEXECPTION - is <login> listed in the .yeswrite/.nowrite in user's home
 * directory?
 */

int ishisexception(int yesfile, char *login)
{

	if (gethispwd()) return(0);
	return(isexcept(hispwd->pw_dir, yesfile, login));
}

int isuexception(char *user, int yesfile, char *login)
{
struct passwd *pwd;
char u[UT_NAMESIZE+1];

	/* Make sure the name is null terminated */
	strncpy(u, user, UT_NAMESIZE);
	u[UT_NAMESIZE]= '\0';

	if ((pwd= getpwnam(u)) == NULL) return(0);
	return(isexcept(pwd->pw_dir, yesfile, login));
}

int isexcept(char *dir, int yesfile, char *login)
{
char buf[LBSIZE+1];
char *b,*e;
FILE *fp;

	sprintf(buf, "%.400s/%s", dir, yesfile ? ".yeswrite" : ".nowrite");

	if ((fp= fopen(buf,"r")) == NULL) return(0);

	buf[LBSIZE]= '\0';
	while (fgets(buf,LBSIZE,fp) != NULL)
	{
		if (buf[0] == '#') continue;
		e= buf-1;
		for (;;)
		{
			/* Skip leading blanks and punctuation */
			for (b= e+1;
			     *b!='\0' && strchr(" ,;:\t\n\r",*b)!=NULL; b++)
			     	;
			if (*b == '\0') break;

			/* Find next blank or punctuation character */
			for (e= b+1;
			     *e!='\0' && strchr(" ,;:\t\n\r",*e)==NULL; e++)
			     	;
			*e= '\0';

			if (!strcmp(b,login))
			{
				fclose(fp);
				return(1);
			}
		}
	}
	fclose(fp);
	return(0);
}
