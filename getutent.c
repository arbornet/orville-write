/* GETUTENT ROUTINES FOR SYSTEMS THAT HAVEN'T GOT THEM */

#include "orville.h"

#ifndef HAVE_GETUTENT

/* OPENUTENT - Ensure the utmp file is open.  Return non-zero if it can't be
 * opened.  This routine is not exported.
 */


/* ENDUTENT - Close the utmp file.
 */

void endutent()
{
    endutxent();
}


/* SETUTENT - Rewind the utmp file.
 */

void setutent()
{
    setutxent();
}


/* UTMPNAME - Set the name of the utmp file.
 */

static char *utmpfname;
int utmpname(const char *file)
{
    /* Close any currently open file */
    endutxent();

    free(utmpfname);
    utmpfname= strdup(file);

    return 0;
}


/* GETUTENT - Read the next entry from the utmp file into static storage.
 */

struct utmpx *getutent()
{
    return getutxent();
}


/* GETUTLINE - Return utmp entry for the next entry in the utmp file whose
 * ut_line field matches in->ut_line.  It's obnoxious that this wants a
 * whole utmp structure passed in instead of just a char pointer, but
 * we conform with Linux and Solaris.
 */

struct utmpx *getutline(const struct utmpx *in)
{
    struct utmpx *ut;
    while ((ut = getutxent()) != NULL)
    {
        if ((ut->ut_type == USER_PROCESS || ut->ut_type == LOGIN_PROCESS) &&
            strncmp(ut->ut_line, in->ut_line, sizeof(ut->ut_line)) == 0)
        {
            return ut;
        }
    }
    return (struct utmpx *)NULL;
}

#endif /* HAVE_GETUTENT */
