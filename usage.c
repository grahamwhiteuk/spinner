/*
usage.c for Spinner
vers 1.2.2
$Revision: 1.3 $

    Copyright (C) 2002, 2003 Joe Laffey <joe@-REMOVE-THIS-laffeycomputer.com>

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


#include <sysexits.h>
#include <stdio.h>
#include <stdlib.h>

#include "usage.h"
#include "spinner.h"


/***************************/
/* Print usage info        */
void usage(void)
{
	
	fprintf(stderr, "------------------------------------------------\n");
	fprintf(stderr, PACKAGE " " VERSION  " by Joe Laffey, LAFFEY Computer Imaging.\n");
	fprintf(stderr, "Visit http://www.laffeycomputer.com/ for updates.\n");
	fprintf(stderr, "This software comes with ABSOLUTELY NO WARRANTY. " PACKAGE " -L for license.\n");   
    fprintf(stderr, "Usage:  " PACKAGE "[-IntTuvl<path>[f<path>|F][p<prio>|P]] [delay]\n");
    fprintf(stderr, "        -f   <path> Set pid file path (default is ~/" DEFAULT_PID_FILENAME ")\n");
	fprintf(stderr, "        -F   Do *not* create a pid file\n");
	fprintf(stderr, "        -I   Do *not* use inverse video for spinner\n");
	fprintf(stderr, "        -l   <path> Set log file path (for debugging). Off by default.\n");
	fprintf(stderr, "        -L   Display the license\n");
	fprintf(stderr, "        -n   Send only null characters. (No visible output.)\n");
	fprintf(stderr, "        -p   <priority> Specify process priority to use (default is %d)\n", DEFAULT_PRIORITY);
	fprintf(stderr, "        -P   Do *not* change process priority (default is to make nice)\n");
	fprintf(stderr, "        -r   Reset term on quit (Use if you get left in inverse a lot.)\n");
	fprintf(stderr, "        -R   Reset the term and Quit immediately. (nothing else)\n");
	fprintf(stderr, "        -t   <tty path> Specify path of TTY to which to write\n");
	fprintf(stderr, "        -T   Ignore incompatible TERM environment var setting\n");
	fprintf(stderr, "        -u   Delay is in microseconds instead of seconds\n");	
	fprintf(stderr, "        -v   Verbose mode (lots of output)\n");

	fprintf(stderr, "Returns: 0 on success, non-zero on failure.\n");
	fprintf(stderr, "Launches into the background on success.\n");
	fprintf(stderr, "Use: kill `cat <pidfile>` to stop.\n");
	

	exit(EX_USAGE);
}



/*****************************/
/*  Print out the license    */
void License(void)
{
	fprintf(stderr, "------------------------------------------------\n");
	fprintf(stderr, PACKAGE " " VERSION " by Joe Laffey, LAFFEY Computer Imaging.\n");
	fprintf(stderr, "Visit http:// www.laffeycomputer.com/ for updates.\n");
	fprintf(stderr, "This software is copyright 2002, 2003 by Joe Laffey.\n\n");   
    
	fprintf(stderr, "%s comes with ABSOLUTELY NO WARRANTY; for details see the COPYING file\nthat accompained this distribution. ", PACKAGE);
	fprintf(stderr, "This is free software, and you are welcome\nto redistribute it");
	fprintf(stderr, " under the terms of GNU PUBLIC LICENSE.\n");


	exit(EX_USAGE);
}

