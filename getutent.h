/* Header file for utmp reading routines */

#ifndef GETUTENT_H
#define GETUTENT_H

#ifndef HAVE_GETUTENT
void endutent(void);
void setutent(void);
struct utmpx *getutent(void);
struct utmpx *getutline(const struct utmpx *ut);
#endif /*HAVE_GETUTENT*/

#endif /* GETUTENT_H */
