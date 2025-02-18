/* Dump WRTTMP file - This is a debugging command which prints out the current
 * contents of the wrttmp file in a human-readable format.  Since it is rather
 * cryptic, I wouldn't recommend installing it for general use.
 */

#include "orville.h"

/* Return a pointer to a string of n dashes.  No more than MAXDASH allowed. */
char *dashes(n)
int n;
{
#define MAXDASH 30
static char *d= "------------------------------";  /* Exactly MAXDASH -'s */

	return(n<=MAXDASH ? d+(MAXDASH-n) : d);
}

main(int argc, char **argv)
{
FILE *fp;
struct wrttmp w;
struct wrthdr wt_head;
int slot= 0;
long x;
	
	progname= leafname(argv[0]);
	readconfig(NULL);

	if ((fp= fopen(f_wrttmp,"r")) == NULL)
	{
		printf("cannot open %s\n",f_wrttmp);
		exit(1);
	}

	if (!fread(&wt_head,sizeof(struct wrthdr),1,fp))
	{
		printf("%s exists, but has no header\n",f_wrttmp);
		exit(1);
	}
	printf("HEAD SIZE = %ld   ENTRY SIZE = %ld\n\n",
		wt_head.hdr_size, wt_head.tmp_size);

	printf("--line%s --what%s --last%s * E  P M H R B --pid-- ---login time---\n",
		dashes(UT_LINESIZE-6),
		dashes(UT_NAMESIZE-6),
		dashes(UT_NAMESIZE-6));
	for (;;)
	{
		x= wrttmp_offset(slot++);
		fseek(fp,x,0);
		if (!fread(&w,sizeof(struct wrttmp),1,fp))
			break;

		printf("%-*.*s %-*.*s %-*.*s %c %c %2d %c %c %c %c %7d  %15.15s\n",
			UT_LINESIZE,UT_LINESIZE,w.wrt_line,
			UT_NAMESIZE,UT_NAMESIZE,w.wrt_what,
			UT_NAMESIZE,UT_NAMESIZE,w.wrt_last,
#ifndef TTYPERMS
			isprint(w.wrt_mesg) ? w.wrt_mesg : '#',
#else
			' ',
#endif
			isprint(w.wrt_except) ? w.wrt_except : '#',
			w.wrt_telpref,
			isprint(w.wrt_modepref) ? w.wrt_modepref : '#',
			isprint(w.wrt_help) ? w.wrt_help : '#',
			isprint(w.wrt_modepref) ? w.wrt_record : '#',
			isprint(w.wrt_bells) ? w.wrt_bells : '#',
			w.wrt_pid,
			ctime(&w.wrt_time)+4);
	}
}
