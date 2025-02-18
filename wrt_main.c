/*		suid WRITE command		Version 2.50
 *
 * This is a replacement for the unix 'write' command with a large number of
 * enhancements.  See the manual page for details.
 *
 * (C) Copyright, Nov 1995 - Jan Wolter
 *
 *  This program may be used, distributed and modified free of charge.
 *
 * History has been moved to "CHANGES" file.
 *
 * Acknowledgements:  Russ Cage first implemented a version of the disconnect
 *   option, but his version of the source was lost.  The reimplementation here
 *   is probably similar to his.  The idea of helpers more or less originated
 *   with Eoin Cain.  The telegram command was originally a separate command
 *   implemented in C by Jeff Spindler and improved by Russ Cage.  Discussions
 *   with Marcus Watts were helpful in the design of the TTY_GROUP version.
 *   The record options were inspired by Jef Poskanzer's "say" command.
 *   Jim Knight installed zillions of versions on M-Net and added logging.
 *   Valerie Mates installed zillions of versions on Grex and sent back lots
 *   of valuable bug reports and suggestions.  Jared Mauch supplied some
 *   Linux compatibility fixes.  Input from John Remmers was useful in the
 *   design of the exception files.  Shane Wegner made some patches to support
 *   use of getutent() and various installation scripts.  Karyl Stein has
 *   supplied various good Linux bug fixes and reports.  M-Netters and
 *   Grexers by the thousands have served (willy-nilly) as beta testers.
 */

char *version= "2.55";

#include "write.h"
#include <sys/resource.h>

char linebuf[LBSIZE+1];	/* A place to stash lines of typed in stuff */
char *telmsg;		/* Index of second word of telegram text in linebuf */

char myname[UT_NAMESIZE+1]= "";	/* my name (based on utmp file) */
char myuidname[UT_NAMESIZE+1];	/* my name (based on uid number) */
char *mytty;			/* my tty name in "tty##" format */
#ifdef TTYPERMS
int myperms;			/* my original tty perms */
#endif /*TTYPERMS*/

FILE *histerm;		/* Opened version of device to write to */
struct wrttmp hiswrt;	/* His wrttmp entry - later used for my tmp wrttmp */
char hisname[UT_NAMESIZE+1]= "";/* his name */
char histty[UT_LINESIZE+1]= "";	/* his tty name in "tty##" format */
char hisdevname[UT_LINESIZE+7];	/* Name of device to write to */

/* Mode flags are set by run time options */

bool char_by_char=FALSE;/* Run in character by character mode */
bool ask_root= FALSE;	/* Ask for root password? */
bool telegram= FALSE;	/* send a telegram? */
bool no_switch= FALSE;	/* don't switch between tel and write? */
char tmp_mesg='s';	/* How to set messages while running: y, n or s */
bool postpone= FALSE;	/* Postpone all tel interuptions */

bool am_root;		/* Either uid is 0, or gave root passwd */
bool rec_only= FALSE;	/* Do a postponed telegram, recorded, but not sent */

/* Exit flags tell what to do to restore sanity upon exiting */

bool fixed_modes= FALSE;	/* On exiting, change wrttmp to mywrt? */
bool in_cbreak= FALSE;		/* On exiting, get out of cbreak mode? */
bool is_writing= FALSE;		/* On exiting, print goodbye message? */
bool insys= FALSE;		/* Am I in | or & escape? */


bool readyn(void);
void type_help(char *file);
void wrtlog(char *outcome);


main(int argc, char **argv)
{
long pos;
struct wrttmp tmpwrt;
char hisperms;
extern struct wrttmp mywrt;
bool nl,file_input,replying= 0;
struct rlimit rlim;
char *tty;
int tmp;

    /* Turn off signals while we are starting up - I don't remember why */
    /* On the whole, probably a good idea */
    sigoff();

    /* Check that stdin/stdout/stderr all exist and are open (from mdw) */
    if ((tmp=dup(0)) < 3)
    {
	/* We haven't got stderr, so don't print an error message. */
	exit(25);
    }
    close(tmp);

#ifdef RLIMIT_CORE
    /* Disable core dumps -- may have part of shadow password file in memory */
    rlim.rlim_cur= rlim.rlim_max= 0;
    setrlimit(RLIMIT_CORE, &rlim);
#endif

    /* Set up options */
    default_opts(argv[0]);	/* Read options from orville.conf */
    user_opts(argc,argv);	/* Read options from command line */

    file_input= !isatty(0);
    if (!telegram && !file_input && f_novicehelp != NULL && getenv("NOVICE"))
	    type_help(f_novicehelp);

    /* If -f has been specified, make sure standard input is a tty */
    if (!f_pipes && file_input)
    {
	printf("%s: input from non-tty disabled\n",progname);
	done(1);
    }

    /* Never allow standard output to be redirected - discourages tel bombs */
    if (!isatty(1))
    {
	fprintf(stderr,"%s: standard output may not be redirected\n",progname);
	done(1);
    }

    /* Get user's tty name safely */
    if (getdevtty()) done(1);
    mytty= mydevname+5;
#ifdef TTYPERMS
    if (saveperms()) done(1);
#endif /*TTYPERMS*/
    /* Set terminators on various names */
    myname[UT_NAMESIZE]= hisname[UT_NAMESIZE]= histty[UT_LINESIZE]= '\0';

    /* Open the wrttmp file */
    if (init_wstream(O_RDWR))
	done(1);

    /* Set up ttynames and usernames and get our wrttmp entries */
    find_us();
    if (telegram) fflush(stdout);
    nl= telegram;

    if (!iswritable())
    {
	if (telegram) putchar('\n');
	printf("Permission denied: %s is not accepting messages\n",hisname);
	wrtlog("FAIL: denied");
	done(1);
    }

    /* If he is busy, confirm that we want to interupt him */
    if (*hiswrt.wrt_what != '\000' &&
	!(replying= !strncmp(hiswrt.wrt_what,myname,UT_NAMESIZE)))
    {
	/* his mesgs are on, but he is writing someone else ... */
	if (nl) putchar('\n');
	nl= FALSE;
	if (hiswrt.wrt_what[0] == '!')
	    printf("%s is running %0.*s\n",hisname,
		   UT_NAMESIZE-1, hiswrt.wrt_what+1);
	else
	    printf("%s is now writing %0.*s.\n",
		   hisname, UT_NAMESIZE, hiswrt.wrt_what);
	if (rec_only || (telegram && hiswrt.wrt_record == 'a'))
	{
	    telegram= TRUE;
	    rec_only= TRUE;
	}
	if (rec_only)
	    printf("Do you want to leave %s message for when %s is done? ",
		(telmsg == NULL || telmsg[0] == '\0' || telmsg[0] == '\n') ?
		  "a" : "your", hisname);
	else
	    printf("Do you still want to write %s? ",hisname);
	fflush(stdout);
	if (!readyn())
	{
		wrtlog("ABANDON: interupting");
		done(1);
	}
    }

    /* Switch from tel to write (or vice versa) on recipient's preference */
    if (!(am_root && no_switch) && !rec_only &&
	(((telegram || file_input) && !(hiswrt.wrt_telpref & TELPREF_TEL)) ||
         (!telegram && !(hiswrt.wrt_telpref & TELPREF_WRITE) && !replying)))
    {
	if (nl) putchar('\n');
	nl= FALSE;
	printf("%s is not accepting %ss.\n",
		hisname, (telegram||file_input)?"telegram":"write");
	if (!(hiswrt.wrt_telpref & (TELPREF_WRITE|TELPREF_TEL)) )
	{
	    if (!no_switch) printf("Try the \042talk\042 command.\n");
	    wrtlog("FAIL: can't switch - talk only");
	    done(1);
	}
	if (no_switch || file_input)
	{
	    if (!no_switch) printf("Try the \042write\042 command "
				   "(with no input redirection).\n");
	    wrtlog("FAIL: can't switch");
	    done(1);
	}
	else
	{
	    printf("%s instead? ", telegram?"write":"send telegram");
	    fflush(stdout);
	    if (!readyn())
	    {
		if (!am_root)
		{
		    wrtlog("ABANDON: won't switch");
		    done(1);
		}
	    }
	    else
		telegram= !telegram;
	}
	printf("%s to %s on %s...\n", telegram?"Telegram":"Writing",
		hisname, histty);
    }

    /* Switch between character and line mode on recipient's preference */
    if (!telegram)
    {
	if (file_input)
	    char_by_char= FALSE;
	else
	{
	    if (char_by_char && hiswrt.wrt_modepref == 'l')
	    {
		printf("[Changing to line mode by %s's preference]\n",hisname);
		char_by_char= FALSE;
	    }
	    else if (!char_by_char && hiswrt.wrt_modepref == 'c')
	    {
		printf("[Changing to character mode by %s's preference]\n",
			hisname);
		char_by_char= TRUE;
	    }
	}
    }
    else
    {
    	/* Turn off various options irrelevant to telegrams */
    	char_by_char= FALSE;
    	tmp_mesg= 's';
    	postpone= FALSE;
    }

    /* Open his terminal */
    if (rec_only)
    {
	if ((histerm= fopen("/dev/null","w")) == NULL)
	{
	    printf("%s:Panic - Cannot open /dev/null to write\n",progname);
	    done(1);
	}
    }
    else
    {
	if ((histerm= fopen(hisdevname,"w")) == NULL)
	{
	    printf("%s:Panic - Cannot open %s to write\n",progname,hisdevname);
	    done(1);
	}
	fcntl(fileno(histerm),F_SETFD,1);		/* Close over execs */
    }

    if (telegram || file_input)
	open_record();

    open_hist();
    wrtlog(rec_only?"POSTPONED":"OK");

    /* Now that his terminal and the wrttmp file are open, abandon superuser */
    setuid(getuid());
    setgid(getgid());

    /* Fix my entry in wrttmp */
    set_modes();

    /* Get rid of any previously recorded messages */
    if (postpone) init_lastmesg();

    siginit();

    if (telegram)
	dotelegram(nl);
    else
        dowrite();

    done(0);
}


/* READYN - Read a "yes" or "no" from /dev/tty (in case stdin is redirected).
 * Return TRUE if yes.
 */

bool readyn()
{
FILE *fp;
char ynbuf[LBSIZE];

    if ((fp= fopen("/dev/tty","r")) == NULL) fp= stdin;
    flushinput(fileno(fp));
    fgets(ynbuf,LBSIZE,fp);
    if (fp != stdin) fclose(fp);
    return (*ynbuf == 'y' || *ynbuf == 'Y');
}


/* DONE -- This is the standard exit routine.  It prints the EOF message (if
 * we are actually writing the other user) and cleans up ttymodes and the
 * wrttmp file.
 */

void done(int code)
{
extern FILE *recfp;

    /* Put all those signals to sleep */
    sigoff();

    /* If we were writing, print the exit message */
    if (is_writing && !nested_write())
    {
	fprintf(histerm,"EOF (%s)\n",myname);
	fflush(histerm);
	if (recfp) fprintf(recfp,"EOF (%s)\n",myname);
    }

    if (postpone) show_lastmesg();

    /* If we changed our wrttmp entry, restore it */
    reset_modes();

    /* If we were in cbreak, restore old tty modes */
    if (in_cbreak)
	cbreak(FALSE);

    exit(code);	/* This should be the only call to exit() */
}


/* TYPE_HELP -- This prints out a help file.
 */

void type_help(char *file)
{
FILE *fp;
int ch;

    if ((fp= fopen(file,"r")) != NULL)
    {
	while ((ch= getc(fp)) != EOF)
	    putchar(ch);
	fclose(fp);
    }
}


/* LOG - This logs a write execution.
 */

void wrtlog(char *outcome)
{
FILE *fp;
time_t tock;
extern bool helpseeker;

    /* If we aren't logging, just get out */
    if (f_loglevel == 0 || (f_loglevel == 1 && !helpseeker))
    	return;

    /* If log file exist, open it, otherwise leave without creating it */
    if (f_log == NULL || access(f_log,0) || (fp= fopen(f_log,"a")) == NULL)
	return;

    tock= time((time_t *)0);
    if (helpseeker)
    {
	fprintf(fp,"%20.20s %-*.*s %s help (%s %s): %s\n",
	    ctime(&tock)+4,
	    UT_NAMESIZE, UT_NAMESIZE, myname,
	    progname,
	    (hisname[0] && strcmp(hisname,"help"))?hisname:"-",
	    histty[0]?histty:"-",
	    outcome);
    }
    else
    {
	fprintf(fp,"%20.20s %-*.*s %s %s %s: %s\n",
	    ctime(&tock)+4,
	    UT_NAMESIZE, UT_NAMESIZE, myname,
	    progname,
	    hisname[0]?hisname:"-",
	    histty[0]?histty:"-",
	    outcome);
    }
    fclose(fp);
}
