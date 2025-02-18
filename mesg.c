/*           MESG command for Orville write
 *
 * This 'mesg' command is an extended version of the standard Unix 'mesg'
 * command.  It is designed to work with Orville write
 *
 * (C) Copyright, Sept 1985 - Jan Wolter
 *
 */

#include "orville.h"
#include <signal.h>
#include <pwd.h>
#include <sys/stat.h>


#define TRUE	1
#define FALSE	0

char *mytty;			/* my tty name in tty?? format */
long mypos;			/* offset of my entry in wrttmp file */
struct wrttmp mywrt;		/* my wrttmp entry */
struct utmpx myutmp;		/* A tmp buffer for reading utmp entries */

char silent= FALSE;		/* generates no output if true */
int verbose= FALSE;		/* generate whole table of output if true */

#define SMESG 0
#define SHELP 1
#define SRECO 2
#define SPREF 3
#define SMODE 4
#define SBELL 5
#define SEXCP 6		/* This must be last flag */
#define S_NUM 7

char *name[S_NUM]= {"mesg", "helper", "record", "preference",
                    "mode", "bells", "exceptions"};
char *vname[S_NUM]= {"messages", "helper flag", "record",
#ifdef WITH_TALK
		     "preference (write/tel/talk)",
#else
		     "preference (write/tel)",
#endif
		     "mode (line/character)",
		     "bells", "exceptions"};

/* Legal tag values for each type of setting.  These must be listed in the
 * order of the corresponding numerical codes. nmap lists all legal flags,
 * smap lists only the flags that may be set by "mesg" and must have only
 * truncated versions of the nmap strings. */
char *nmap[S_NUM]= {"yn","ynY","yna","wtkan","lca","yn","yn"};
char *smap[S_NUM]= {"yn","ynY","yn","wtkan","lca","yn","yn"};

char *vmap[S_NUM][4]= { {"yes","no","yes (exceptions)", "no (exceptions)"},
			{"yes","no","always (Y)",NULL},
			{"yes","no","all",NULL},
			{NULL}, /* not used */
			{"line","character","any",NULL},
			{"yes","no",NULL,NULL},
			{"yes","no",NULL,NULL}};

int report= SMESG;		/* which of the above to put in return code */

void set_perms(char new_mesg, int open_up);
int report_status(void);
int get_status(int rep);
int find_me(void);
int may_help(void);
void do_disconnect(void);
void window_warning(int newmode);
char *myhomedir(void);
int maywriteme(char *user, int mode);

int main(int argc, char **argv)
{
int i,j;
char new[S_NUM] = {'x', 'x', 'x', 0x00, 'x', 'x', 'x'};
char disconnect= FALSE;
char open_up = FALSE;
int setting= SMESG;
int flip= 0;
int update;
char wassilent= FALSE;
int lastset= -1;
char flag;

    /* Check if I have a valid tty */
    if (!(fcntl(2,F_GETFL,0) & O_RDWR) || !isatty(2))
    {
	printf("%s: I/O stream 2 improperly redirected\n",argv[0]);
	exit(2);
    }

    /* Find my tty line */
    if (getdevtty()) exit(1);
    mytty= mydevname+5;
    progname= leafname(argv[0]);

    /* Read configuration file */
    readconfig(NULL);

    /* Parse options */
    for(i=1;i<argc;i++)
        for (j= 0; argv[i][j] != '\0'; j++)
	{
	    flag= argv[i][j];

	    /* Convert number arguments to appropriate characters */
	    if (flag >= '0' && flag <= '9')
	    {   if (setting != SPREF)
	    	{
		    if (flag < '0' + strlen(smap[setting]))
			flag= smap[setting][flag-'0'];
		}
		else
		{
		    /* could be multi-digit number - add it up */
		    flag-= '0';
		    while (argv[i][j+1] >= '0' && argv[i][j+1] <= '9')
		    {
			flag= 10*flag + argv[i][++j] - '0';
		    }
		    if (flag > TELPREF_ALL || flag < 0)
		    	goto usage;
		    new[SPREF]= flag;
		    continue;
		}
	    }
	    else
		flag= argv[i][j];

	    switch (flag)
	    {
	    case '-':
		if (j != 0) goto usage;
		break;

	    case 'v':
		verbose= TRUE;
		break;

	    case 's':
		if (setting != SMESG) goto usage;
		wassilent= silent= TRUE;
		break;

	    case 'm':
		if (setting != SMESG) report= setting;
		setting= SMODE;
		break;

	    case 'b':
		if (setting != SMESG) report= setting;
		setting= SBELL;
		break;

	    case 'r':
		if (setting != SMESG) report= setting;
		setting= SRECO;
		break;

	    case 'p':
		if (setting != SMESG) report= setting;
		setting= SPREF;
		flip= 0;
		break;

	    case 'x':
		if (setting != SMESG) report= setting;
		setting= SPREF;
		if (flip == 0) new[SPREF]= TELPREF_ALL;
		flip= 1;
		break;

	    case 'h':
		if (!f_helpers)
		{
		    printf("%s: Cannot use 'h' flag.  Helpers not enabled "
		    	"on this system.\n", progname);
		    exit(1);
		}
		if (setting != SMESG) report= setting;
		setting= SHELP;
		break;

	    case 'E':
	    case 'e':
		if (!f_exceptions)
		{
		    printf("%s: Cannot use '%c' flag.  Exceptions not enabled "
		    	"on this system.\n", progname, flag);
		    exit(1);
		}
		if (lastset != SMESG) goto usage;
		new[SEXCP]= 'y';
		break;

	    case 'Y':
		if (setting == SMESG)
		{
		    open_up= TRUE;
		    new[SMESG]= 'y';
		    lastset= SMESG;
		    break;
		}
		else if (setting != SHELP)
		    goto usage;
		/* else drop through */

#ifdef WITH_TALK
	    case 'k':
#endif
	    case 'y':
	    case 'n':
	    case 'l':
	    case 'c':
	    case 'w':
	    case 't':
	    case 'a':
		if (strchr(smap[setting],flag) == NULL) goto usage;
		if (setting == SPREF)
		{
		    if (flip)
		    {
			switch (flag)
			{
			case 'w': new[SPREF]&= ~TELPREF_WRITE; break;
			case 't': new[SPREF]&= ~TELPREF_TEL; break;
#ifdef WITH_TALK
			case 'k': new[SPREF]&= ~TELPREF_TALK; break;
#endif
			case 'n': new[SPREF]= TELPREF_ALL; break;
			case 'a': goto usage;
			}
		    }
		    else
		    {
			switch (flag)
			{
			case 'w': new[SPREF]|= TELPREF_WRITE; break;
			case 't': new[SPREF]|= TELPREF_TEL; break;
#ifdef WITH_TALK
			case 'k': new[SPREF]|= TELPREF_TALK; break;
#endif
			case 'a': new[SPREF]= TELPREF_ALL; break;
			case 'n': goto usage;
			}
		    }
		}
		else
		    new[setting]= flag;
		lastset= setting;
		setting= SMESG;
		break;

	    case 'N':
		lastset= SMESG;
	    case 'd':
		if (setting != SMESG) goto usage;
		disconnect= TRUE;
		if (flag == 'N') new[SMESG]= 'n';
		break;

#ifndef WITH_TALK
	    case 'k':
	    	printf("%s: Talk permissions cannot be separately set "
		    "at this site\n",progname);
		goto usage;
#endif

	    default:
		goto usage;
	    }
	}

    /* If we changed message perms, but didn't set exceptions, turn them off */
    if (new[SMESG] != 'x' && new[SEXCP] == 'x') new[SEXCP]= 'n';

    if (setting != SMESG)
	report= setting;
    else if (report == SMESG)
	report= ((lastset > SMESG) ? lastset : SMESG);

    /* Open the wrttmp file and find us in it */
    if (init_wstream(O_RDWR))
	exit(-1);
    find_me();

    if ((new[SHELP] == 'y'  || new[SHELP] == 'Y') && !may_help())
    {
	printf("You are not on the helper list.\n");
	exit(1);
    }

    /* Update permissions/helperness/preferences/bells */
    update= 0;
    if (new[SMESG] != 'x')
    {	set_perms(new[SMESG],open_up); update= 1; }
    if (f_helpers && new[SHELP] != 'x')
    {	mywrt.wrt_help= new[SHELP]; update= 1; }
    if (f_exceptions && new[SEXCP] != 'x')
    {	mywrt.wrt_except= new[SEXCP]; update= 1; }
    if (new[SRECO] != 'x')
    {	mywrt.wrt_record= new[SRECO]; update= 1; }
    if (new[SMODE] != 'x')
    {	mywrt.wrt_modepref= new[SMODE]; update= 1; }
    if (new[SPREF] != 0x00 && !flip)
    {	mywrt.wrt_telpref= new[SPREF]; update= 1; }
    if (new[SPREF] != TELPREF_ALL && flip)
    {	if (new[SPREF] == 0x00)
            set_perms('n',0);
        else
	    mywrt.wrt_telpref= new[SPREF];
	update= 1;
    }
    if (new[SBELL] != 'x')
    {	mywrt.wrt_bells= new[SBELL]; update= 1; }

    /* Write out new settings */
    if (update)
    {
	lseek(wstream,mypos,0);
	write(wstream,&mywrt,sizeof(struct wrttmp));
	silent= TRUE;
    }

    /* Disconnect existing connections */
    if (disconnect)
    {
    	if (f_disconnect)
	    do_disconnect();
	else
	    printf("Disconnect option not enabled on this system\n");
        silent= TRUE;
    }

    /* Close the utmp file */
    endutxent();

   if (f_wrthist != NULL &&
       !wassilent && (new[SMESG] == 'n' || new[SEXCP] == 'y'))
       window_warning((new[SMESG]=='n')+2*(new[SEXCP]=='y'));

    exit(report_status());

usage:
    printf("usage: %s [-sv] [y|n", progname);
    if (f_disconnect) printf("|d");
    if (f_exceptions) printf("|ye|ne");
    printf("|Y");
    if (f_disconnect)
    {
    	printf("|N");
	if (f_exceptions)
	    printf("|NE");
    }
    printf("] ");
    if (f_helpers) printf(" [-h[y|n|Y]]");
    printf(" [-r[y|n]] [-m[l|c|a]] [-p[w|t|k|a]] [-x[w|t|k|n]] [-b[y|n]]\n");
    exit(-1);
}


/* SET_PERMS:  This sets the message permissions to "new_mesg" (which is
 * either 'y' or 'n'.  It throws the tty permissions wide open as well if
 * "open_up" is true.
 */

void set_perms(char new_mesg, int open_up)
{
int mode;

    /* Change the device permissions */
    if (open_up)
	mode= PERMS_OPEN;
#ifdef TTYPERMS
    else if (new_mesg == 'y')
	mode= PERMS_ON;
#endif /*TTYPERMS*/
    else
	mode= PERMS_OFF;

    chmod(mydevname,mode);

    /* Change the wrttmp permissions */
#ifndef TTYPERMS
    mywrt.wrt_mesg= new_mesg;
#endif /*TTYPERMS*/
}


/* REPORT_PREF -- This prints a message reporting the perf setting, in
 * verbose or non-verbose format.  The appropriate numeric return code is
 * returned.
 */

int report_pref(int verbose)
{
int val,n;

    if (!silent)
    {
	if (verbose)
	    printf("%s: ",vname[SPREF]);
	else
	    printf("%s is ",name[SPREF]);
    }

    val= mywrt.wrt_telpref;
    
    if (val & TELPREF_OLD)
    {
    	switch (val)
	{
	case 'w': val= TELPREF_WRITE; break;
	case 't': val= TELPREF_TEL; break;
#ifdef WITH_TALK
	case 'k': val= TELPREF_TALK; break;
#endif
	case '-': case 'a': val= TELPREF_ALL; break;
	}
    }

    if (!silent)
    {
	if (val == TELPREF_ALL)
	{
	    fputs(verbose ? "all\n":"a\n" ,stdout);
	    return val;
	}
	n= 0;
	if (val & TELPREF_WRITE)
	{
	    fputs(verbose ? "write":"w" ,stdout);
	    n++;
	}
	if (val & TELPREF_TEL)
	{
	    if (n > 0) putchar(',');
	    fputs(verbose ? "tel":"t" ,stdout);
	    n++;
	}
#ifdef WITH_TALK
	if (val & TELPREF_TALK)
	{
	    if (n > 0) putchar(',');
	    fputs(verbose ? "talk":"k" ,stdout);
	}
#endif
	putchar('\n');
    }
    return val;
}

/* REPORT_STATUS -- This prints the current message permission settings and
 * returns the status code (0 = yes; 1 = no).  If silent is true, it omits
 * the printing.  If report is set to something other than SMESG it reports
 * on that other setting.  If verbose is true, it reports on all settings.
 */

int report_status()
{
int code,rc,i;

    if (verbose)
    {
	for (i= 0; i < S_NUM; i++)
	{
		if (i == SEXCP) continue;
		if (!f_helpers && i == SHELP) continue;
		if (i == SPREF) {report_pref(1); continue; }
		code= get_status(i);
		printf("%s: %s\n",vname[i],
		    vmap[i][code] != NULL ? vmap[i][code] : "strange");
		if (i == report) rc= code;
	}
    }
    else
    {
	rc= get_status(report);
	if (!silent)
	{
	    if (f_exceptions && report == SMESG)
		printf("%s is %c%s\n",name[report],nmap[report][rc%2],
		       rc > 1 ? "e" : "");
	    else if (report == SPREF)
	        report_pref(0);
	    else
		printf("%s is %c\n",name[report],nmap[report][rc]);
    	}
    }
    return rc;
}


/* GET_STATUS -- Get current index code of given switch */

int get_status(int rep)
{
struct stat stb;
int code;

    switch (rep)
    {
    case SMESG:
#ifdef TTYPERMS
	/* Stat my tty */
	if (stat(mydevname,&stb))
	{
	    printf("%s: Error - can't stat %s\n",progname,mydevname);
	    exit(-1);
	}
	code= ((stb.st_mode & 0020)==0);
#else
	code= strchr(nmap[SMESG],mywrt.wrt_mesg) - nmap[SMESG];
#endif /* TTYPERMS */
	if (f_exceptions)
	    code+= 2 - 2*(strchr(nmap[SEXCP],mywrt.wrt_except) - nmap[SEXCP]);
	break;

    case SEXCP:
	code= strchr(nmap[SEXCP],mywrt.wrt_except) - nmap[SEXCP];
	break;

    case SHELP:
	code= strchr(nmap[SHELP],mywrt.wrt_help) - nmap[SHELP];
	break;

    case SRECO:
	code= strchr(nmap[SRECO],mywrt.wrt_record) - nmap[SRECO];
	break;

    case SMODE:
	code= strchr(nmap[SMODE],mywrt.wrt_modepref) - nmap[SMODE];
	break;

    case SBELL:
	code= strchr(nmap[SBELL],mywrt.wrt_bells) - nmap[SBELL];
	break;
    }
    return code;
}


/* DO_DISCONNECT -- This does the disconnect, by searching the wrttmp file for
 * people who are writing you and sending a SIGTERM to each of the process
 * numbers listed there.  This whole approach is a bit shady.
 */

void do_disconnect()
{
struct utmpx *ut;		/* A tmp buffer for reading utmp entries */
struct wrttmp hiswrt;		/* Someone's wrttmp entry */
int slot= 0;

    /* Rewind utmp file */
    setutxent();

    /* For each user who is writing me */
    for (;;)
    {
	lseek(wstream, wrttmp_offset(slot++), 0);
	if (read(wstream, &hiswrt, sizeof(struct wrttmp)) !=
	       sizeof(struct wrttmp))
	   break;

	if (!strncmp(hiswrt.wrt_what, myutmp.ut_user, sizeof(myutmp.ut_user)))
	{
	    setutent();
	    /* Check apparant writer against utmp file */
	    while ((ut= getutxent()) != NULL)
		if (
#ifdef USER_PROCESS
		    ut->ut_type == USER_PROCESS &&
#endif
		    !strncmp(hiswrt.wrt_line, ut->ut_line, sizeof(ut->ut_line)))
		{
		    /* Writer is for real: bonk him one */
		    kill(hiswrt.wrt_pid, SIGTERM);
		    break;
		}
	}
    }
}



/* FIND_ME -- This finds my wrttmp entry and loads it into "mywrt".
 */

int find_me()
{
struct utmpx *ut;

    /* Find our entry in the Utmp file */
    if ((ut= find_utmp(mytty)) == NULL || ut->ut_user[0] == '\0')
    {
	printf("%s: Unable to find your tty (%s) in utmp file\n",
		progname,mytty);
	exit(-1);
    }
    myutmp= *ut;

    /* Find the entry in the wrttmp file */
    find_wrttmp(mytty,myutmp.ut_tv.tv_sec,&mywrt,&mypos);
}


/* MAY_HELP -- Check if the user's name is anywhere to be found in the
 * f_helperlist file.  If he is there, or the file doesn't exist at all, then
 # he may designate himself as a helper.
 */

int may_help()
{
#define BUFSZ 80
FILE *hfp;
char buf[BUFSZ+1];
char myname[sizeof(myutmp.ut_user)+1];

	if (f_helperlist == NULL || (hfp= fopen(f_helperlist,"r")) == NULL)
		return TRUE;
	
	strncpy(myname,myutmp.ut_user,sizeof(myutmp.ut_user) -1);
	myname[sizeof(myutmp.ut_user)]= '\0';
	strcat(myname,"\n");
	
	while (fgets(buf,BUFSZ,hfp) != NULL)
		if (!strcmp(buf,myname))
		{
			fclose(hfp);
			return TRUE;
		}

	fclose(hfp);
	return FALSE;
}


/* WINDOW_WARNING - If there are people who can still write you under the
 * four minute window rule, print a warning message.
 */

void window_warning(int newmode)
{
struct wrthist *hist;
struct wrttmp w;
struct utmpx *u;
long writer, writee;
time_t now;
int n, foundsome= 0;
FILE *fp;

    if (newmode == 0) return;

    /* Compute my slot number from my offset */
    if ((writer= (mypos - wt_head.hdr_size)/wt_head.tmp_size) > MAXSLOTS)
	return;

    if ((fp= fopen(f_wrthist,"r")) == NULL)
	return;

    fseek(fp,whindex(writer,0),0);
    hist= (struct wrthist *)malloc(MAXSLOTS*sizeof(struct wrthist));
    n= fread(hist,sizeof(struct wrthist),MAXSLOTS,fp);
    fclose(fp);

    now= time((time_t *)0);

    for (writee= 0; writee < n; writee++)
    {
	if (hist[writee].tm > myutmp.ut_tv.tv_sec &&
	    now - hist[writee].tm <= f_answertel)
	{
	    /* Fetch "his" wrttmp entry - it may actually belong to a previous
	     * user on that line, so only the wrt_line field is really to be
	     * trusted.
	     */
	    lseek(wstream,wrttmp_offset(writee),0);
	    if (read(wstream, &w, sizeof(struct wrttmp)) !=
		   sizeof(struct wrttmp))
		continue;

	    /* Check times to ensure this is not from a later user */
	    if (hist[writee].tm < w.wrt_time)
	    	continue;

	    /* Fetch his utmp entry, and confirm that the current user was
	     * already logged in there when we sent our last telegram there.
	     */
	    if ((u= find_utmp(w.wrt_line)) == NULL || u->ut_user[0] == '\0' ||
	        hist[writee].tm < u->ut_tv.tv_sec)
	    	continue;

	    /* Check if due to exceptions he may write us anyway */
	    if (f_exceptions && newmode > 1 && maywriteme(u->ut_user, newmode))
	    	continue;

	    if (!foundsome)
	    {
	        printf("Warning: Users to whom you recently sent messages may\n");
	        printf("still be able to write you for a few more minutes:\n");
	        foundsome= 1;
	    }
	    printf("  %-*.*s %-*.*s %4.1f more minutes\n",
	    	sizeof(u->ut_user) -1, sizeof(u->ut_user) -1, u->ut_user,
	    	sizeof(u->ut_line) -1, sizeof(u->ut_line) -1, u->ut_line,
	        (float)(f_answertel - now + hist[writee].tm)/60.0);
	}
    }
}


/* MYHOMEDIR - Return my home directory.  Speed counts over reliability here.
 * Return NULL if we can't find it.
 */

char *myhomedir()
{
char myname[sizeof(myutmp.ut_user)+1];
struct passwd *pw;
char *dir, *getenv();

    /* Try environment variable first */
    if ((dir= getenv("HOME")) != NULL)
	return dir;

    /* If that don't work, try passwd file */
    strncpy(myname,myutmp.ut_user,sizeof(myutmp.ut_user) -1);
    myname[sizeof(myutmp.ut_user)]= '\0';
    if ((pw= getpwnam(myname)) != NULL)
    	return pw->pw_dir;

    /* If that don't work, give it up */
    return NULL;
}


/* MAYWRITEME - can the named user write me?  This checks exceptions.  Does
 * some caching so that multiple successive calls are efficient.
 */

int maywriteme(char *user, int mode)
{
static int oldmode= -1;
static char *list;
struct stat st;
char *dir,*fname, *p,*q;
FILE *fp;
int n, l, nl, sp;

    if (mode < 2)
	return (mode == 0);

    if (mode != oldmode)
    {
	list= NULL;
	oldmode= mode;

	/* Open the appropriate exception file */
	if ((dir= myhomedir()) == NULL)
	    return FALSE;
	fname= (char *)malloc(strlen(dir)+15);
	sprintf(fname,"%s/%s",dir, mode==2 ? ".nowrite" : ".yeswrite");
	fp= fopen(fname,"r");
	free(fname);

	/* Load the file into memory */
	if (fp != NULL && !fstat(fileno(fp),&st))
	{
	    list= (char *)malloc(st.st_size+1);
	    n= fread(list,1,st.st_size,fp);
	    list[n]= '\0';
	}

	if (list != NULL)
	{
	    /* Compress list into login names separated by single spaces */
	    nl= 1; sp= 1;
	    for (p= q= list; *p != '\0'; p++)
	    {
		/* Skip lines starting with # */
		if (nl && (*p == '#'))
		{
		    p= strchr(p,'\n');
		    if (p == NULL) break;
		}

		nl= (*p == '\n');

		if (strchr(" ,;:\t\n\r",*p))
		{
		    if (!sp) *(q++)= ' ';
		    sp= 1;
		}
		else
		{
		    sp= 0;
		    *(q++)= *p;
		}
	    }
	    *q= '\0';
	}
    }

    if (list != NULL)
    {
	/* Find length of user name */
	for (l= 0; l < UT_NAMESIZE && user[l] != '\0'; l++)
	    ;

	/* Search compressed list for the name */
	p= list;
	for (;;)
	{
	    if (!strncmp(p,user,l) && (p[l] == ' ' || p[l] == '\0'))
	    	return (mode != 2);
	    if ((p= strchr(p,' ')) == NULL) break;
	    p++;
	}
    }
   
    /* Not on list */
    return (mode == 2);
}
