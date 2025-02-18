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
	    utmp.state= 1;
	    return 0;
    }
}


/* ENDUTENT - Close the utmp file.
 */

void endutent()
{
    if (utmp.state == 1)
    	utmp.state= 0;
}


/* SETUTENT - Rewind the utmp file.
 */

void setutent()
{
	;
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

struct utmpx *getutent()
{
static struct utmpx *ut;

    switch (utmp.state)
    {
    case 0:
	openut();
    	/* Drop through */
    case 1:
	if ((ut = getutxent()) != NULL)
	    return ut;
    	/* Drop through */
    default:
    	return (struct utmpx *)NULL;
    }
}


/* GETUTLINE - Return utmp entry for the next entry in the utmp file whose
 * ut_line field matches in->ut_line.  It's obnoxious that this wants a
 * whole utmp structure passed in instead of just a char pointer, but
 * we conform with Linux and Solaris.
 */

struct utmpx *getutline(const struct utmpx *in)
{
static struct utmpx *ut;

    switch (utmp.state)
    {
    case 0:
	openut();
    	/* Drop through */
    case 1:
	while ((ut = getutxent()) != NULL)
	{
	    if (
#if defined(USER_PROCESS) && defined(LOGIN_PROCESS)
	        (ut->ut_type == USER_PROCESS || ut->ut_type == LOGIN_PROCESS) &&
#endif
		!strncmp(ut->ut_line, in->ut_line, sizeof(ut->ut_line)))
	    {
		return ut;
	    }
    	}
    	/* Drop through */
    default:
    	return (struct utmpx *)NULL;
    }
}

#endif /* HAVE_GETUTENT */
