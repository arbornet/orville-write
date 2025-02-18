/* Standard header for all files that make up the Orville 'write' program */

#ifndef WRITE_H
#define WRITE_H

#include "orville.h"

#define LBSIZE 1024	/* Size of the input line buffer */

/* ======================== GLOBAL VARIABLES =========================== */

extern bool char_by_char, ask_root, fixed_modes, in_cbreak, is_writing,
	    insys, telegram, no_switch, am_root, rec_only, postpone;
extern char tmp_mesg;
extern char *progname, *mytty, *version, *telmsg;
extern char myname[], hisname[], hisdevname[], histty[], linebuf[];
extern char myuidname[];
extern FILE *histerm;
extern int wstream;
extern long hispos,mypos;

/* =================== PROCEDURE DEFINITIONS ===-----================== */

/* wrt_him.c functions */
void find_us(void);
bool iswritable(void);
bool he_replied(void);

/* wrt_hist.c functions */
void open_hist(void);
bool may_answer_tel(void);
int check_flood(void);
void register_tel(void);

/* wrt_main.c functions */
void done(int code);

/* wrt_me.c functions */
void find_me(void);
void set_modes(void);
void reset_modes(void);
bool nested_write(void);
void init_lastmesg(void);
void show_lastmesg(void);

/* wrt_opt.c functions */
char *leafname(char *fullpath);
void default_opts(char *argname);
void user_opts(int argc, char **argv);

/* wrt_sig.c functions */
void siginit(void);
void sigoff(void);
RETSIGTYPE intr(int sig);
RETSIGTYPE susp(void);
void dosystem(char *cmd);

/* wrt_tty.c functions */
void cbreak(bool flag);
void flushinput(int dev);
int get_cols(void);

/* wrt_type.c functions */
void dowrite(void);
void dotelegram(bool nl);
void open_record(void);
int ishisexception(int yesfile, char *login);
int isuexception(char *user, int yesfile, char *login);

#endif /* WRITE_H */
