#ifndef _HPUX_SOURCE
#pragma ident "$Id: main.c 240 2012-05-22 13:39:38Z dba15 $"
#else
static char *sccsid = "$Id: main.c 240 2012-05-22 13:39:38Z dba15 $";
#endif
#include <unistd.h>
#include <stdlib.h>
#include <libgen.h>
#include <stdio.h>

extern void cpu();

extern int extended;

char *prog;

int	errflg = 0, iflg  = 0, hflg = 0, sflg = 0, xflg = 0;

int interval = 1;

#define USAGE "[-h] [-x] [-i interval]"

int
#ifdef __STDC__
main(int argc, char *argv[])
#else
main(argc, argv)
 int	argc;
 char 	*argv[];
#endif
{
	extern	char *optarg;
	extern	int optind, opterr;
	int		c;
	char	buf[BUFSIZ];


	prog = basename(argv[0]);

	opterr = 0;

	while ((c = getopt(argc, argv, "i:hsx")) != -1)
		switch (c) {
		case 'i':		/* interval */
			if (iflg)
				errflg++;
			interval = atoi(optarg);
			iflg++;
			break;
		case 'h':		/* hide diabled/failed CPUs */
			hflg++;
			break;
		case 's':		/* split screen */
			sflg++;
			break;
		case 'x':		/* extended info */
			xflg++;
			extended = 1;
			break;
		case '?':
			errflg++;
			break;
		}


	if (errflg) {
		fprintf(stderr, "%s: %s\n", prog, USAGE);
		exit(2);
	}

	cpu();
}
