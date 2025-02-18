/* AMIN.C
 *
 * Run a command, warning potential writers that you are in it.
 *
 *  8-24-86  Written by Jan Wolter
 *  8-27-86  Added code to make it work with shell scripts
 *  2-12-94  Fixed suspends.
 * 11-30-95  Added -p
 */

#include "orville.h"
#include <signal.h>
#include <errno.h>
#include <pwd.h>
#ifdef HAVE_SYS_WAIT_H
# include <sys/wait.h>
#endif
#ifndef WEXITSTATUS
#define WEXITSTATUS(x) ((unsigned)(x) >> 8)
#endif
#ifndef WIFEXITED
#define WIFEXITED(x) (((x) & 255) == 0)
#endif

#include <sys/stat.h>

extern int errno;

char *my_dir(void);
RETSIGTYPE susp(void);
void fix_modes(char *fullcmd);
void restore_modes(void);
int exec_cmd(int argc, char **argv);

struct wrttmp mywrt;	/* My old wrttmp entry, to be restored at exit time */
struct wrttmp newwrt;	/* Space to build temporary wrttmp entry */
long mypos= -1;		/* offset of my entry in the wrttmp file */
char tmp_mesg= 's';	/* Temporary setting of mesg during write */
int fixed_modes= 0;	/* Have we fixed the wrttmp/tty modes? */
int postpone= 0;	/* Should telegrams be postponed? */
int exceptions= 0;	/* Should we allow exceptions on message permissions */


int main(int argc, char **argv)
{
char cmdbuf[280];	/* Space to build full command path name */
char lmfile[200];	/* Space to store full path name of .lastmesg file */
FILE *lmfp= NULL;
int i,rc;
char *c;

    progname= leafname(argv[0]);
    readconfig(NULL);

    for (i= 1; i<argc; i++)
    {
	if (*(c= argv[i]) == '-')
	    while (*(++c) != '\000')
		switch(*c)
		{
		case 'n':
		case 'y':
		case 's':
			tmp_mesg= *c;
			break;
		case 'p':
			postpone= 1;
			break;
		case 'e':
			if (!f_exceptions)
			{
			    printf("%s: exceptions are not enabled on this "
				   "system so -e cannot be used.\n",progname);
			    exit(1);
			}
			if (tmp_mesg != 'n' && tmp_mesg != 'y')
			{
			    printf("%s: -e option must be preceded by "
				   "-n or -y option.\n",progname);
			    exit(1);
			}
			exceptions= 1;
			break;
		default:
			goto usage;
		}
	else 
	    break;
    }
    argv= argv+i;
    argc= argc-i;

    if (argc < 1) goto usage;

    /* Trap darn near every possible signal */
    signal(SIGINT,SIG_IGN);
    signal(SIGQUIT,SIG_IGN);
    signal(SIGTERM,SIG_IGN);
    signal(SIGHUP,SIG_IGN);
#ifdef SIGTSTP
    signal(SIGTSTP,(RETSIGTYPE (*)())susp);
#endif /*SIGTSTP*/

    /* Open the wrttmp file */
    if (init_wstream(O_RDWR))
	exit(1);

    /* Give up my root status */
    setuid(getuid());
    setgid(getgid());

    /* Check if device 2 is a tty open for read */
    if (!(fcntl(2,F_GETFL,0) & O_RDWR) || !isatty(2))
    {
	printf("%s: I/O stream 2 improperly redirected\n",progname);
	exit(1);
    }

    if (postpone)
    {
	char *dir;
	struct stat st;

	if ((dir= my_dir()) == NULL)
	    postpone= 0;
	else
	{
	    sprintf(lmfile,"%.180s/.lastmesg",dir);
	    unlink(lmfile);
	}
    }

    fix_modes(argv[0]);

    /* Fork off child */
    rc= exec_cmd(argc,argv);

    if (postpone && (lmfp= fopen(lmfile, "r")) != NULL)
	unlink(lmfile);

    restore_modes();

    signal(SIGINT,SIG_DFL);
    signal(SIGQUIT,SIG_DFL);
    signal(SIGTERM,SIG_DFL);
    signal(SIGHUP,SIG_DFL);
#ifdef SIGTSTP
    signal(SIGTSTP,SIG_DFL);
#endif

    if (lmfp != NULL)
    {
	int ch;

	printf("Messages received while you were busy...\n");
	while ((ch= getc(lmfp)) != EOF)
	    if (ch != '\007') putchar(ch);
    }

    exit(rc);

usage:
    printf("usage: %s [-ny%ssp] <command> <args>\n",progname,
    	f_exceptions ? "e" : "");
    exit(1);
}


/* MY_DIR - Return name of my home directory, or NULL if I can't find it.
 */

char *my_dir()
{
struct passwd *pwd;
char *r;

    /* Try the environment variable first */
    if ((r= getenv("HOME")) != NULL) return(r);

    /* Try the password file */
    setpwent();
    pwd= getpwuid(getuid());
    endpwent();
    if (pwd != NULL)
	return(pwd->pw_dir);

    /* Give up */
    return(NULL);
}


/* LOCATE_WRTTMP - find my entry in the wrttmp file.
 */

void locate_wrttmp(char *tty, struct wrttmp *wbuf, long *pos)
{
struct utmpx *ut;

    /* Find utmp entry */
    if ((ut= find_utmp(tty)) == NULL || ut->ut_user[0] == '\0')
    {
	printf("%s: Can't find your tty (%s) in utmp\n",progname,tty);
	exit(1);
    }

    find_wrttmp(tty, ut->ut_tv.tv_sec, wbuf, pos);
}


/* FIX_MODES - modify wrttmp and tty permissions.
 */

void fix_modes(char *fullcmd)
{
char *mytty;		/* my tty name in "tty##" format */
char *shortcmd;		/* command without full pathname */

    /* Find my tty line */
    if (getdevtty()) exit(1);
    mytty= mydevname+5;

    /* Find my wrttmp entry */
    locate_wrttmp(mytty,&mywrt,&mypos);

    if (mypos < 0)
    {
	printf("%s: Error - unable to locate %s in %s\n", 
		progname, mytty, f_wrttmp);
	exit(1);
    }

    /* Close utmp file */
    endutxent();

    /* Figure out name of program being exec'ed */
    if ((shortcmd= strrchr(fullcmd,'/')) == NULL)
	shortcmd= fullcmd;
    else
	shortcmd++;

    /* Fix my entry in wrttmp */
    newwrt= mywrt;
    newwrt.wrt_what[0]= '!';
    strncpy(newwrt.wrt_what+1, shortcmd, sizeof(((struct utmpx *)0)->ut_user)-2);
#ifndef TTYPERMS
    if (tmp_mesg != 's') newwrt.wrt_mesg= tmp_mesg;
#endif
    if (tmp_mesg != 's')
    	newwrt.wrt_except= (f_exceptions && exceptions) ? 'y' : 'n';
    if (postpone) newwrt.wrt_record= 'a';
    lseek(wstream,mypos,0);
    write(wstream,&newwrt,sizeof(struct wrttmp));

#ifdef TTYPERMS
    /* Fix my tty permissions */
    if (tmp_mesg != 's')
    {
	    if (saveperms()) exit(1);
	    setperms(tmp_mesg);
    }
#endif /*TTYPERMS*/
    fixed_modes= 1;
}


/* RESTORE_MODES - Restore original modes.
 */

void restore_modes()
{
    /* Restore old (or default) wrttmp entry */
    lseek(wstream,mypos,0);
    write(wstream,&mywrt,sizeof(struct wrttmp));
    close(wstream);

#ifdef TTYPERMS
    if (tmp_mesg != 's') resetperms();
#endif /*TTYPERMS*/
}


/* EXEC_CMD - Run the command as a child process
 */

int exec_cmd(int argc, char **argv)
{
int cpid,wpid;		/* Child pid and pid returned by wait */
char **shargv;		/* Arguements for running things with shell */
int status;		/* Child's return status */
int i;

    if ((cpid= fork()) == 0)
    {
	/* Child process */

	/* Use default interupts */
	signal(SIGINT,SIG_DFL);
	signal(SIGQUIT,SIG_DFL);
	signal(SIGTERM,SIG_DFL);
	signal(SIGHUP,SIG_DFL);
#ifdef SIGTSTP
	signal(SIGTSTP,SIG_DFL);
#endif /*SIGTSTP*/

	/* Close open file descriptors */
	/* close(wstream);  /* Don't need this, since set close-on-exec */
	endutent();	/* Might or might not need this */

	execvp(argv[0],argv);
	/* If wasn't executable, try running under a shell */
	if (errno == ENOEXEC)
	{
		shargv= (char **)malloc((argc+3)*sizeof(char *));
		shargv[0]= "sh";
		shargv[1]= "-c";
		for (i= 0; i < argc; i++)
		    shargv[i+2]= argv[i];
		shargv[i+2]= NULL;
		execv("/bin/sh",shargv);
	}
	
	printf("%s: command not found\n",argv[0]);
	exit(1);
    }
    /* Parent Process waits for child to exit */
    while ((wpid= wait(&status)) != cpid && wpid != -1)
	;

    return (WIFEXITED(status) ? WEXITSTATUS(status) : -2);
}


/* SUSP -- Suspend the process.  Restore wrttmp entries first, since we can't
 * assume that stopped processes will be restarted in the reverse order in
 * which they were stopped.
 */

#ifdef SIGTSTP
RETSIGTYPE susp()
{
int mask;

	/* Disable further signals */
	mask= sigblock(sigmask(SIGTSTP));

	/* Restore modes and make wrttmp look like we exited */
	if (fixed_modes)
	{
		lseek(wstream,mypos,0);
		write(wstream,&mywrt,sizeof(struct wrttmp));
#ifdef TTYPERMS
		if (tmp_mesg != 's') resetperms();
#endif /*TTYPERMS*/
		fixed_modes=0;
	}

	signal(SIGTSTP,SIG_DFL);
	sigsetmask(0);
	kill(getpid(),SIGTSTP);

	/* STOP HERE */

	sigsetmask(mask);
	signal(SIGTSTP,(void (*)())susp);

	/* Reinstate wrttmp entry */
	lseek(wstream,mypos,0);
	write(wstream,&newwrt,sizeof(struct wrttmp));

#ifdef TTYPERMS
	if (tmp_mesg != 's')
	{
	    if (saveperms()) exit(1);
	    setperms(tmp_mesg);
	}
#endif /*TTYPERMS*/
}
#endif /*SIGTSTP*/
