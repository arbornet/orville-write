/* Header file for utmp reading routines */

#ifndef GETUTENT_H
#define GETUTENT_H

#ifdef HAVE_PATHS_H
# include <paths.h>
#endif
#ifndef _PATH_UTMP
# define _PATH_UTMP "/etc/utmp"
#endif

#ifndef HAVE_GETUTENT
int utmpname(const char *file);
void endutent(void);
void setutent(void);
struct utmp *getutent(void);
struct utmp *getutline(const struct utmp *ut);
#endif /*HAVE_GETUTENT*/

#endif /* GETUTENT_H */
