/* GETUTENT ROUTINES FOR SYSTEMS THAT HAVEN'T GOT THEM */

#include "orville.h"

#ifndef HAVE_GETUTENT
#include <sys/stat.h>

static struct {
	char *fname;	/* Filename.  If NULL, name is _PATH_UTMP */
	int fd;		/* file descriptor for open utmp file */
	int state;	/* 0 = not open; 1 = open; -1 = cannot open */
	} utmp= {NULL,0,0};


/* OPENUTENT - Ensure the utmp file is open.  Return non-zero if it can't be
 * opened.  This routine is not exported.
 */

static int openut()
{
    if (utmp.state == 0)
    {
	if ((utmp.fd= open(utmp.fname ? utmp.fname : _PATH_UTMP,
		O_RDONLY)) < 0)
	{
	    utmp.state= -1;
	    return -1;
	}
	else
	{
	    utmp.state= 1;
	    fcntl(utmp.fd, F_SETFD, 1);		/* Close over execs */
	    return 0;
	}
    }
}


/* ENDUTENT - Close the utmp file.
 */

void endutent()
{
    if (utmp.state == 1)
    	close(utmp.fd);
    utmp.state= 0;
}


/* SETUTENT - Rewind the utmp file.
 */

void setutent()
{
    if (utmp.state == 1)
    	lseek(utmp.fd, 0L, 0);
}


/* UTMPNAME - Set the name of the utmp file.
 */

int utmpname(const char *file)
{
    /* Close any currently open file */
    endutent();

    if (utmp.fname != NULL) free(utmp.fname);
    utmp.fname= strdup(file);

    return 0;
}


/* GETUTENT - Read the next entry from the utmp file into static storage.
 */

struct utmp *getutent()
{
static struct utmp ut;

    switch (utmp.state)
    {
    case 0:
	openut();
    	/* Drop through */
    case 1:
	if (read(utmp.fd, &ut, sizeof(struct utmp)) == sizeof(struct utmp))
	    return &ut;
    	/* Drop through */
    default:
    	return (struct utmp *)NULL;
    }
}


/* GETUTLINE - Return utmp entry for the next entry in the utmp file whose
 * ut_line field matches in->ut_line.  It's obnoxious that this wants a
 * whole utmp structure passed in instead of just a char pointer, but
 * we conform with Linux and Solaris.
 */

struct utmp *getutline(const struct utmp *in)
{
static struct utmp ut;

    switch (utmp.state)
    {
    case 0:
	openut();
    	/* Drop through */
    case 1:
	while (read(utmp.fd, &ut, sizeof(struct utmp)) == sizeof(struct utmp))
	{
	    if (
#if defined(USER_PROCESS) && defined(LOGIN_PROCESS)
	        (ut.ut_type == USER_PROCESS || ut.ut_type == LOGIN_PROCESS) &&
#endif
		!strncmp(ut.ut_line, in->ut_line, UT_LINESIZE))
	    {
		return &ut;
	    }
    	}
    	/* Drop through */
    default:
    	return (struct utmp *)NULL;
    }
}

#endif /* HAVE_GETUTENT */
