/*
spinner.h
vers 1.2.1
$Revision: 1.3 $

    Copyright (C) 2002 Joe Laffey <joe@-REMOVE-THIS-laffeycomputer.com>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/





#ifndef SPINNER_H
#define SPINNER_H


#ifndef VERSION
#define VERSION "1.2"
#endif

#ifndef PACKAGE
#define PACKAGE "spinner"
#endif


#include <sys/types.h>



/* deafult delay in seconds */
#define	DEFAULT_DELAY		2

/*default process priority */
#define DEFAULT_PRIORITY	10

/* the name of the file to which to write our pid */
#define	DEFAULT_PID_FILENAME	".spinner.pid"

/* the name of the file to which to write our log msgs*/
#define	DEFAULT_LOG_FILENAME	".spinner.log"

/* timeout for opening a tty in seconds */
#define TTY_OPEN_TIMEOUT	8

/* path separator character - ought to determine dynamically */
#define PATH_SEPARATOR		'/'

/* max length of the fail message */
#define MAX_FAIL_MSG_LEN	256

/* maximum length of the TERM env var - bounds are checked */
#define MAX_TERM_TYPE_LEN	64

#ifndef _POSIX_PATH_MAX
/* Not good */
#define	_POSIX_PATH_MAX	256
#endif

#define MAXINT				(~((unsigned int)1 << ((8 * sizeof(int)) - 1)))
#define ESCAPE	 			27

#define MAX( a , b )		((a > b)?a:b)


/* max len of output string.. change if strings are changed below ( for cache )*/
#define	OUTPUT_STR_MAX	16

#define VT100_RESET_CODE						"\033c"
#define VT100_RESTORE_POS_AND_SETTINGS_CODE		"\0338"
#define VT100_STORE_POS_AND_SETTINGS_CODE		"\0337"
#define VT100_HOME_CODE							"\033[H"
#define VT100_INVERSE_CODE						"\033[7m"

static void DoInterrupt(void);
static int Daemonize(int verbose, char* logFileName, int logFileSet, FILE* logfile, int doPidFile, char* pidFileName);
static void WritePidFile(int verbose, pid_t pid, FILE* logfile, char* pidFileName );
static void InterruptBlockedOpen(void);
static inline int OpenTTY(int logFileSet, FILE* logfile, char* whichTTY, char* ttyName );
static void ResetTerm( int verbose, int logFileSet, FILE* logfile, char* whichTTY, char* ttyName );
static int SpinnerLoop( int verbose, int inverse, int time, int microTime, int logFileSet, FILE* logfile, char* whichTTY, char* ttyName, char* failMsg );
static int NullLoop( int verbose, int time, int microTime, int logFileSet, FILE* logfile, char* whichTTY, char* ttyName, char* failMsg );
static int GetHomePath(int verbose, FILE* logfile, char* fullHomePath);

#endif /* SPINNER_H */
