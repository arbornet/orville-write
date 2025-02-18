/* WRITE FINDHIM ROUTINES -- This searchs the wrttmp file for the person
 * to write.
 */

#include "write.h"
#include <pwd.h>
#include <sys/stat.h>

#ifndef TCFLSH
#include <sys/file.h>
#endif /*TCFLSH*/

#ifdef HAVE_GETSPNAM
# define PW_GETSPNAM
# include <shadow.h>
  struct spwd *getspnam();
# ifdef HAVE_KG_PWHASH
   char *pw_encrypt();
   char *kg_pwhash(char *clear, char *user, char *result, int resultlen);
# else
#  ifdef HAVE_PW_ENCRYPT
    char *pw_encrypt();
#   define pcrypt(l,p,s) pw_encrypt(p, s)
#  else
#   define pcrypt(l,p,s) crypt(p, s)
#  endif
# endif
#else
# ifdef HAVE_GETUSERPW
#  define PW_GETUSERPW
#  define pcrypt(l,p,s) crypt(p, s)
#  include <sys/userpw.h>
# else
#  define PW_GETPWNAM
#  define pcrypt(l,p,s) crypt(p, s)
# endif
#endif /* HAVE_GETSPNAM */

long hispos;			/* The offset of his entry in the wrttmp file */
bool helpseeker= FALSE;		/* Did we write to a random helper? */

char *what[2] = {"Writing", "Telegram"};

extern struct wrttmp hiswrt;
extern struct wrthdr wt_head;

void find_him(void);
int find_tty(void);
void find_answer(void);
void find_helper(void);
int perms_on(struct wrttmp *thewrt, time_t *atime);

/* Password encryption for Grex
 */

#ifdef HAVE_KG_PWHASH
char *pcrypt(char *login, char *plain, char *salt)
{
static char bf[40];
char *cpass;

    if (salt[0] != '%')
    	cpass= pw_encrypt(plain, salt);
    else if ((cpass= kg_pwhash(plain, login, bf, 40)) == NULL)
    {
    	bf[0]= '\0';
	cpass= bf;
    }
    return cpass;
}
#endif /* HAVE_KG_PWHASH */

/* FIXPERF: Fix old-style wrt_telpref fields in the given wrttmp structure.
 */

void fixpref(struct wrttmp *wrt)
{
    if (wrt->wrt_telpref & TELPREF_OLD)
    {
    	switch (wrt->wrt_telpref)
	{
	case 'w': wrt->wrt_telpref= TELPREF_WRITE; break;
	case 't': wrt->wrt_telpref= TELPREF_TEL; break;
#ifdef WITH_TALK
	case 'k': wrt->wrt_telpref= TELPREF_TALK; break;
#endif
	case 'a': case '-': wrt->wrt_telpref= TELPREF_ALL; break;
	}
    }
}

/* FIND_US: This digs the two of us out of the utmp file and sets up myname,
 * mytty, mydevname, mypos, hisname, histty, hisdevname and hispos.
 *
 * Finding myself is straightforward, but there are several cases for finding
 * him:
 *  (1)  Both name and tty are given:  Check that that person is really
 *       on that tty.
 *  (2)  Just the tty is given.  Find the name on that tty, if any.
 *  (3)  Just the name is given.  Find all tty's occupied by someone by
 *       that name.  If there are more than one, look for one who is
 *	 writing me or one with his permissions on.
 *  (4)  Neither name nor tty is given.  Find if anyone is writing me.
 *       use that.
 *  (5)  The name "help" was given.  Find a helper who is not busy.
 *  (6)  The name "." was given.  Try writing to the last person written.
 *
 *  If any of these fail, an appropriate error message is printed and
 *  we exit.  If the succeed, a message is printed describing who we
 *  ended up writing.
 */

void find_us()
{
int rc;
extern struct wrttmp mywrt;

    /* Open utmp file */
    setutxent();

    /* Look me up */
    find_me();

    /* Find his name and wrttmp entry */

    if (*hisname == '\0' && *histty == '\0')
    {
	/* No name was given - search wrttmp for someone writing me */
	find_answer();
    }
    else if (f_helpers && *histty == '\0' && !strcmp(hisname,f_helpername))
    {
	/* Search for a helper who has his perms on and is not busy */
	find_helper();
    }
    else if (*histty == '\0')
    {
	/* If name is "." get name from last field */
	if (hisname[0] == '.' && hisname[1] == '\0')
	{
	    if (mywrt.wrt_last[0] == '\0')
	    {
		printf("No previous write or telegram.  Can't repeat\n");
		wrtlog("FAIL: no previous");
		done(1);
	    }
	    strncpy(hisname,mywrt.wrt_last,UT_NAMESIZE);
	}

	/* Just the name given...look for him */
	find_him();
    }
    else
    {
	if ((rc= find_tty()) > 0)
	{
	    /* In telegram mode, if ttyname doesn't work, it is probably the
	     * first word of the message, and not the ttyname at all */
	    if (telegram)
	    {
		telmsg= linebuf;
		histty[0]= '\0';
		if (hisname[0] == '\0')
		    find_answer();
		else if (f_helpers && !strcmp(hisname,f_helpername))
		    find_helper();
		else
		    find_him();
	    }
	    else if (rc == 1)
	    {
		printf("No such tty\n");
		wrtlog("FAIL: no such tty");
		done(1);
	    }
	    else
	    {
		printf("%s not logged onto %s\n",hisname,histty);
		wrtlog("FAIL: not on tty");
		done(1);
	    }
	}
    }

    sprintf(hisdevname,"/dev/%.*s",UT_LINESIZE,histty);

    fixpref(&hiswrt);
    fixpref(&mywrt);

    endutent();
}


/* FIND_HIM:  Given just his name, search wrttmp for the person to write.
 * If he is logged on more than once, we'll take any one of them, but we'd
 * prefer one who is writing us, or at least has his perms on.  If he isn't
 * on, print an error message and exit.
 */

void find_him()
{
int cnt= 0;
int write_me;
int perm, hisperm= 0;
time_t hisatime= 0;
time_t atime;
struct utmpx *ut;
struct wrttmp tmpwrt;
long tmppos;

    setutent();
    while ((ut= getutent()) != NULL)
    {
        /* Check if this is the target user, ignoring X-window lines */
        if (ut->ut_line[0] != ':' &&
	    ut->ut_type == USER_PROCESS &&
	    !strncmp(hisname, ut->ut_user, UT_NAMESIZE))
	{
	    /* Count matches */
	    cnt++;

	    /* Find wrttmp entry */
	    find_wrttmp(ut->ut_line, ut->ut_tv.tv_sec, &tmpwrt, &tmppos);

	    /* Is this guy writing me? */
	    write_me= !strncmp(tmpwrt.wrt_what,myname,UT_NAMESIZE);

	    /* Do I have write access?  How long has he been idle? */
	    if (!write_me)
	    	perm= perms_on(&tmpwrt,&atime);

	    /* If writing me, is less idle, or is first to be permitted, save */
	    if (write_me || (perm && !hisperm) ||
	        (atime > hisatime && (perm == hisperm)) )
	    {
		strncpy(histty, ut->ut_line, UT_LINESIZE);
		hiswrt= tmpwrt;
		hispos= tmppos;
		hisatime= atime;
		hisperm= perm;

		/* If this guy is writing me, look no further */
		if (write_me) break;
	    }
	}
    }

    /* Check for abnormal results */
    if (cnt == 0)
    {
	/* didn't find any matches, trouble */
	printf("%s is not logged on\n",hisname);
	{
	    wrtlog("FAIL: not on");
	    done(1);
	}
    }
    else if (cnt == 1)
    {
	/* Found one match - the usual */
	printf("%s to %s on %s...",what[telegram],hisname,histty);
        if (!telegram) putchar('\n');
    }
    else
    {
	/* Found multiple matches -- tell which we used */
	printf("%s logged on more than once\n",hisname);
	printf("%s to %s...",what[telegram],histty);
	if (!telegram) putchar('\n');
    }
}


/* FIND_TTY:  Given his ttyname, and possibly his login name, find a matching
 * user.  If none match, print an error message and exit.  Set if hisname if
 * it wasn't given.  Return code:
 *    0 - success.
 *    1 - no such tty.
 *    2 - named user not on named tty.
 */

int find_tty()
{
struct utmpx *ut;

    if ((ut= find_utmp(histty)) == NULL)
	return(1);

    if (*hisname != '\0')
    {
	/* Does the name not match? */
	if (strncmp(hisname, ut->ut_user, UT_NAMESIZE))
	    return(2);
    }
    else
    {
	/* Is anyone on that line? */
	if (*ut->ut_user == '\0')
	{
	    printf("No one logged onto %s\n",histty);
	    wrtlog("FAIL: empty tty");
	    done(1);
	}
	strncpy(hisname, ut->ut_user, UT_NAMESIZE);
    }
    printf("%s to %s on %s...",what[telegram],hisname,histty);
    if (!telegram) putchar('\n');
    find_wrttmp(histty,ut->ut_tv.tv_sec,&hiswrt,&hispos);
    return(0);
}


/* FIND_ANSWER:  Find a user who is writing me.  Set up histty and hisname if
 * a match is found.  Note that if the time in the wrttmp entry doesn't match
 * the time in the utmp entry, then we presume it is a dude wrttmp left over
 * from some other login and we ignore it.  If not found, print an error
 * message and terminate.
 */

void find_answer()
{
struct utmpx *ut;
int slot;

    lseek(wstream,hispos= wrttmp_offset(slot= 0),0);
    while (read(wstream, &hiswrt, sizeof(struct wrttmp)) ==
	   sizeof(struct wrttmp))
    {
	if (!strncmp(myname, hiswrt.wrt_what, UT_NAMESIZE))
	{
	    /* Found someone writing me - get his name from utmp */
	    strncpy(histty,hiswrt.wrt_line,UT_LINESIZE);
	    if ((ut= find_utmp(histty)) != NULL && ut->ut_user[0] != '\0')
	    {
		strncpy(hisname,ut->ut_user,UT_NAMESIZE);
		printf("Replying to %s on %s...",hisname,histty);
		if (!telegram) putchar('\n');
		return;
	    }
	}
	lseek(wstream,hispos= wrttmp_offset(++slot),0);
    }

    printf("You are not being written\n");
    wrtlog("FAIL: not written");
    done(0);
}


/* FIND_HELPER:  Find an unoccupied user who has his helper flag set.  If
 * found, set up histty and hisname.  If there is none, print an error message
 * and terminate.  As usual, ignore wrttmp entries who's login times don't
 * agree with the times in utmp.  If there is more than one helper available,
 * one is selected at random.
 */

void find_helper()
{
extern struct wrttmp mywrt;
int nhelpers= 0;	/* Number of helpers on */
int ahelpers= 0;	/* Number of helpers available */
int previous;
int slot= 0;
struct utmpx *ut;
struct wrttmp tmpwrt;
long tmppos;

    helpseeker= TRUE;

    SEEDRAND((unsigned)time((time_t *)0));

    for (;;)
    {
	/* Read in the next line of the file, remembering our offset */
	lseek(wstream, tmppos= wrttmp_offset(slot++), 0);
	if (read(wstream, &tmpwrt, sizeof(struct wrttmp)) !=
	    sizeof(struct wrttmp))
	    break;

	/* Reject obvious non-helpers */
	if (tmpwrt.wrt_help == 'n' ||		 /* Helper flag off */
	    !strncmp(tmpwrt.wrt_line, mytty, UT_LINESIZE))
						 /* I can't help myself! */
	    continue;
	
	/* Find the helper candidate in utmp - if he's not there skip out */
	if ((ut= find_utmp(tmpwrt.wrt_line)) == NULL ||
	    ut->ut_user[0] == '\0' || ut->ut_tv.tv_sec != tmpwrt.wrt_time)
	    continue;
	
	/* Reject helpers with their message permissions off */
	if (tmpwrt.wrt_help != 'Y')
	{
	    if (!perms_on(&tmpwrt,NULL))
	    {
	    	/* Perms off - but am I in .yeswrite file? */
	    	if (!f_exceptions || tmpwrt.wrt_except != 'y' ||
	    	    !isuexception(ut->ut_user, 1, myname))
	    	    	continue;
	    }
	    else
	    {
	    	/* Perms on - but am I in .nowrite file? */
	    	if (f_exceptions && tmpwrt.wrt_except == 'y' &&
	    	    isuexception(ut->ut_user, 0, myname))
	    	    	continue;
	    }
        }

	/* Whoopie! We have found a real live helper */
	nhelpers++;

	/* Reject helpers who are busy writing someone else */
	if (tmpwrt.wrt_what[0] != '\0' &&	/* He busy and ...   */
	    strncmp(tmpwrt.wrt_what, myname, UT_NAMESIZE))
						/* ...not writing me */
	    continue;

	/* Zowie!  He's not only there, he's available to help us */
	ahelpers++;

	/* Has he helped us before? */
	previous= !strncmp(ut->ut_user, mywrt.wrt_last, UT_NAMESIZE);

	/* So roll the dice to see if we will choose him */
	if (!previous && (unsigned)RAND() > (unsigned)RAND_MAX / ahelpers)
	    continue;

	/* We chose him, so make him our helper candidate so far */
	strncpy(histty, tmpwrt.wrt_line, UT_LINESIZE);
	strncpy(hisname, ut->ut_user, UT_NAMESIZE);
	hiswrt= tmpwrt;
	hispos= tmppos;

	/* If he helped us before, choose him now */
	if (previous) break;
    }

    if (ahelpers == 0)
    {
	if (nhelpers == 0)
	{
	    printf("Sorry, no helpers are available right now.\n");
	    if (f_nohelp) type_help(f_nohelp);
	    wrtlog("FAIL: no helpers");
	}
	else
	{
	    printf("Sorry, all helpers currently available are busy.\n");
	    if (f_nohelp) type_help(f_nohelp);
	    printf("Try again later...\n");
	    wrtlog("FAIL: help busy");
	}
	done(0);
    }

    printf("%s to helper %s on %s...",what[telegram],hisname,histty);
    if (!telegram) putchar('\n');
}


/* PERMS_ON: Return true if permissions for the user whose wrttmp entry is
 * passed in are on.  This is a less thorough version of iswritable() used
 * while selecting a user to write.  Also returns last access time of tty, if
 * atime is not null.
 *
 */

int perms_on(struct wrttmp *thewrt, time_t *atime)
{
char devname[UT_LINESIZE+7];
struct stat stb;
short flag;

#ifndef TTYPERMS
    if (atime != NULL || thewrt->wrt_mesg != 'n')
    {
    	/* Even if wrttmp perms are off, user may still be writable if
	 * physical perms are on, so we need to stat the tty anyway in that
	 * case, or if we want the last access time.
	 */
#endif /*TTYPERMS*/

	/* Stat the terminal */
    	sprintf(devname,"/dev/%.*s",UT_LINESIZE,thewrt->wrt_line);
	if (stat(devname,&stb))
	{
	    printf("%s: Panic - can't stat %s\n",progname,devname);
	    done(1);
	}
	if (atime != NULL) *atime= stb.st_atime;

#ifndef TTYPERMS
    }
    if (thewrt->wrt_mesg != 'n')
	return(TRUE);
#endif /*TTYPERMS*/

    /* Is his tty physically writable? */
    if (getuid() == stb.st_uid)
    	flag= stb.st_mode & 0200;
    else if (getegid() == stb.st_gid || getgid() == stb.st_gid)
	flag= stb.st_mode & 0020;
    else
	flag= stb.st_mode & 0002;

    return(flag != 0);
}


/* ISWRITABLE:  Check if he is writtable, either because his permissions are
 * on, he is writing me, I am root, his tty is writable, or I can give the
 * right root password with the -r option.
 */

bool iswritable()
{
char *cpass;
#ifdef PW_GETPWNAM
struct passwd *pwd;
#endif

    /* Can I give the root password?
     * We always test this first, so that we always ask for the root password
     * when -r is given.  This reduces the chance of accidentally sending the
     * root password to the writtee because you expected it to ask for the
     * root password and it didn't.
     */
    if (ask_root)
    {
#ifdef PW_GETPWNAM
	pwd= getpwuid(0);
	cpass= pcrypt("root",getpass("Password:"),pwd->pw_passwd);
	flushinput(0);		/* discard typeahead */
	if (am_root= !strcmp(cpass,pwd->pw_passwd))
	    return (TRUE);
#endif
#ifdef PW_GETSPNAM
    struct spwd *spwd;
	spwd= getspnam("root");
	cpass= pcrypt("root",getpass("Password:"),spwd->sp_pwdp);
	flushinput(0);		/* discard typeahead */
	if (am_root= !strcmp(cpass,spwd->sp_pwdp))
	    return (TRUE);
#endif
#ifdef PW_GETUSERPW
    struct userpw *upwd;
	upwd= getuserpw("root");
	cpass= pcrypt("root",getpass("Password:"),upwd->upw_passwd);
	flushinput(0);		/* discard typeahead */
	if (am_root= !strcmp(cpass,upwd->upw_passwd))
	    return (TRUE);
#endif
	printf("Password incorrect\n");
    }

    /* Is he writing me? */
    if (!strncmp(hiswrt.wrt_what, myname, UT_NAMESIZE))
	return(TRUE);

    /* Am I already root? */
    if (am_root= (getuid() == 0))
	return(TRUE);

    /* Are his permissions on? */
    if (perms_on(&hiswrt,NULL))
    {
	/* Permissions are on, but are we an exception? */
	if (!f_exceptions || hiswrt.wrt_except != 'y' ||
	    !ishisexception(0,myname))
	    return(TRUE);
    }
    else
    {
	/* Permissions are off, but are we an exception? */
	if (f_exceptions && hiswrt.wrt_except == 'y' &&
	    ishisexception(1,myname))
	    return(TRUE);
    }

    if (f_wrthist != NULL && may_answer_tel())
	return(TRUE);

    /* Is he running "amin -p"? */
    if (hiswrt.wrt_what[0] != '\0' && hiswrt.wrt_record == 'a')
    {
	/* NOTE:  rec_only is only set if his perms are otherwise off.
	 * Later it gets set for all other cases too.  */
	rec_only= TRUE;
	return(TRUE);
    }

    if (helpseeker && hiswrt.wrt_help == 'Y')
	return(TRUE);

    return(FALSE);
}


/* HE_REPLIED -- This routine returns true if the person we are writing appears
 * to be writing us back.  It should only be called after a call to find_us().
 */

bool he_replied()
{
struct wrttmp wbuf;

    lseek(wstream, hispos, 0);
    if (read(wstream, &wbuf, sizeof(struct wrttmp)) != sizeof(struct wrttmp))
	return(FALSE);
    return(!strncmp(wbuf.wrt_what, myname, UT_NAMESIZE));
}
