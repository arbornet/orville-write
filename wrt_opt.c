/* WRITE OPTION ROUTINES -- Code to set options from orville.conf file or
 * command line.
 */

#include "write.h"

/* SET_OPTS:  Given a pointer to a string of one character option flags,
 * terminated by a null or by white space, this sets the global variables
 * corresponding to those flags.  It returns 1 if any of the characters were
 * illegal, 0 otherwise.
 */

int set_opts(char *str)
{
	for(;;str++)
		switch(*str)
		{
		case 'S':
			no_switch= TRUE;
			break;
		case 'c':
			char_by_char= TRUE;
			break;
		case 'l':
			char_by_char= FALSE;
			break;
		case 'n':
		case 'y':
		case 's':
			tmp_mesg= *str;
			break;
		case 'f':
			f_pipes= FALSE;
			break;
		case 't':
			telegram= TRUE;
			break;
		case 'r':
			ask_root= TRUE;
			break;
		case 'p':
			postpone= TRUE;
			break;
		case 'v':
			fprintf(stderr,"Orville Write version %s\n",version);
			break;
		case '\0':
		case '\n':
		case '\t':
		case ' ':
			return(0);
		default:
			return(1);
		}
}


/* SET_DFLT:  Called for each "options" line in the configuration file.  It
 * is passed the rest of the line after the "options" keyword and any spaces.
 * It checks if the filename matchs our program name, and if so, sets the
 * options on that line.
 */

void set_dflt(char *p)
{
int len;

    /* Does file line start with progname or a star? */
    if (*p == '*')
	p++;
    else
    {
	len= strlen(progname);
    	if (!strncmp(progname,p,len))
	    p= p+len;
	else
	    return;
    }

    /* Is that name followed by a space character? */
    if (*p != ' ' && *p != '\t')
	return;

    /* Scan the rest of the line for dashes, and set anything after it */
    for (p++ ;*p != '\n' && *p != '\0'; p++)
	if (*p == '-' && set_opts(p+1))
	    fprintf(stderr,"%s: Unknown flag on options line in "
		ORVILLE_CONF"\n", progname);
}

/* DEFAULT_OPTS:  This reads through the orville.conf file loading configuration
 * settings and looking for "options" lines that match progname.  (Either
 * progname itself or a "*").  If a match is found, the rest of the line is
 * scanned for strings starting with a "-" that we can interpret as option
 * flags.  Those flags are set.  Then we continue scanning for more matches.
 */

void default_opts(char *argname)
{
    /* Strip leading components off invocation name of program */
    progname= leafname(argname);

    /* Use common reading routine */
    readconfig(set_dflt);
}



/* USER_OPTS:  This parses the command line arguments.  It sets options, and
 * also gets the name or tty of the person to write, if given.  If there are
 * problems in the argument syntax, then it prints a usage message.  Note
 * that with the telegram option, all arguments after the user name (possibly
 * including the ttyname) are stuck into linebuf.  telmsg is pointed at the
 * part after the first word which might be the target tty rather than part
 * of the message.
 */

void user_opts(int argc, char **argv)
{
int i;
char *c;
bool gotname=FALSE;
bool gottty=FALSE;
int wastelegram= telegram;
char *telegram_end= linebuf;

    telmsg= NULL;

    for (i=1; i<argc; i++)
    {
	c= argv[i];
	if (!(gottty && telegram) && *c == '-')
	{
	    if (*(++c) == '\000')
	    {
		if (gotname)
		{
		    gottty= TRUE;
		    telmsg= linebuf;
		}
		else
		    gotname= TRUE;
	    }
	    else if (set_opts(c))
		goto usage;
	}
	else if (!gotname)
	{
	    strncpy(hisname, c, UT_NAMESIZE);
	    gotname= TRUE;
	}
	else if (!gottty || telegram)
	{
	    if (!gottty)
	    {
		if (telegram && strlen(c) > UT_LINESIZE)
		    telmsg= linebuf;
		else
		    strncpy(histty, c, UT_LINESIZE);
		gottty= TRUE;
	    }

	    if (telegram)
	    {
		if (telegram_end != linebuf)
		{
		    *(telegram_end++)= ' ';
		    if (telmsg == NULL)
			telmsg= telegram_end;
		}
		while (*c != '\0' & telegram_end < linebuf+LBSIZE-1)
		{
		    *(telegram_end++)= *c;
		    /* Erase message words so they don't show on "ps" */
		    /*  what needs to be done to do this varies on unixes */
		    /*  but this should work most places */
		    *(c++)= ' ';
		}
	    }
	}
	else
	    goto usage;
    }
    *(telegram_end++)= '\n';
    *telegram_end= '\0';
    return;

usage:
    if (wastelegram)
	    printf("usage: %s [-ynsr] user [ttyname] [message...]\n",
		progname);
    else
	    printf("usage: %s [-clynsptr] user [ttyname]\n",progname);
    done(1);
}
