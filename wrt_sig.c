/* WRITE SIGNAL HANDLING ROUTINES -- Code to handle suspends and interputs and
 * fun stuff like that, as well as the normal exit routines.  Support for
 * system calls is here too.
 */

#include "write.h"
#include <signal.h>
#include <setjmp.h>

#ifdef USER_SHELL
void wsystem(char *cmd);
FILE *wpopen(char *cmd, char *mode);
void xwpclose(void);
#define wpclose(s)	xwpclose()
#else
#define wsystem(s)	system(s)
#define wpopen(s,m)	popen(s,m)
#define wpclose(s)	pclose(s)
#endif


jmp_buf sysenv;		/* Where to jump on intrupt during | and & escapes */


/* SIGINIT -- Set up the signal handler routines.
 */

void siginit()
{
	signal(SIGTERM,(RETSIGTYPE (*)())intr);
	signal(SIGINT,(RETSIGTYPE (*)())intr);
	signal(SIGHUP,(RETSIGTYPE (*)())intr);
#ifdef JOB_CONTROL
	signal(SIGTSTP,(RETSIGTYPE (*)())susp);
#endif /*JOB_CONTROL*/

}


/* SIGOFF -- Turn off all signals the signal handler routines.
 */

void sigoff()
{
	signal(SIGINT,SIG_IGN);
	signal(SIGQUIT,SIG_IGN);
	signal(SIGTERM,SIG_IGN);
#ifdef JOB_CONTROL
	signal(SIGTSTP,SIG_IGN);
#endif /*JOB_CONTROL*/
}



/* INTR -- Leave on an interupt.  If the interupt was during a shell escape,
 * resume as if the shell escape exited.
 */

RETSIGTYPE intr(int sig)
{
	if (insys && (sig == SIGINT))
	{
		signal (SIGINT, (RETSIGTYPE (*)())intr);
		longjmp(sysenv,1);
	}
	done(1);
}


/* SUSP -- Suspend the process.  Unlike shell escapes, we restore the wrttmp
 * entry before stopping.  This is because, unlike shell escapes, we can't
 * assume that stopped processes will be restarted in the reverse order in
 * which they were stopped.
 */

#ifdef JOB_CONTROL
RETSIGTYPE susp()
{
int was_cbreak= in_cbreak;
int mask;

	/* Disable further signals */
	mask= sigblock(sigmask(SIGTSTP));

	/* Restore modes and make wrttmp look like we exited */
	if (in_cbreak) cbreak(FALSE);
	if (postpone) show_lastmesg();
	reset_modes();

	signal(SIGTSTP,SIG_DFL);
	sigsetmask(0);
	kill(getpid(),SIGTSTP);

	/* STOP HERE */

	sigsetmask(mask);
	signal(SIGTSTP,(RETSIGTYPE (*)())susp);

	/* Reinstate cbreak mode and wrttmp entry */
	if (was_cbreak) cbreak(TRUE);
	update_modes();
	if (!telegram) set_modes();
}
#endif /*JOB_CONTROL*/


/* DOSYSTEM: This routine does a system call of one of three types.  If the
 * first character is a "!" then we do the normal system call.  If the first
 * character is a "&" then we open a pipe to that command and send one copy
 * of it's output to the other person, and one copy to me.  If the first
 * character is a "|" we only send the output to him.
 *
 * This routine absolutely must not be called until after we are no longer
 * root.  The wpopen() and wsystem() calls used here are not secure, even with
 * full paths given.
 */

void dosystem(char *cmd)
{
char was_cbreak= in_cbreak;
FILE *pip, *wpopen();
int ch;

	if (in_cbreak) cbreak(FALSE);

	if (*cmd == '!')
	{
		wsystem(cmd+1);
	}
	else if (!f_pipes)
		printf("%s: piped shell escapes disabled\n",progname);
	else
	{
		if ((pip= wpopen(cmd+1,"r")) == NULL)
			printf("%s:Panic - Unable to start command\n",progname);
		else
		{
			if (!setjmp(sysenv))
			{
				insys= TRUE;
				while((ch= getc(pip)) != EOF)
				{
					if (*cmd == '&') putchar(ch);
					sendchar(ch);
				}

			}
			insys= FALSE;
			wpclose(pip);
		}
	}

	/* Restore cbreak mode */
	if (was_cbreak) cbreak(TRUE);

	if (tmp_mesg == 's') update_modes();

	printf("%c\n",*cmd);
}

#ifdef USER_SHELL
/* WSYSTEM:  A modified version of the system() command that uses the user's
 * own shell (as specified by the "SHELL" environment variable) instead of
 * always using sh.
 */

void wsystem(char *cmd)
{
register int cpid,wpid;
RETSIGTYPE (*old_intr)(), (*old_quit)();
char *shell;

	if ((cpid = fork()) == 0)
	{
	    dup2(2,1);
	    /*setuid(getuid()); setgid(getgid());*/
	    endutent();
	    if ((shell= getenv("SHELL")) == NULL) shell= "/bin/sh";
	    execl(shell,leafname(shell),"-c",cmd,(char *)NULL);
	    fprintf(stderr,"%s: cannot execute shell %s\n", progname,shell);
	    exit(-1);
	}
	old_intr = signal(SIGINT,SIG_IGN);
	old_quit = signal(SIGQUIT,SIG_IGN);
	while ((wpid = wait((int *)0)) != cpid && wpid != -1)
	    ;
	signal(SIGINT,old_intr);
	signal(SIGQUIT,old_quit);
}


/* WPOPEN/WPCLOSE - Run command on a pipe
 *
 *    This is similar to the Unix popen() and pclose() calls, except
 *      (1)     upopen() closes the last upopen() whenever it is called.
 *      (2)     upclose() closes the last upopen(), and takes no args.
 *      (3)     the user's own shell is used to process commands.
 */

FILE *f_lastpop = NULL;         /* current upopened stream  (NULL means none) */
int p_lastpop;                  /* process id of last upopened command */

FILE *wpopen(char *cmd, char *mode)
{
int pip[2];
register int chd_pipe,par_pipe;
FILE *fdopen();
char *shell;

	if (f_lastpop) xwpclose();

	/* Make a pipe */
	if (pipe(pip)) return((FILE *)0);

	/* Choose ends */
	par_pipe= (*mode == 'r') ? pip[0] : pip[1];
	chd_pipe= (*mode == 'r') ? pip[1] : pip[0];

	switch (p_lastpop= fork())
	{
	case 0:
		/* Child - run command */
		close(par_pipe);
		if (chd_pipe != (*mode == 'r'?1:0))
		{
			dup2(chd_pipe,(*mode == 'r'?1:0));
			close(chd_pipe);
		}
		/*setuid(getuid()); setgid(getgid());*/
		endutent();	/* Dunno if close-on-exec is set */
		if ((shell= getenv("SHELL")) == NULL) shell= "/bin/sh";
		execl(shell,leafname(shell),"-c",cmd,(char *)NULL);
		fprintf(stderr,"%s: cannot execute shell %s\n", progname,shell);
		exit(-1);
	case -1:
		close(chd_pipe);
		close(par_pipe);
		return((FILE *)0);
	default:
		close(chd_pipe);
		return(f_lastpop=fdopen(par_pipe,mode));
	}
}

void xwpclose()
{
int pid;
	if (f_lastpop == NULL || fclose(f_lastpop)) return;

	f_lastpop=NULL;
	while ((pid=wait((int *)0)) != -1 && pid != p_lastpop )
		;
}
#endif
