/* WRITE ROUTINES TO MANAGE MY WRTTMP ENTRY */

#include "write.h"
#include <pwd.h>
#include <sys/stat.h>


struct wrttmp mywrt;    /* My old wrttmp entry, to be restored at exit time */
long mypos= -1;		/* offset of my entry in the wrttmp file */


/* FIND_ME:  Find my name and ttyline.  Use these to dig up my wrttmp entry.
 */

void find_me()
{
struct utmpx *ut;
struct passwd *pw;
int myuid;

    /* Search utmp for myself */

    if ((ut= find_utmp(mytty)) == NULL || ut->ut_user[0] == '\0')
    {
	printf("%s: Panic - Unable to find your tty (%s) in "_PATH_UTMP"\n",
	    progname, mytty);
	done(1);
    }
    strncpy(myname, ut->ut_user, UT_NAMESIZE);

    /* Check if this is our real identity */
#ifndef SLOWPASSWD
    myuid= getuid();
    if ((pw= getpwnam(myname)) == NULL || pw->pw_uid != myuid)
    {
	if ((pw= getpwuid(myuid)) == NULL)
	{
	    printf("%s: Panic - No passwd file entry for uid %d (uname %s)\n",
		progname,myuid, myname);
	    done(1);
	}
	strncpy(myuidname,pw->pw_name,UT_NAMESIZE);
	myuidname[UT_NAMESIZE]= '\0';
    }
    else
#endif
	myuidname[0]= '\0';	/* ie: myuidname is the same as myname */

    /* Find my wrt_tmp entry */

    find_wrttmp(mytty, ut->ut_tv.tv_sec, &mywrt, &mypos);
}


/* SET_MODES:  This sets our runtime modes into the wrttmp file and makes any
 * temporary changes to our ttymodes.  A copy of our original modes is left
 * unchanged in mywrt and myperms.  This is called as we go into write mode.
 * For telegrams, the only change we make is to update the "last" field, if
 * necessary.  No transient changes are made.
 */

void set_modes()
{
struct wrttmp tmpwrt;

   if (telegram)
   {
	/* If not writing same user again, update last field in wrttmp file */
	if (strncmp(mywrt.wrt_last, hisname, UT_NAMESIZE))
	{
	    strncpy(mywrt.wrt_last, hisname, UT_NAMESIZE);
	    lseek(wstream,mypos,0);
	    if (write(wstream,&mywrt,sizeof(struct wrttmp)) !=
		    sizeof(struct wrttmp))
	    {
		printf("%s: Panic - Cannot write to wrttmp file\n",progname);
		done(1);
	    }
	}
   }
   else
   {
	/* Set last field - this should persist after we exit so do in mywrt */
	strncpy(mywrt.wrt_last, hisname, UT_NAMESIZE);

	tmpwrt= mywrt;

	/* Make temporary changes to wrttmp entry */
	strncpy(tmpwrt.wrt_what, hisname, UT_NAMESIZE);
	tmpwrt.wrt_pid= getpid();
#ifdef TTYPERMS
	setperms(tmp_mesg);
#else
	if (tmp_mesg != 's') tmpwrt.wrt_mesg= mywrt.wrt_mesg;
#endif /*TTYPERMS*/
	if (postpone) tmpwrt.wrt_record= 'a';

	lseek(wstream,mypos,0);
	fixed_modes= TRUE;
	if (write(wstream,&tmpwrt,sizeof(struct wrttmp)) !=
		sizeof(struct wrttmp))
	{
	    printf("%s: Panic - Cannot write to wrttmp file\n",progname);
	    done(1);
	}

    }
}


/* UPDATE_MODES:  This is called after a shell escape or a suspend to see if
 * the user changed his message permissions while he was gone.  If so, it
 * copies the change into our copy of his old wrttmp entry so it will be
 * restored to that after we exit write.
 */

void update_modes()
{
struct wrttmp tmpwrt;

#ifdef TTYPERMS
	if (saveperms()) done(1);
#endif /*TTYPERMS*/
	lseek(wstream,mypos,0);
	if (read(wstream,&tmpwrt,sizeof(struct wrttmp))==sizeof(struct wrttmp))
	{
		strncpy(tmpwrt.wrt_last, mywrt.wrt_last, UT_NAMESIZE);
#ifndef TTYPERMS
		mywrt.wrt_mesg= tmpwrt.wrt_mesg;
#endif /*TTYPERMS*/
		mywrt.wrt_record= tmpwrt.wrt_record;
		mywrt.wrt_telpref= tmpwrt.wrt_telpref;
		mywrt.wrt_modepref= tmpwrt.wrt_modepref;
		mywrt.wrt_help= tmpwrt.wrt_help;
		mywrt.wrt_bells= tmpwrt.wrt_bells;
		mywrt.wrt_except= tmpwrt.wrt_except;
	}
}


/* RESET_MODES:  Before exiting, restore wrttmp modes to original state (aside
 * from any modifications made through update_modes()).
 */

void reset_modes()
{
	if (fixed_modes)
	{
		lseek(wstream,mypos,0);
		write(wstream,&mywrt,sizeof(struct wrttmp));
 		fixed_modes= FALSE;
#ifdef TTYPERMS
		resetperms();
#endif /*TTYPERMS*/
	}
}


/* NESTED_WRITE:  Returns true if this write was run from within a write
 * process with a shell escape
 */

bool nested_write()
{
	return(!strncmp(mywrt.wrt_what, hisname, UT_NAMESIZE));
}


/* INIT_LASTMESG - Figure out what my lastmesg file is and empty it.
 */

char lmfile[100]= "";

void init_lastmesg()
{
char *dir;
struct passwd *pwd;

    /* A little insurance that we aren't root anymore */
    if (getuid() != geteuid())
    	return;

    /* Try the environment variable first */
    if ((dir= getenv("HOME")) == NULL)
    {
	/* Otherwise get directory from passwd file */
	setpwent();
	pwd= getpwuid(getuid());
	endpwent();
	if (pwd == NULL)
	{
	    postpone= FALSE;
	    return;
	}
	dir= pwd->pw_dir;
    }
    sprintf(lmfile, "%.80s/.lastmesg", dir);
    unlink(lmfile);
}


/* SHOW_LASTMESG - Print the .lastmesg file.
 */

void show_lastmesg()
{
FILE *fp;
int ch;

    /* A little insurance that we aren't root anymore */
    if (getuid() != geteuid())
    	return;

    if (lmfile[0] == '\0' || (fp= fopen(lmfile,"r")) == NULL) return;
    unlink(lmfile);

    printf("Messages received while you were busy...\n");
    while ((ch= getc(fp)) != EOF)
	if (ch != '\007') putchar(ch);
}
