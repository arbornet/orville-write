#include "write.h"
#include <pwd.h>
#include <sys/stat.h>

int keep= 0, silent= 0;


/* MY_DIR - Return name of my home directory, or NULL if I can't find it.
 */

char *my_dir()
{
char *r;
struct passwd *pwd;

    /* Try the environment variable first */
    if ((r= getenv("HOME")) != NULL) return r;

    /* Try the password file */
    setpwent();
    pwd= getpwuid(getuid());
    endpwent();
    if (pwd != NULL) return pwd->pw_dir;

    /* Give up */
    return NULL;
}


/* RECORD_ON -- return true if the user has his record flag on.  We don't
 * really care, but we want to tell him about it if this is the reason he
 * hasn't any messages.
 */

int record_on()
{
struct utmpx *ut;
struct wrttmp wt;
char *tty;
long pos;

    /* Open the utmp file */
    setutxent();

    /* Open the wrttmp file */
    if (init_wstream(O_RDONLY)) return 1;

    /* Get tty name */
    if (getdevtty()) exit(1);
    tty= mydevname+5;

    /* Find our entry in the utmp file */
    if ((ut= find_utmp(tty)) == NULL || ut->ut_user[0] == '\0') return 1;

    /* Find the entry in the wrttmp file */
    find_wrttmp(tty, ut->ut_tv.tv_sec, &wt, &pos);

    /* Close utmp file */
    endutxent();

    return (wt.wrt_record != 'n');
}


int main(int argc, char **argv)
{
char fname[200];
char *dir;
FILE *fp;
int ch;
int i,j;
struct stat st;

    progname= leafname(argv[0]);
    readconfig(NULL);

    for (i= 1; i < argc; i++)
    {
	if (argv[i][0] == '-')
	    for (j= 1; argv[i][j] != '\0'; j++)
		switch(argv[i][j])
		{
		case 'k':
			keep= 1;
			break;
		case 's':
			silent= 1;
		default:
			goto usage;
		}
	else
	    goto usage;
    }

    if ((dir= my_dir()) == NULL)
    {
	fprintf(stderr,"%s: cannot find your home directory\n",progname);
	exit(1);
    }

    sprintf(fname,"%.180s/.lastmesg",dir);

    if ((fp= fopen(fname,"r")) == NULL)
    {
	printf("No recorded messages%s.\n", (silent || record_on()) ? "":
	    " (do \042mesg -r y\042 to record future messages)");
	exit(0);
    }

    if (fstat(fileno(fp), &st) || st.st_uid != getuid())
    {
    	printf("Incorrect ownership of %s\n",fname);
    	exit(1);
    }

    /* Unlink the file -- do it now to minimize conflicts with incoming
     * messages.
     */
    if (!keep) unlink(fname);

    /* Display the file */
    if (!silent)
	while ((ch= getc(fp)) != EOF)
	{
	    if (ch != '\007') putchar(ch);
	}

    exit(0);

usage: fprintf(stderr,"usage: %s [-ks]\n", progname);
    exit(1);
}
