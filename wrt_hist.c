/* WRITE HISTORY ROUTINES -- Stuff to keep track of the last time I wrote
 * various people and enforce rules about flooding.
 */

#include "write.h"

int hist_fd= -1;	/* file descriptor for wrthist file */
struct wrthist myhist;	/* Copy of my wrthist entry */ 
extern struct wrttmp hiswrt;


/* OPEN_HIST - Open the history file for read/write, if it is not already
 * open.
 */

void open_hist()
{
	if (hist_fd >= 0) return;

	if ((hist_fd= open(f_wrthist,O_RDWR|O_CREAT,0600)) < 0)
	{
		printf("%s: Unable to open %s to read/write\n",
			progname, f_wrthist);
		done(1);
	}
	fcntl(hist_fd, F_SETFD, 1);	/* close over execs */
}


/* EMPTY_HIST - blank out a history field to say no write has occurred for
 * one heck of a long time.
 */

void empty_hist(hist)
struct wrthist *hist;
{
int i;
	hist->tm= 0;
#if MAX_TEL_N > 1
	for (i= 0; i < MAX_TEL_N - 1; i++)
		hist->intv[i]= MAX_TEL_T + 1;
#endif
}


#ifdef DEBUG
/* PRINT_HIST - A debugging output routine */
void print_hist(struct wrthist *hist)
{
int i;

	printf("last=%ld %s",hist->tm,ctime(&hist->tm));
	printf("intervals=( ");
#if MAX_TEL_N > 1
	for (i= 0; i < MAX_TEL_N - 1; i++)
		printf("%d ",hist->intv[i]);
#endif
	printf(")\n");
}
#endif


/* GET_HIST - Get the history file entry for the last times the person in
 * slot "writer" wrote the person in slot "writee".
 */

void get_hist(long writer, long writee, struct wrthist *hist)
{
	if (writee > MAXSLOTS)
	{
		empty_hist(hist);
		return;
	}

	open_hist();

	lseek(hist_fd,whindex(writer,writee),0);

	if (read(hist_fd,hist,sizeof(struct wrthist)) !=
			sizeof(struct wrthist))
		empty_hist(hist);
}


/* PUT_HIST - Write the history file entry for the last times the person in
 * slot "writer" wrote the person in slot "writee".
 */

void
put_hist(long writer, long writee, struct wrthist *hist)
{
	if (writee > MAXSLOTS)
		return;

	open_hist();

	lseek(hist_fd,whindex(writer,writee),0);

	if (write(hist_fd,hist,sizeof(struct wrthist)) !=
			sizeof(struct wrthist))
		printf("%s: Error writing history entry\n",progname);
}


/* MAY_ANSWER_TEL - Check if he sent me a tel in the last f_answertel seconds.
 */

bool may_answer_tel()
{
int his_slot= hispos / sizeof(struct wrttmp);
int my_slot= mypos / sizeof(struct wrttmp);
struct wrthist hist;

	get_hist(his_slot,my_slot,&hist);

	return (hist.tm > hiswrt.wrt_time &&
	        time((time_t *)0) - hist.tm <= f_answertel);
}


/* CHECK_FLOOD - return a number of seconds we should sleep before sending
 * another tel.  This must be called before register_tel().
 */

int check_flood()
{
int his_slot= hispos / sizeof(struct wrttmp);
int my_slot= mypos / sizeof(struct wrttmp);
time_t now= time((time_t *)0);
int total;
int i,tmp,sft;

	get_hist(my_slot,his_slot,&myhist);
	if (myhist.tm > now) empty_hist(&myhist);

	total= now - myhist.tm;
	myhist.tm= now;
#if MAX_TEL_N > 1
	sft= (total > MAX_TEL_T) ? MAX_TEL_T + 1 : total;
	for (i= MAX_TEL_N-2; i >= 0; i--)
	{
		total+= myhist.intv[i];

		tmp= myhist.intv[i];
		myhist.intv[i]= sft;
		sft= tmp;
	}
#endif

	return ((total >= MAX_TEL_T) ? 0 : MAX_TEL_T - total);
}


/* REGISTER_TEL() - Record that we did do a tel.  Must be called after
 * check_flood().
 */

void register_tel()
{
int his_slot= hispos / sizeof(struct wrttmp);
int my_slot= mypos / sizeof(struct wrttmp);

	put_hist(my_slot,his_slot,&myhist);
}
