/* HELPERS - List or count of current helpers.
 *
 * (C) Copyright, Nov 1995 - Jan Wolter
 */

#include "orville.h"
#include <sys/stat.h>


/* Structure for list of ttys with helpers on them. */

struct hlp {
	time_t time;			/* login time from wrttmp file */
	char line[sizeof(((struct utmpx *)0)->ut_line) -1];		/* ttyline occupied by a helper */
	int busy;			/* is he busy? */
	struct hlp *next;		/* next helper */
	} *list= NULL;


/* FINDLIST - search the list for the named tty.  If found, remove it from the
 * list, and return a pointer to it.  If not found, return NULL.
 */

struct hlp *findlist(char *tty)
{
struct hlp *curr, *prev;

    for (curr= list, prev= NULL; curr != NULL; prev= curr,curr= prev->next)
    {
	if (!strncmp(tty, curr->line, sizeof(curr->line)))
	{
	    if (prev == NULL)
	    	list= curr->next;
	    else
	    	prev->next= curr->next;
	    return(curr);
	}
    }
    return(NULL);
}


/* PERMS_ON: Return true if permissions for the user whose wrttmp entry is
 * passed in are on.  This doesn't test the exception files.
 */

int perms_on(struct wrttmp *w)
{
struct stat st;
char devname[sizeof(((struct utmpx *)0)->ut_line) +6];

#ifdef TTYPERMS
#define MASK 022
#else
#define MASK 002
    if (w->wrt_mesg != 'n')
	return(1);
#endif /*TTYPERMS*/

    /* Is his tty physically writable? */

    sprintf(devname,"/dev/%.*s",((struct utmpx *)0)->ut_line -1,w->wrt_line);
    if (stat(devname,&st))
    	return(0);

    return ((st.st_mode & MASK) != 0);
}


main(int argc, char **argv)
{
FILE *fp;
struct wrttmp w;
struct wrthdr wt_head;
struct utmpx *u;
struct hlp *tmp;
int i, j;
int slot= 0;
int count= 0;
int freeonly= 0;
int listthem= 1;

    progname= leafname(argv[0]);
    readconfig(NULL);

    /* Parse command line options */
    for (i= 1; i < argc; i++)
    {
	if (argv[i][0] != '-' || argv[i][1] == '\0')
	    goto usage;

	for (j= 1; argv[i][j] != '\0'; j++)
	    switch(argv[i][j])
	    {
	    case 'f':
		freeonly= 1;
		break;
	    case 'l':
		listthem= 1;
		break;
	    case 'n':
		listthem= 0;
		break;
	    default:
		goto usage;
	    }
    }
    
    /* Open wrttmp file */
    if ((fp= fopen(f_wrttmp,"r")) == NULL)
    {
	printf("cannot open %s\n", f_wrttmp);
	exit(1);
    }

    /* Scan through wrttmp file, looking for help */
    if (fread(&wt_head,sizeof(struct wrthdr),1,fp))
    {
	for (;;)
	{
	    fseek(fp,wrttmp_offset(slot++),0);
	    if (!fread(&w,sizeof(struct wrttmp),1,fp))
		break;

	    /* skip obvious non-helpers */
	    if ((w.wrt_help != 'y' && w.wrt_help != 'Y') ||
		(freeonly && w.wrt_what[0] != '\0'))
		continue;

	    /* check his permissions */
	    if (w.wrt_help != 'Y' && !perms_on(&w))
	    	continue;

	    /* Found an apparent helper - save in linked list */
	    tmp= (struct hlp *)malloc(sizeof(struct hlp));
	    strncpy(tmp->line,w.wrt_line,UT_LINESIZE);
	    tmp->time= w.wrt_time;
	    tmp->busy= (w.wrt_what[0] != '\0');
	    tmp->next= list;
	    list= tmp;
	}
    }
    fclose(fp);

    /* Scan through utmp file, looking for ttys on our list */
    if (list != NULL)
    {
    	/* Do the scan */
	while ((u= getutxent()) != NULL)
    	{
#ifdef USER_PROCESS
	    if (u->ut_type != USER_PROCESS)
	    	continue;
#endif
	    if ((tmp= findlist(u->ut_line)) != NULL)
	    {
	    	/* If the time stamps don't match, this isn't a real helper */
	    	if (u->ut_tv.tv_sec == tmp->time)
	    	{
		    /* Found a real helper -- count and print */
		    count++;

		    if (listthem)
			printf("%-*.*s %-*.*s%s\n",
			    sizeof(u->ut_user)-1, sizeof(u->ut_user)-1, u->ut_user,
			    sizeof(u->ut_line)-1, sizeof(u->ut_user)-1, u->ut_line,
			    tmp->busy ? " [busy]" : "");

	    	}
		free(tmp);
		if (list == NULL) break;
	    }
    	}

    	endutxent();
    }

    if (!listthem)
	printf("%d\n",count);
    else if (count == 0)
    	printf("NONE\n");

    exit(0);

usage:
    printf("usage: %s [-fln]\n",argv[0]);
    exit(1);
}
