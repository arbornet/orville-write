/* WRTTMP FILE
 *
 * The wrttmp file consists of a single "wrthdr" entry, followed by multiple
 * wrttmp entries.  The wrttmp entries each describe one user on one tty line,
 * and correspond to lines in the utmp file.  "wrthdr" is mostly version info.
 *
 * This is the structure for each wrttmp file entry.  Write can be configured
 * to either use permissions stored in the wrtmp file, or to use the physical
 * device permissions (if TTYPERMS is defined).  The latter configuration is
 * preferred, for easier coexistance with other programs, espeically if your
 * old write program ran group "tty".
 *
 * Preferences (telegram vs write and line-mode vs character-mode) are also
 * stored here.  Normally these are set to "-" meaning that the choice is up
 * to the sender.  But if a preference is set by the reciever, it overrules
 * the sender's choice.
 *
 * Also the name of the person currently being written is stored here.  This is
 * used by the auto-reply feature and the gizmo that asks about interupting.
 *
 * The name of the last person a telegram was sent to is also kept.  This is so
 * the next telegram can be sent to the same person.
 *
 * New entries in the wrthdr or wrttmp structures should be added to the end,
 * leaving the old structures alone.  This will ensure that the file can still
 * be shared with programs compiled with an older version of wrttmp.
 */

#ifndef WRTTMP_H
#define WRTTMP_H

#include <sys/types.h>
#include <utmpx.h>

/* BSDI is only Unix I know of that threatens to change namesize from 8 to
 * anything else.  Most don't even have a define for it.  Here we default
 * NAMESIZE and LINESIZE to 8, if the operating system doesn't set them
 * for us.
 */
#ifndef UT_NAMESIZE
#define UT_NAMESIZE 32
#endif
#ifndef UT_LINESIZE
#define UT_LINESIZE 16
#endif

#if defined(TTY_GROUP) || defined(TTY_OTHERS)
#define TTYPERMS
#endif

/* Weird alignment stuff borrowed from Marcus.  Is it needed? */

#if defined(__GNUC__) && defined(WEIRD)
#define WORD_ALIGN      __attribute__ ((packed))
#else
#ifdef _IBMR2
#pragma options align=twobyte
#endif
#define WORD_ALIGN      /**/
#endif

struct wrthdr {
	long hdr_size;			/* sizeof(struct wrthdr) */
	long tmp_size;			/* sizeof(struct wrttmp) */
	};

struct wrttmp {
	char wrt_line[UT_LINESIZE];	/* a tty line */
	char wrt_what[UT_NAMESIZE];	/* what this user is doing? */
	char wrt_last[UT_NAMESIZE];	/* Who did he last write to? */
#ifndef TTYPERMS
	char wrt_mesg;			/* user's write perms (y or n) */
#endif /*TTYPERMS*/
	char wrt_record;		/* Should we record tels? y, n, or a */
	char wrt_telpref;		/* user's program prefs (TELPREF_*) */
	char wrt_modepref;		/* user's mode prefs (c, l or -) */
	char wrt_help;			/* user is a helper?  y or n */
	int  wrt_pid WORD_ALIGN;	/* our process id */
	time_t wrt_time WORD_ALIGN;	/* should match ut_time in utmp file */
	char wrt_bells;                 /* ring bells? y or n */
	char wrt_except;                /* use exception file? y or n */
	};

/* wrt_telperf bit masks:  If TELPREF_OLD is set, then wrt_telperf has one of
 * the following values:
 *     '-'   any program - equivalent to new value 0
 *     'w'   write program only - equivalent to new value TELPREF_WRITE
 *     't'   tel program only - equivalent to new value TELPREF_TEL
 * If TELPREF_OLD is not set, then any of the programs whose bits are set are
 # acceptable.
 */
#define TELPREF_OLD	0x20	/* Use old-style flag value */
#define TELPREF_WRITE	0x01	/* Accept writes */
#define TELPREF_TEL	0x02	/* Accept tels */
#ifdef WITH_TALK
#define TELPREF_TALK	0x04	/* Accept talks */
#define TELPREF_ALL	(TELPREF_WRITE | TELPREF_TEL | TELPREF_TALK)
#else
#define TELPREF_ALL	(TELPREF_WRITE | TELPREF_TEL)
#endif

#ifdef _IBMR2
#pragma options align=reset
#endif

/* These are the tty modes to use when permissions are on and off */

#define PERMS_OFF 0600		/* To depermit everything */
#define PERMS_OPEN 0622		/* To allow cats to your device */
#ifdef TTY_OTHERS
#define PERMS_ON  PERMS_OPEN	/* To allow writes and telegrams and what not */
#else
#define PERMS_ON  0620		/* To allow writes and telegrams and what not */
#endif /*TTY_OTHERS*/

/* Initial (at login) settings of various flags in wrttmp */
#define DFLT_MESG 'n'		/* this effects only TTYPERMS versions */
#define DFLT_HELP 'n'
#define DFLT_RECO 'n'
#define DFLT_MODE 'a'
#define DFLT_PREF TELPREF_ALL
#define DFLT_BELL 'y'
#define DFLT_EXCP 'n'

/* Offset of the nth entry in the wrttmp file */
#define wrttmp_offset(n) (wt_head.hdr_size + (n)*wt_head.tmp_size)


/* WRTHIST file
 * 
 * The wrthist file keeps track of when each user last wrote each other user.
 * It is used to allow telegrams to be sent to people who recently sent you
 * telegrams, even if their permisions are off, and enforce rules about
 * flooding -- repeatedly sending many telegrams in a short period of time.
 *
 * It functions as a two dimensional array indexed by my slot number and
 * his slot number.  Slot numbers are the index our wrttmp entry have in the
 * wrttmp file.  It's like an array "struct wrthist table[][MAXSLOTS], where
 * the first index is myslot number and the second is his slot number.
 */

#ifndef MAXSLOTS
#define MAXSLOTS 512
#endif

/* At most MAX_TEL_N telegrams may be sent in any MAX_TEL_T second period */
#ifndef MAX_TEL_T
#define MAX_TEL_T 25
#endif
#ifndef MAX_TEL_N
#define MAX_TEL_N 4
#endif

struct wrthist {
	time_t tm;			   /* last time I wrote him */
#if MAX_TEL_N > 1
	unsigned short intv[MAX_TEL_N-1];  /* intervals to pervious times */
#endif
	};


/* Location of slot describing last time a wrote b -- b should be < MAXSLOTS */

#define whindex(a,b) (long)((a * MAXSLOTS + b) * sizeof(struct wrthist))

#endif /* WRTTMP_H */
