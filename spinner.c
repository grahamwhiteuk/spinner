/*
spinner.c
vers 1.2.3
$Revision: 1.9 $

    Copyright (C) 2002, 2003 Joe Laffey <software@-REMOVE-THIS-laffeycomputer.com>

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


/* This program makes a little character spinner. */
/* e.g. - \ | / - \ | / ...                       */
/* It is useful for keeping links alive, etc.     */

/* http://www.laffeycomputer.com/software.html    */

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/uio.h>
#include <fcntl.h>
#include <limits.h>
#include <errno.h>
#include <sysexits.h>
#include <string.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <pwd.h>
#include <termios.h>
#include <setjmp.h>

#include "spinner.h"
#include "usage.h"


/* These are the chars used to make the spinner */
/* You may edit these, changing the count if desired */
#define SPINNER_CHARS		{ '-', '\\', '|','/','-','\\','|','/' }

/* Maximum value for the time argument passed 			*/
/* This is pretty insignificant.						*/
#define MAX_TIME			65000



/****************/
/* Static vars  */


/* NUmber of times our interrupt signal handler is called */
static int 						gInterrupted = 0;

/* jump buffer for return from our read timeout signal handler */
static sigjmp_buf				gAlarmJump;

/* Flag to decide if we can call siglongjmp yet */
static volatile sig_atomic_t	gCanJump = 0;






/******************************/
/*  Handle interrupt Signals  */
static void DoInterrupt(void)
{
	int	saveErrno;
	
	gInterrupted++;
	/* for a quit if we get two signals in a row before we have quit normally */
	if(gInterrupted > 1)
	{

	
		_exit(EX_TEMPFAIL);
	}

}


/***************************************************************/
/*  Signal Alarm handler to interrupt blocked open of a TTY    */
static void InterruptBlockedOpen(void)
{
	if(gCanJump == 0	)
		return;	/* Not ready for signal yet */
	
	/* Make sure we don't do this again */
	gCanJump = 0;
	/* just jump back to our setjump */
	siglongjmp( gAlarmJump, 1);

}



/****************************/
/*  Become a daemon proc    */
static int Daemonize(int verbose, char* logFileName, int logFileSet, FILE* logfile, int doPidFile, char* pidFileName)
{
	pid_t pid;
	
	if( (pid = fork()) < 0 )
		return (-1);
	else if( pid != 0 )
	{
		if(doPidFile)
		{
			
			WritePidFile(verbose, pid, logfile, pidFileName);
			
		}
		
		/* poorly assume %d is good for a pid */
		if(verbose)
		{
			if(logFileSet)
			{
				fprintf(logfile, "%s: Parent process terminating.\n", PACKAGE);	
				fprintf(logfile, "%s: Child process ( pid %d ) continuing...\n", PACKAGE, pid);	
				fprintf(logfile, "%s: Further output to %s\n", PACKAGE, logFileName);	
			}
		}
		exit(0); /*parent exits stage left */
	}	
	
	/* now child */
	setsid();	/* become sess leader */
	
	chdir("/");	/* chdir to root to allow unmounting of filesystems, etc. */
	
	umask(0);	
	
	
	/* Close all of these up */
	/* We must close all of them, including stderr to allow logout on linux. 		*/
	/* Any descriptors open on the active TTY prevent a logout on linux, BSD is OK. */
	close(STDIN_FILENO);
	close(STDOUT_FILENO);
	close(STDERR_FILENO);
	
	return(0);

}



/****************************/
/*  Create our pid file     */
static void WritePidFile(int verbose, pid_t pid, FILE* logfile, char* pidFileName )
{
	FILE*		pidFile;
	int			pidFileDes;
	
		
	if( 0 > (pidFileDes = open(pidFileName, O_CREAT | O_WRONLY | O_TRUNC, 0600)) )
	{
		fprintf(logfile, "%s: WARNING: Unable to open pid file %s for writing.\n%s: %s\n%s: WARNING: No pid file will be written.\n", PACKAGE, pidFileName, PACKAGE, strerror(errno), PACKAGE);	
		return;
	}
	
	
	pidFile = fdopen(pidFileDes, "w");
	if(pidFile == NULL)
	{
		fprintf(logfile, "%s: WARNING: Unable to fdopen pid file %s for writing.\n%s: %s\n", PACKAGE, pidFileName, PACKAGE, strerror(errno));	
		exit(1);
	}
	else
	{
		if(verbose)
			fprintf(logfile, "%s: Writing daemon's pid to pid file %s.\n", PACKAGE, pidFileName);	
			
		/* poorly assume %d is good for a pid */
		fprintf(pidFile, "%d\n", pid );
		
		fclose(pidFile);
		
	}
}



/*********************/
/*  Open the TTY     */
static inline int OpenTTY(int logFileSet, FILE* logfile, char* whichTTY, char* ttyName )
{
	int	ttyFileDes;
	
	/* This sometimes blocks on tty's not currently in use, depending on OS, but I am hesitant to try to open a tty nonblock and proceed as usual */
	/* So we use an alaram */
	if( SIG_ERR == signal(SIGALRM, (void *)InterruptBlockedOpen) )
	{
		if(logFileSet)
			fprintf(logfile, "%s: Unable to install SIGALRM signal handler to interrupt blocks to open()!\n%s: %s\n", PACKAGE, PACKAGE, strerror(errno));
		exit(1);
	
	}
		
	
	/* Use sigsetjmp and siglongjmp to interrupt blocking I/O.			 					*/
	/* This method is used to prevent a race condition betweenn the initial call to alarm() */
	/* and the blocking system call. It also handles restarted system calls. See Stevens.	*/
	
	/* Here we set the "return" address for our future call to siglongjump */
	if( 0 != sigsetjmp(gAlarmJump, 1) )
	{
		/* if sigsetjump returns nonzero then we have arrived here because of a call to siglongjump() 	*/
		/* That call could only have come from our singnal handler InterruptBlockedOpen()				*/
		/* So we abort with a message about the timeout.												*/
		if(logFileSet)
			fprintf(logfile, "%s: The attempt to open the TTY %s\n%s: for writing timed out after %d seconds.\n%s: Perhaps that is not an active TTY...\n", PACKAGE, whichTTY, PACKAGE, TTY_OPEN_TIMEOUT, PACKAGE);
		exit(1);
	}
	
	/* OK to to use siglongjmp */
	gCanJump = 1;
	
	/* Set the alarm to interrupt a blocking open of the TTY */
	alarm( TTY_OPEN_TIMEOUT );
	
	/* Open the TTY */
	ttyFileDes = open( whichTTY, O_WRONLY );
	if(ttyFileDes < 0)
	{
		if(logFileSet)
			fprintf(logfile, "%s: Unable to open %s for writing!\n%s: %s\n", PACKAGE, whichTTY, PACKAGE, strerror(errno));
		exit(1);
	}
	
	/* turn off the interrupt alarm */
	alarm(0);

	/* Remove the alarm handler */
	if( SIG_ERR == signal(SIGALRM, SIG_DFL) )
	{
		if(logFileSet)
			fprintf(logfile, "%s: Unable to set SIGALRM signal handler to default!\n%s: %s\n", PACKAGE, PACKAGE, strerror(errno));
		exit(1);
	
	}
	
	
	/* Test to see if we opened a terminal 						*/
	if(!isatty(ttyFileDes))
	{
		if(logFileSet)
			fprintf(logfile, "%s: %s is not an active terminal!\n", PACKAGE, whichTTY);
		exit(1);
	}
	
	/* get the name of our term */
	strncpy(ttyName, ttyname(ttyFileDes), _POSIX_PATH_MAX);
	ttyName[_POSIX_PATH_MAX] = '\0';
	
	return ttyFileDes;
	
}



/***********************************/
/* Send Reset code to the term     */
static void ResetTerm( int verbose, int logFileSet, FILE* logfile, char* whichTTY, char* ttyName )
{
	int ttyDes; 
	
	
	if(verbose)
	{
		if(logFileSet)
			fprintf(logfile, "%s: Attempting to reset terminal...\n", PACKAGE);
		sleep(1);	
	}
	
	/* Open up the TTY and get its name */
	ttyDes = OpenTTY( logFileSet, logfile, whichTTY, ttyName );
		
	write(ttyDes, VT100_RESET_CODE, sizeof(VT100_RESET_CODE)); /* call reset term */
	
	if( 0 != close(ttyDes))
	{
		if(logFileSet)
			fprintf(logfile, "%s: Unable to close TTY %s!\n", PACKAGE, ttyName);
		_exit(1);
	}
	
	if(verbose)
	{
		if(logFileSet)
			fprintf(logfile, "%s: Terminal reset code written.\n", PACKAGE);
	}

	return;
}


/***************************************************/
/* retrieve the default filename. Use /etc/passwd  */
static int GetHomePath(int verbose, FILE* logfile, char* fullHomePath)
{

	/* the HOME string  */
	char* homePath;
	
	struct passwd	*pwdPtr;

			
	/* try to get home out of  passwd db*/
	if( NULL == (pwdPtr = getpwuid(getuid()) ) )
	{
		fprintf(logfile, "%s: WARNING: Unable to determine HOME directory.\n%s: WARNING: No pid file will be written.\n", PACKAGE, PACKAGE);
	}
	else
	{
		/* this ought never be null, but code safely */
		if( pwdPtr->pw_dir == NULL )
		{
			fprintf(logfile, "%s: Home dir entry (pw_dir) from passwd database was null. Aborting.\n", PACKAGE);
			return(1);
		}
		else
		{
			int len;
			
			homePath = malloc(MAX(strlen(pwdPtr->pw_dir)+1, _POSIX_PATH_MAX+1) );
			if(homePath == NULL)
			{
				fprintf(logfile, "%s: Unable to allocate memory for home directory string!\n", PACKAGE);
				return(1);
			}
		
			
			strncpy(homePath, pwdPtr->pw_dir, _POSIX_PATH_MAX);
			homePath[_POSIX_PATH_MAX] = '\0'; /* to be safe */
			
			if(verbose)
				fprintf(logfile, "%s: Retrieved HOME directory from passwd database.\n", PACKAGE);
	
			
			strncpy(fullHomePath, homePath , _POSIX_PATH_MAX);
		
		
			free(homePath);	
			
			
			len = strlen(fullHomePath);
			
			if( fullHomePath[len] != PATH_SEPARATOR )
			{
				if(len < _POSIX_PATH_MAX)
				{
					/* add a slash */
					fullHomePath[len] = PATH_SEPARATOR;
					fullHomePath[len + 1] = '\0'; /* Terminate ( see Arnold, et. al. ) */
				}
				else
				{
					fprintf(logfile, "%s: Length of retrieved home directory is too long!!\n", PACKAGE);
					return(1);
				}
			}
		
			
		}
		

	} /* end else of if( pwdPtr->pw_dir == NULL ) */

	/* fullHomePath is modified, as it is called by reference */
	return(0);	
}



/****************************************/
/* MainLoop for Printing the spinner    */
static int SpinnerLoop( int verbose, int inverse, int time, int microTime, int logFileSet, FILE* logfile, char* whichTTY, char* ttyName, char* failMsg )
{

	/* the file descriptor of the terminal to which we write */
	int ttyDes;
	
	/* The spinning characters */
	char chars[] = SPINNER_CHARS;

	/* length of above */
	int numChars = sizeof(chars);
	
	/* holds the pre-built strings of code to display */
	char cache[sizeof(chars)][OUTPUT_STR_MAX + 1];
	
	/* length of the cached strings. They are all the same */
	int strLen = 0; /* defaulf to quell warnings*/
	
	/* count.. */
	unsigned int i = 0; /* defaulf to quell warnings*/
	
	/* signal sets */
	sigset_t	sigFilledSet;
	sigset_t	sigSavedSet;
	
	int exitCode = 0;
	

	if(verbose)
	{
		if(logFileSet)
			fprintf(logfile, "%s: Generating display strings for spinner.\n", PACKAGE);
	}
	
	
	 /*We pre-cache all the character strings for the spinner */
	 for(i = 0; i < numChars; i++)
	 {	
		 /* These sprintfs create the strings that get sent to the term. */
		 /* They include vt100 escape codes. 							*/
	 
		 sprintf(
					cache[i], "%s%s%s%c%s", 
					VT100_STORE_POS_AND_SETTINGS_CODE,
					VT100_HOME_CODE,
					(inverse)?VT100_INVERSE_CODE:"", 
					chars[i], 
					VT100_RESTORE_POS_AND_SETTINGS_CODE
				);
		
	 }
	 
	/* length of the cacched strings.. They are all the same length */
	strLen = strlen(cache[0]); 
	
	if(verbose)
	{
		if(logFileSet)
			fprintf(logfile, "%s: Setting up signal handlers...\n", PACKAGE);
	}

	/*  Set up our signal handlers... */
	if( SIG_ERR == signal(SIGINT, (void *)DoInterrupt))
	{
		if(logFileSet)
			fprintf(logfile, "%s: Unable to install SIGINT signal handler!\n%s: %s\n", PACKAGE, PACKAGE, strerror(errno));
		exit(1);
	}
	
	if( SIG_ERR == signal(SIGTERM, (void *)DoInterrupt))
	{
		if(logFileSet)
			fprintf(logfile, "%s: Unable to install SIGTERM signal handler!\n%s: %s\n", PACKAGE, PACKAGE, strerror(errno));
		exit(1);
	}
	
	/* We really ought not be receiving this, since we go daemon... */
	if( SIG_ERR == signal(SIGHUP, (void *)DoInterrupt))
	{
		if(logFileSet)
			fprintf(logfile, "%s: Unable to install SIGHUP signal handler!\n%s: %s\n", PACKAGE, PACKAGE, strerror(errno));
		exit(1);
	}

	
	
	/* Set up our signal masks */
	if( 0 != sigfillset( &sigFilledSet ))
	{
		if(logFileSet)
			fprintf(logfile, "%s: Unable to fill in signal mask!\n", PACKAGE);
		_exit(1);
	}
	

	if(verbose)
	{
		if(logFileSet)
			fprintf(logfile, "%s: Commencing loop to display spinner...\n", PACKAGE);
	}
	
	
	/* main loop */
	while(!gInterrupted)
	{
		if( i < MAXINT )
			i++;
		else
			i = 0;
        	
        /* The TTY is opened and closed each time through the loop because sshd on linux */
        /* will not logout with the tty open. BSD works fine... */
        	
        /* Open up the TTY and get its name */
		ttyDes = OpenTTY( logFileSet, logfile, whichTTY, ttyName );
		
        /* Block all signals we can during write so we don't end up with the cursor	*/
		/* in a bad place, or mode (like inverse)									*/
		if( 0 > sigprocmask( SIG_BLOCK, &sigFilledSet, &sigSavedSet ))
		{
			if(logFileSet)
				fprintf(logfile, "%s: Unable to set process signal mask!\n", PACKAGE);
			_exit(1);
		}
			
	
		/* Be sure we still have an active term */
		if(!isatty(ttyDes))
		{
			if(logFileSet)
				fprintf(logfile, "%s: %s is no longer an active terminal!\n", PACKAGE, ttyName);
			
			/* Bail out */
			_exit(1);
		}
		
		
		
		/* Drain the terminal driver output queue */
		/* to be sure we can send our codes uninterrupted */	
		if( -1 == tcdrain(ttyDes))
		{
			if(logFileSet)
				fprintf(logfile, "%s: Unable to drain terminal output!\n", PACKAGE);
			_exit(1);
		}
		
		/* Print our line */
		if( -1 == write(ttyDes, cache[i % numChars], strLen) )
		{
			/* If this failed due to a temporarily unavailable code (EAGAIN) then ignore it.  					*/
			/* Mac OSX generates this a lot. It seems to be Terminal.app  as it works OK in XWindows on OS X 	*/
			if(errno == EAGAIN)
			{
				/* Ignore */
			}
			else
			{
				snprintf(failMsg, MAX_FAIL_MSG_LEN, "%s: write failed!\n%s: %s\n", PACKAGE, PACKAGE, strerror(errno));
				failMsg[MAX_FAIL_MSG_LEN] = '\0';
				gInterrupted = 1; /* we're gonna quit, but got to clenup */
				exitCode = 1;
			}
		}

		/* Drain the terminal driver output queue   */
		/* to be sure what we sent is uninterrupted */	
		if( -1 == tcdrain(ttyDes))
		{
			fprintf(logfile, "%s: Unable to drain terminal output!\n", PACKAGE);
			_exit(1);
		}
	

		/* restore the signals */
		if( 0 > sigprocmask( SIG_SETMASK, &sigSavedSet, NULL ))
		{
			if(logFileSet)
				fprintf(logfile, "%s: Unable to set process signal mask!\n", PACKAGE);
			_exit(1);
		}
		
		if( 0 != close(ttyDes))
		{
			if(logFileSet)
				fprintf(logfile, "%s: Unable to close TTY %s!\n", PACKAGE, ttyName);
			_exit(1);
		}
		
		if(microTime)
			usleep(time);
		else
			sleep(time);
	
	} /* end main loop */
	
	

	return exitCode;
}



/****************************************/
/* MainLoop for Printing the nulls     */
static int NullLoop( int verbose, int time, int microTime, int logFileSet, FILE* logfile, char* whichTTY, char* ttyName, char* failMsg )
{
	char	theNull[] = {'\0'};
	int 	exitCode = 0;
	int 	ttyDes;


	
	if(verbose)
	{
		if(logFileSet)
			fprintf(logfile, "%s: Setting up signal handlers...\n", PACKAGE);
	}

	/*  Set up our signal handlers... */
	if( SIG_ERR == signal(SIGINT, (void *)DoInterrupt))
	{
		if(logFileSet)
			fprintf(logfile, "%s: Unable to install SIGINT signal handler!\n%s: %s\n", PACKAGE, PACKAGE, strerror(errno));
		exit(1);
	}
	
	if( SIG_ERR == signal(SIGTERM, (void *)DoInterrupt))
	{
		if(logFileSet)
			fprintf(logfile, "%s: Unable to install SIGTERM signal handler!\n%s: %s\n", PACKAGE, PACKAGE, strerror(errno));
		exit(1);
	}
	
	/* We really ought not be receiving this, since we go daemon... */
	if( SIG_ERR == signal(SIGHUP, (void *)DoInterrupt))
	{
		if(logFileSet)
			fprintf(logfile, "%s: Unable to install SIGHUP signal handler!\n%s: %s\n", PACKAGE, PACKAGE, strerror(errno));
		exit(1);
	}

	if(verbose)
	{
		if(logFileSet)
			fprintf(logfile, "%s: Commencing loop to send nulls...\n", PACKAGE);
	}
		
	while(!gInterrupted)
	{
	
		/* The TTY is opened and closed each time through the loop because sshd on linux */
        /* will not logout with the tty open. BSD works fine... */
        
		/* Open up the TTY and get its name */
		ttyDes = OpenTTY( logFileSet, logfile, whichTTY, ttyName );
	
		/* Be sure we still have an active term */
		if(!isatty(ttyDes))
		{
			if(logFileSet)
				fprintf(logfile, "%s: %s is no longer an active terminal!\n", PACKAGE, ttyName);
			
			/* Bail out */
			_exit(1);
		}
	
		/* Drain the terminal driver output queue   */
		/* to be sure what we sent is uninterrupted */	
		/* This is probably not needed in null mode, but it just seems like a good idea.. */
		if( -1 == tcdrain(ttyDes))
		{
			if(logFileSet)
				fprintf(logfile, "%s: Unable to drain terminal output!\n", PACKAGE);
			
			_exit(1);
		}
	
		if( -1 == write(ttyDes, theNull, sizeof(theNull)) )
		{
			/* If this failed due to a temporarily unavailable code (EAGAIN) then ignore it.  					*/
			/* Mac OSX generates this a lot. It seems to be Terminal.app  as it works OK in XWindows on OS X 	*/
			if(errno == EAGAIN)
			{
				/* Ignore */
			}
			else
			{
				snprintf(failMsg, MAX_FAIL_MSG_LEN, "%s: write failed!\n%s: %s\n", PACKAGE, PACKAGE, strerror(errno));
				failMsg[MAX_FAIL_MSG_LEN] = '\0';
				gInterrupted = 1; /* we're gonna quit, but got to clenup */
				exitCode = 1;
			}
		}
		
		/* Drain the terminal driver output queue   */
		/* to be sure what we sent is uninterrupted */	
		/* This is probably not needed in null mode, but it just seems like a good idea.. */
		if( -1 == tcdrain(ttyDes))
		{
			if(logFileSet)
				fprintf(logfile, "%s: Unable to drain terminal output!\n", PACKAGE);
			_exit(1);
		}
		
		if( 0 != close(ttyDes))
		{
			if(logFileSet)
				fprintf(logfile, "%s: Unable to close TTY %s!\n", PACKAGE, ttyName);
			_exit(1);
		}
		
		if(microTime)
			usleep(time);
		else
			sleep(time);
	}
	
	return exitCode;

}


/****************************/
/* Main Entry               */
int main(int argc, char** argv)
{
	
	
	/* thechar array name of the terminal to which we write */
	char ttyName[_POSIX_PATH_MAX + 1]; 	

	
	/* Should we send only nulls? */
	int sendNulls = 0;
	

	/* time of sleep in secs or microseconds depending on switch*/
	int time;

	/* incoming name of tty to open from -t option */
	char whichTTY[_POSIX_PATH_MAX + 1];
	
	/* for getopt */
	int ch;
	
	/* should we use microtime for delay ? */
	int microTime = 0;
	
	/* Should we inverse the spinner ? */
	int inverse = 1;
	
	/* should we open another TTY other than DEFAULT_TTY ?*/
	int useTTY = 0;
	
	/* print extra garbage */
	int verbose = 0;
	
	/* Should we set the priority? defaults to a lower priority */
	int setPriority = 1;
	
	/* The default value to use for priority */
	int priority = DEFAULT_PRIORITY;
	
	/* Set the envrionment var to contain the child PID ? */
	int doPidFile = 1;
	
	/* hold the file path to the pid file */
	char pidFileName[_POSIX_PATH_MAX + 1] = "";
	
	/* hold the file path to the log file */
	char logFileName[_POSIX_PATH_MAX + 1] = "";
	
	/* hold the file path to the user's home dir */
	char fullHomePath[_POSIX_PATH_MAX + 1] = "";
	
	/* did the user specify a pid file ? */
	int pidFileSet = 0;
	
	/* did the user specify a log file ? */
	int logFileSet = 0;
	
	/* hold a failure message for later display after reset */
	char failMsg[MAX_FAIL_MSG_LEN +1] = "";
	
	/* the exit code to return */
	int exitCode = 0;
	
	/* Should we try to reset the term on quit ? */
	int resetOnQuit = 0;
	
	/* Should we try to reset the term ONLY and then quit (do nothing else)? */
	int resetOnly = 0;
	
	/* Should we check the TERM env var to be vt100 or vt102 ? */
	int checkTermType = 1;
	
	/* the TERM string from the env */
	char* termType;
	
	/* file to write errors to. This starts as stderr, and switches to a logfile on daemonizing */
	FILE*	logfile;
	




	
	while ((ch = getopt(argc, argv, "Iut:vp:Pf:l:FRTLn")) != -1)		/* get our options */
		switch(ch) {
		case 'I':
				 inverse = 0;
				 break;
		case 'u':
				/* delay should be in microseconds as opposed to default of seconds */
				microTime = 1;
				break;
		case 't':
				useTTY = 1;
				strncpy(whichTTY, optarg, _POSIX_PATH_MAX);
				whichTTY[_POSIX_PATH_MAX] = '\0'; /* be safe */
				break;
		case 'v':
				verbose = 1;
				break;
		case 'P':
				setPriority = 0;
				break;
		case 'n':
				sendNulls = 1;

				/* no need for this to send nulls */
				checkTermType = 0;
				break;
		case 'p':
				priority = atoi(optarg);;
				break;
		case 'F':
				doPidFile = 0;
				break;
		case 'f':
				/* pid file path */
				pidFileSet = 1;
				strncpy(pidFileName, optarg, _POSIX_PATH_MAX);
				pidFileName[_POSIX_PATH_MAX] = '\0'; /* SafteyPup sez, "terminate your strings" */
				break;
		case 'l':
				/* log file path */
				logFileSet = 1;
				strncpy(logFileName, optarg, _POSIX_PATH_MAX);
				logFileName[_POSIX_PATH_MAX] = '\0'; /*terminate  */
				break;
		case 'R':
				/* reset only*/
				resetOnly = 1;
				doPidFile = 0;
				break;		
		case 'r':
				resetOnQuit = 1;
				break;		
		case 'T':
				checkTermType = 0;
				break;	
				
		case 'L':
				License();
				break;	
				
		default:
				 usage();
	 }
	 argc -= optind;
	 argv += optind;

	
	if(argc > 2)
		usage();
	
	/* Default to writing errors to stderr */
	logfile = stderr;

	/* Stop any silly admin  that make this setuid... */
	if(getuid() != geteuid())
	{

		fprintf(logfile, "%s: **** This program is setuid! ****\n%s: **** Terminating due to security concerns! ****\n%s: If you *must* you could use setgid, but this is not recommened either.\n", PACKAGE, PACKAGE, PACKAGE);
		exit(1);
	}
	
	/* Warn admins that make this setgid. And refuse to operate on files. */
	if(getgid() != getegid())
	{
		/* here we poorly assume %d is right for a gid */
		fprintf(logfile, "%s: WARNING: *** This program is setgid to gid %d ! ****\n%s: WARNING: Pid file has been disabled for security reasons.\n", PACKAGE, getegid(), PACKAGE);
		
		/* disable for security against symlink attacks, etc. */
		doPidFile = 0;
	}
	
	
	
	/* Check term type */
	if(checkTermType)
	{
		
		if(NULL == (termType = getenv( "TERM" ) ))
		{
			fprintf(logfile, "%s: Unable to determine TERM type from the environment.\n%s: Use -T switch to override check and use VT100 codes anyway.\n", PACKAGE, PACKAGE);
			exit(1);
		}
		
	
		if( ! ( 
					(0 == strncasecmp(termType, "VT100", sizeof("VT100") )) 
				||  (0 == strncasecmp(termType, "VT102", sizeof("VT102") )) 
				||  (0 == strncasecmp(termType, "XTERM", sizeof("XTERM") )) 
				||  (0 == strncasecmp(termType, "SCREEN", sizeof("SCREEN") )) 
				||  (0 == strncasecmp(termType, "ANSI", sizeof("ANSI") )) 
			  ) 
		  )
		{
			fprintf(logfile, "%s: I cannot determine if your TERM type from the environment (%s)\n%s: is VT100 compatible.\n%s: Use -T switch to override this check and use VT100 codes anyway.\n%s: In most cases this is fine (unless you have a \"dumb\" terminal).\n", PACKAGE, termType, PACKAGE, PACKAGE, PACKAGE);
			exit(1);
		}
	}
	
	
	if(doPidFile && !pidFileSet)
	{
		/* retrieve the home directory for the cur user */
		if(0 != GetHomePath(verbose, logfile, fullHomePath))
		{
			fprintf(logfile, "%s: Unable to retrieve path to HOME directory from /etc/passwd!.\n", PACKAGE);
			exit(1);
		}	
			
		strncpy(pidFileName, fullHomePath, _POSIX_PATH_MAX );
		strncat(pidFileName, DEFAULT_PID_FILENAME, (_POSIX_PATH_MAX - strlen(pidFileName) - 1) );
		pidFileName[_POSIX_PATH_MAX] = '\0'; /* terminate this bad boy */
	
	}
	
	/* Print some info out for the user if they want it */
	if(verbose)
	{
		fprintf(logfile, "%s: Inverse mode %s.\n", PACKAGE, (inverse? "enabled":"disabled"));
		if(useTTY)
			fprintf(logfile, "%s: Using user-selected TTY: %s.\n", PACKAGE, whichTTY);
		
		if(resetOnQuit)
			fprintf(logfile, "%s: Will attempt to reset the terminal on quit.\n", PACKAGE);
		else
			fprintf(logfile, "%s: Will not attempt to reset the terminal on quit.\n", PACKAGE);
			
		if(setPriority)
			fprintf(logfile, "%s: Will attempt to set process priority to %d.\n", PACKAGE, priority);
		else
			fprintf(logfile, "%s: Will not alter process priority.\n", PACKAGE);	
	}
	
	if(argc <  1)
	{
		/* We did not get a delay specified */
		time = DEFAULT_DELAY;
		
		/* Don't use microTime with the default, as this could be a delay like 2 usec (insane) */
		microTime = 0;
		if(verbose)
			fprintf(logfile, "%s: No delay specified. Using default delay.\n", PACKAGE);
	
	}	
	else
	{
		/* nab the delay from arg */
		time = atoi(argv[0]);
		if(time < 0 || time > MAX_TIME)
		{
			fprintf(logfile, "%s: Delay values must be between 0 and %d inclusive\n", PACKAGE, MAX_TIME);
			exit(1);
		}
	}

	if(verbose)
		fprintf(logfile, "%s: Using %d %ssec delay.\n", PACKAGE, time, (microTime?"u":""));
	
	

	
	
	/* If some admin made this setgid we now drop privs */
	/* The only reason anyone would do this is to open the tty as group tty */
	if(getgid() != getegid())
	{
		if( 0 != setgid( getgid() ) )
		{
			fprintf(logfile, "%s: Unable to drop group priviledges!\n%s,: Aborting...", PACKAGE, PACKAGE);
			exit(1);
		
		}
	}
	
	
	
	if(!useTTY)
	{
		/* use our input tty. We cannot use /dev/tty here because when we 	*/
		/* become a daemon below it is no longer valid!!					*/
		strncpy(whichTTY, ttyname(STDIN_FILENO), _POSIX_PATH_MAX);
		whichTTY[_POSIX_PATH_MAX] = '\0';
	}
	

	if( resetOnly)
	{	
		/* All we do is reset the term */
		ResetTerm(verbose, 1, logfile, whichTTY, ttyName);
		exit(0);
	}	


	/* Set up our priority if required */
	if(setPriority)
	{
		if(verbose)
			fprintf(logfile, "%s: Setting process priority to %d.\n", PACKAGE, priority);
	
		if( -1 == setpriority( PRIO_PROCESS, 0 /* our process */, priority ) )
		{
			fprintf(logfile, "%s: WARNING: Unable to set process priority to %d.\n%s: WARNING: %s\n", PACKAGE, priority, PACKAGE, strerror(errno));
		}
	}
	
	
	/* become a daemon - muahahahaha! */
	if(verbose)
		fprintf(logfile, "%s: Launching into the background.  (Daemonizing...)\n", PACKAGE);
		
		
	/* Open up the logfile  - BEFORE we become a daemon so we still have someplace to send an error msg */
	if(logFileSet)
	{
		logfile = fopen(logFileName, "a");
		if( logfile == NULL )
		{
			fprintf(stderr, "%s: Unable to open logfile for appending! Location: %s\n", PACKAGE, logFileName);
			exit(1);
		}
		
		/* linebuffer the logfile  - Stream is still function if this fails, so no error check...*/
		setvbuf( logfile, NULL, _IOLBF, 0);
	}
	
	
	
	/* Note we pass stderr as the logfile name because we still want output to stderr at this point */
	if( Daemonize( verbose, logFileName, logFileSet, stderr, doPidFile, pidFileName ) != 0)
	{
		fprintf(stderr, "%s: Unable to become a daemon!\n", PACKAGE);
		exit(1);
	}

	if(verbose)
	{
		if(logFileSet)
			fprintf(logfile, "\n%s: Begin output to logfile.\n", PACKAGE);
	}
	/* Enter our main loop depending on whether we are sending */
	/* the spinner characters or nulls.                        */
	if(sendNulls)
	{
		exitCode = NullLoop(verbose, time, microTime, logFileSet, logfile, whichTTY, ttyName, failMsg);
	}
	else
	{
		exitCode = SpinnerLoop(verbose, inverse, time, microTime, logFileSet, logfile, whichTTY, ttyName, failMsg);
	}
	
	
	/* clean up pid file */
	if(doPidFile)
	{
		if( 0 != (unlink( pidFileName )) )
		{
		
			if(logFileSet)	
				fprintf(logfile, "%s: WARNING: Unable to delete pid file %s.\n%s: WARNING: %s\n", PACKAGE, pidFileName, PACKAGE, strerror(errno));
		}
	}
	
	/* reset term if possible */
	/* This is an attempt to keep the terminal in order when spinner stops. 		*/
	/* Nevertheless, on some systems, we end up with the cursor in the wrong 		*/
	/* place, or in thw wrong mode, when we are stopped mid-write.					*/
	
	/* FIXED: I was not blocking the signals correctly.								*/
	/* This should almost never be needed now, but it here for historical reasons. 	*/
	if( resetOnQuit)
	{	
		ResetTerm(verbose, logFileSet, logfile, whichTTY, ttyName);
	}	
	
	
	/* print fail message if needed - AFTER we reset the term...*/
	if(logFileSet)
	{
		fprintf(logfile, "%s", failMsg );
		
		if(verbose)
			fprintf(logfile, "%s: Exiting normally..\n", PACKAGE);

	
		fclose(logfile);
	}
	return(exitCode);
}

