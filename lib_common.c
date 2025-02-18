/* ROUTINES COMMON BETWEEN WRITE, AMIN, MESG, ETC */

#include "orville.h"
#include <sys/stat.h>


/* CONFIGURATION OPTION TABLE - various commands that can be set in the
 * orville.conf file.  The options are actually stored in the global variables
 * declared here (with their default values).  The conftab[] structure is used
 * by the readconf() program to load them up easily.
 */

#define CO_OPT 0	/* 'options' command */
#define CO_STR 1	/* String-valued command */
#define CO_FLG 2	/* Flag-valued command */
#define CO_INT 3	/* Int-valued command */

char *f_wrttmp= D_WRTTMP;	/* The who's-talking-to-whom file */
char *f_wrthist= D_WRTHIST;   	/* The who-talked-to-whom file */
char *f_novicehelp= NULL;	/* Type this if "NOVICE" env var is defined */
char *f_log= NULL;       	/* Log for all write connections */
char *f_helperlist= NULL;   	/* File of users who may be helpers */
char *f_helpername= "help";   	/* Name you write to get help */
char *f_nohelp= NULL;   	/* File to print if no helpers available */

int f_disconnect= TRUE;		/* Does 'mesg d' disconnect users? */
int f_exceptions= TRUE;		/* Do 'mesg ne' and 'mesg ye' work? */
int f_fromhost= FALSE;		/* Include hostname in 'Message from' banner? */
int f_helpers= FALSE;		/* Does 'write help' work? */
int f_pipes= TRUE;		/* Can I cat files through write? */

int f_loglevel= 0;		/* How much logging to do? */
int f_answertel= 240;		/* How many seconds to answer telegrams? */

struct { char *cmd; int len; int type; void *var; } conftab[]= {
    	{"answertel",	9,	CO_INT,	(void *)&f_answertel},
    	{"disconnect", 10,	CO_FLG,	(void *)&f_disconnect},
    	{"exceptions", 10,	CO_FLG,	(void *)&f_exceptions},
    	{"fromhost",	8,	CO_FLG,	(void *)&f_fromhost},
    	{"helperlist", 10,	CO_STR,	(void *)&f_helperlist},
    	{"helpername", 10,	CO_STR,	(void *)&f_helpername},
    	{"helpers",	7,	CO_FLG,	(void *)&f_helpers},
    	{"log",		3,	CO_STR,	(void *)&f_log},
    	{"loglevel",	8,	CO_INT,	(void *)&f_loglevel},
    	{"nohelp", 	6,	CO_STR,	(void *)&f_nohelp},
    	{"novicehelp", 10,	CO_STR,	(void *)&f_novicehelp},
	{"options",	7,	CO_OPT,	NULL},
    	{"pipes", 	5,	CO_FLG,	(void *)&f_pipes},
    	{"wrthist",	7,	CO_STR,	(void *)&f_wrthist},
    	{"wrttmp",	6,	CO_STR,	(void *)&f_wrttmp},
    	{NULL, 		0,	0,	NULL}};


char *progname;			/* The name of this program */
int wstream;			/* File descriptor for open wrttmp file */
struct wrthdr wt_head;		/* Header read out of wrttmp file */
char mydevname[UT_LINESIZE+10];	/* my tty name in /dev/tty?? format */


/* LEAFNAME: return pointer to last component of a pathname */

char *leafname(char *fullpath)
{
char *leaf;

    if ((leaf= strrchr(fullpath,'/')) == NULL)
            leaf= fullpath;
    else
	    leaf++;
    return(leaf);
}


/* GETDEVTTY:  Store the ttyname in the global mydevname variable.
 * Print and error message and return non-zero if stderr not a tty open for
 * read.  This is to prevent people from pretending they are writing from a
 * terminal other than their own by redirecting stderr to fool ttyname().
 * They are unlikely to have read access to anyone else's tty.
 */

int getdevtty()
{
char *tty;

    if (!(fcntl(2,F_GETFL,0) & O_RDWR) || !isatty(2))
    {
    	printf("%s: stderr improperly redirected\n",progname);
	return 1;
    }
    if ((tty= ttyname(2)) == NULL || strlen(tty) < 5)
    {
    	printf("%s: Not on a valid tty\n");
	return 2;
    }
    strncpy(mydevname,tty,UT_LINESIZE+10);
    return 0;
}

/* INIT_WSTREAM - open the wrttmp file, loading and checking the header.
 * Mode can be either O_RDONLY or O_RDWR.  Prints error message and returns
 * true if it fails.
 */

int init_wstream(int mode)
{
    if ((wstream= open(f_wrttmp,mode)) < 0)
    {
	printf("%s: Unable to open %s to read/write\n", progname, f_wrttmp);
	return(1);
    }
    fcntl(wstream,F_SETFD,1);		/* Close over execs */

    /* Read in wrttmp file header */
    if (read(wstream,&wt_head,sizeof(struct wrthdr)) == sizeof(struct wrthdr))
    {
	if ( wt_head.hdr_size < sizeof(struct wrthdr) ||
	     wt_head.tmp_size < sizeof(struct wrttmp) )
	{
	    printf("%s: %s file has undersized entries.  Old version?\n",
		progname, f_wrttmp);
	    return(1);
	}
    }
    else
    {
	/* Initialize header for new wrttmp file */
	wt_head.hdr_size= sizeof(struct wrthdr);
	wt_head.tmp_size= sizeof(struct wrttmp);
	if (mode != O_RDONLY)
	{
	    lseek(wstream, 0L, 0);
	    write(wstream, &wt_head, sizeof(struct wrthdr));
	    lseek(wstream, 0L, 1);	/* must seek between write and read */
	}
    }
    return(0);
}


/* FIND_UTMP -- Find the named tty in the utmp file, returning NULL if we
 * fail.  The tty name need not be null terminated.
 */

struct utmpx *find_utmp(char *tty)
{
struct utmpx tmputmp;

    strncpy(tmputmp.ut_line, tty, UT_LINESIZE);
    setutent(); /* open and/or rewind */
    return getutxline(&tmputmp);
}


/* FIND_WRTTMP:  Find the named tty in the wrttmp file and return the wrttmp
 * field and the offset of that line's entry.  If the entry's login time
 * doesn't match the given one, or if there is no entry in the file, then
 * return a default wrttmp entry instead.  In the latter case, the position
 * returned will be the offset of the first blank slot at the end of the file.
 */

void find_wrttmp(char *tty, time_t time,struct wrttmp *wbuf, long *pos)
{
register int slot;

    slot= 0;
    for(;;)
    {
	lseek(wstream, *pos= wrttmp_offset(slot++),0);
	if (read(wstream, wbuf, sizeof(struct wrttmp)) != sizeof(struct wrttmp))
	    break;

	if (!strncmp(tty, wbuf->wrt_line, UT_LINESIZE))
	{
	    if (wbuf->wrt_time == time)
		return;
	    else
		break;
	}
    }

    /* Set wbuf to default value */
    dflt_wrttmp(wbuf,tty,time);
}


/* DFLT_WRTTMP -- set a wrttmp entry to default status */

void dflt_wrttmp(struct wrttmp *wbuf, char *tty, time_t time)
{
    /* Set wbuf to default value */
    strncpy(wbuf->wrt_line,tty,UT_LINESIZE);
    wbuf->wrt_time= time;
    wbuf->wrt_what[0]= '\0';
    wbuf->wrt_last[0]= '\0';
    wbuf->wrt_pid= -1;
    wbuf->wrt_record= DFLT_RECO;
    wbuf->wrt_telpref= DFLT_PREF;
    wbuf->wrt_modepref= DFLT_MODE;
    wbuf->wrt_bells= DFLT_BELL;
#ifndef TTYPERMS
    wbuf->wrt_mesg= DFLT_MESG;
#endif /*TTYPERMS*/
    wbuf->wrt_help= DFLT_HELP;
    wbuf->wrt_except= DFLT_EXCP;
}


#ifdef TTYPERMS
int myperms;			/* my original tty perms */
extern char mydevname[];

/* SAVEPERMS:  Remember the current tty permissions of my terminal.  Returns
 * 1 on failure.
 */

int saveperms()
{
struct stat stb;

	if (stat(mydevname,&stb))
	{
		printf("%s: Panic - can't stat %s\n",progname,mydevname);
		return(1);
	}
	myperms= stb.st_mode;
	return(0);
}


/* SETPERMS:  Set the tty permissions to yes (perm='y'), no (perm='n'), or
 * leave them alone (perm='s').
 */

void setperms(char perm)
{
	if (perm == 'n')
		chmod(mydevname,PERMS_OFF);

	if (perm == 'y')
		chmod(mydevname,PERMS_ON);
}


/* RESETPERMS:  Set the tty permissions back to saved state.
 */

void resetperms()
{
	chmod(mydevname,myperms);
}
#endif /*TTYPERMS*/



/* READCONFIG:  This reads through the orville.conf file loading configuration
 * settings.  It calls set_dflt() on any 'options' lines it sees, and saves
 * all configuration filenames.  If set_dflt is NULL, it isn't called.
 */

void readconfig(void (*set_dflt)(char *))
{
FILE *fp;
#define BFSZ 1024
char buf[BFSZ+1];
int i;
char *p, *q;

    if ((fp= fopen(ORVILLE_CONF,"r")) == NULL)
    {
	fprintf(stderr,"Unable to open "ORVILLE_CONF" to read\n");
	exit(1);
    }
	
    while (fgets(buf,BFSZ,fp))
    {
	if (buf[0] == '#' || buf[0] == '\n') continue;
	for (i= 0; conftab[i].cmd != NULL; i++)
	{
	    if (strncmp(buf, conftab[i].cmd, conftab[i].len) ||
		strchr(" \n\t\r", buf[conftab[i].len]) == NULL)
		    continue;
	
	    /* skip white space */
	    p= buf + conftab[i].len + 1;
	    while (*p != '\0' && strchr(" \t\n\r",*p) != NULL)
	    	p++;

	    if (*p == '\0' || *p == '\n' || *p == '\r')
	    {
		fprintf(stderr,"%s: No value given for %s in "
		    ORVILLE_CONF"\n", progname, conftab[i].cmd);
	    	exit(1);
	    }

	    switch (conftab[i].type)
	    {
	    case CO_OPT:	/* "options" command */

		if (set_dflt != NULL)
		    (*set_dflt)(p);
		break;
	    
	    case CO_STR:	/* string-valued commands */

		/* Find end of word */
		for (q= p; *q != '\0' && strchr(" \t\n\r",*q) == NULL; q++)
		    ;
		*q= '\0';

		/* Save the new value, without freeing any previous value,
		 * which might be static.
		 */
		*(char **)conftab[i].var= malloc(q-p+1);
		strcpy(*(char **)conftab[i].var,p);
		break;

	    case CO_FLG:	/* flag-valued commands */
	    	
		*(int *)conftab[i].var= (*p == 'y' || *p == 'Y');
		break;

	    case CO_INT:	/* integer-valued commands */
	    	
		*(int *)conftab[i].var= atoi(p);
		break;
	    }
	    break;
	}
    }
    fclose(fp);
}
