#ifndef _HPUX_SOURCE
#pragma ident "$Id: ctop.c 240 2012-05-22 13:39:38Z dba15 $"
#else
static char *sccsid = "$Id: ctop.c 240 2012-05-22 13:39:38Z dba15 $";
#endif

/*
 * processes
 *
 */

#include <sys/param.h>
#include <sys/pstat.h>
#include <sys/pstat/pstat_ops.h>
#include <sys/dk.h>

#include <unistd.h>
#include <stddef.h>
#include <string.h>
#include <stdlib.h>

#include <sys/utsname.h>
#include <pwd.h>
#include <math.h>
#include <limits.h>
#include <libgen.h>

#ifndef MAIN
#define _XOPEN_SOURCE_EXTENDED
#include <curses.h>
#else
#include <stdio.h>
#define USAGE "[-n count]"
int	errflg = 0, nflg = 0, xflg = 0, count = 0;
#endif
int extended = 0;	/* available externally */

#include <time.h>

static struct pst_status *pst = NULL;
static struct passwd *pw;
static struct pst_static pss;

#define K	1024
#define M	1048576
#define G	1073741824

#define MAX_LENGTH 1024
static char	long_command[MAX_LENGTH];
static union pstun pu;

static int
#ifdef __STDC__
node_compare(const void *node1, const void *node2)
#else
node_compare(node1, node2)
 void *node1;
 void *node2;
#endif
{
/* a > b  : <0
 * a == b :  0
 * a < b  : >0
 */
#define compare(a,b)	(a>b ? -1 : (a==b ? 0 : 1))

	return(compare(
		((struct pst_status *)node1)->pst_pctcpu,
		((struct pst_status *)node2)->pst_pctcpu));
}

static char *
canonicalize(n) 
 register _T_LONG_T n;
{
	static char s[10];
	char	*ind = "G";
	int		div = G;

	if ((n * pss.page_size / G) > 10000) {
		ind = "G"; div = G;
		goto out;
	}
	if ((n * pss.page_size / M) > 100000) {
		ind = "G"; div = G;
		goto out;
	}
	if ((n * pss.page_size / M) > 100) {
		ind = "M"; div = M;
		goto out;
	}
	if ((n * pss.page_size / K) > 100000) {
		ind = "M"; div = M;
		goto out;
	}
	if ((n * pss.page_size / K) > 1) {
		ind = "K"; div = K;
		goto out;
	}
out:
	snprintf(s, 10, "%5.0f%s", ((float)n * pss.page_size / div), ind);

	return s;
}

static char *
canonicalize2(n) 
 register _T_LONG_T n;
{
	static char s[10];
	char	*ind = "G";
	int		div = G;


	if ((n / G) > 10000) {
		ind = "G"; div = G;
		goto out;
	}
	if ((n / M) > 100000) {
		ind = "G"; div = G;
		goto out;
	}
	if ((n / M) > 100) {
		ind = "M"; div = M;
		goto out;
	}
	if ((n / K) > 100000) {
		ind = "M"; div = M;
		goto out;
	}
	if ((n / K) > 1) {
		ind = "K"; div = K;
		goto out;
	}
out:
	snprintf(s, 10, "%5.0f%s", ((float)n / div), ind);

	return s;
}

static char *
status(n)
 register _T_LONG_T n;
{
	switch ((int)n) {
	case PS_SLEEP:
		return "SLEEP";
	case PS_RUN:
		return "RUN";
	case PS_STOP:
		return "STOP";
	case PS_ZOMBIE:
		return "ZOMBIE";
	case PS_IDLE:
		return "IDLE";
	case PS_OTHER:
		return "OTHER";
	}
}

int
#ifdef MAIN
#	ifdef __STDC__
		top(void)
#	else
		top()
#	endif
#else
#	ifdef __STDC__
		top(WINDOW *w)
#	else
		top(w)
		 WINDOW *w;
#	endif
#endif
{
	struct pst_dynamic psd;
	struct pst_status *p = NULL;
	int		n, i;
#ifndef MAIN
	int		Y, X, y, x;
#endif

	if (getenv("UNIX95"))
		extended = 1;

	if (extended)
		pu.pst_command = long_command;

	if (pstat_getdynamic(&psd, sizeof(psd), (size_t)1, 0) == -1) {
		perror("pstat_getdynamic");
		return(-1);
	}

	if (!pst) {
		if ((pst = malloc(sizeof(struct pst_status) * psd.psd_maxprocs)) == NULL) {
			perror("malloc");
			return(-1);
		}
		if (pstat_getstatic(&pss, sizeof(pss), (size_t)1, 0) == -1) {
			perror("pstat_static");
			return(-1);
		}
	}
	p = pst;


	if ((n = pstat_getproc(pst, sizeof(struct pst_status), psd.psd_activeprocs, 0)) != -1) {

		qsort((void *)pst, n, sizeof(struct pst_status), node_compare);

#ifdef MAIN
		printf("%-12s %5s %5s %9s %6s %s\n",
			"UID", "PID", "PPID", "TIME", "%CPU", "CMD");

		for (i = 0; i < n; i++, p++) {
			pw = getpwuid(p->pst_uid);

			if (count && i == count)
				break;

			if (extended)
				if (pstat(PSTAT_GETCOMMANDLINE, pu, MAX_LENGTH, 1, p->pst_pid) == -1) {
					perror("pstat");
					return(-1);
				}

			printf("%-12s %5lld %5lld %6ld:%02ld %6.2f %s\n",
				pw ? pw->pw_name : ltoa(p->pst_uid),
				p->pst_pid,
				p->pst_ppid,
				(int)floor((double)(p->pst_utime + p->pst_stime)/60),
				(int)floor((double)((p->pst_utime + p->pst_stime)%60)),
				p->pst_pctcpu * 100,
				extended ? pu.pst_command : p->pst_cmd);
		}
#else
		getmaxyx(w, Y, X);

		wattron(w, A_REVERSE);
		mvwhline(w, 1, 1, ' '|A_REVERSE, X-2);

		mvwprintw(w, 1, 1, "%3s %5s %5s %3s %2s %-8s %6s %6s %6s %7s %6s %s %d",
			"CPU", "PID", "PPID", "Pri", "NI", "User", "Size", "RSS", "State", "Time", "%CPU", "Cmd", n);
		wattroff(w, A_REVERSE);

		for (y = 2, x = 1; y < Y-1 ; y++, p++) {
			char	*s;
			int		rem;
			pw = getpwuid(p->pst_uid);
			//                                                Usr Sz  RSS Sta Tim      %CPU	 CMD
			// Total width at this point is 70
			mvwprintw(w, y, x, "%3lld %5lld %5lld %3lld %2lld %-8s %6s %6s %6s %4ld:%02ld %6.2f ",
				p->pst_procnum,
				p->pst_pid,
				p->pst_ppid,
				p->pst_pri,
				p->pst_nice,
				pw ? pw->pw_name : ltoa(p->pst_uid),
				canonicalize2(
					(p->pst_vtsize*p->pst_text_size) +
					(p->pst_vdsize*p->pst_data_size) +
					p->pst_vssize*pss.page_size  +
					p->pst_vshmsize*pss.page_size +
					p->pst_vmmsize*pss.page_size +
					p->pst_vusize*pss.page_size +
					p->pst_viosize*pss.page_size
				),
				canonicalize(p->pst_rssize),
				status(p->pst_stat),
#ifdef CLICKS
				(int)floor((double)p->pst_cptickstotal/CLK_TCK/60),
				(int)floor((double)p->pst_cptickstotal/CLK_TCK)%60,
#else
				(int)floor((double)(p->pst_utime + p->pst_stime)/60),
				(int)floor((double)((p->pst_utime + p->pst_stime)%60)),
#endif
				p->pst_pctcpu * 100);

			if (extended)
				if (pstat(PSTAT_GETCOMMANDLINE, pu, MAX_LENGTH, 1, p->pst_pid) == -1) {
					perror("pstat");
					return(-1);
				}

			if ((s = strtok(extended ? pu.pst_command : p->pst_cmd, " \t")) == NULL)
				waddnstr(w, p->pst_ucomm, X-70);
			else {
				waddnstr(w, basename(s), X-70);

				// print args to command

				if ((rem = X - 70 - strlen(basename(s)) - 1) > 0) {
					while (s = strtok(NULL, " \t")) {
						waddnstr(w, " ", 1);
						waddnstr(w, s, rem);

						if ((rem -= strlen(s) + 1) <= 0)
							break;
					}
				}
			}
		}
#endif
	} else 
		perror("pstat_getproc");

	return 0;
}

#ifdef MAIN


static char *prog;

int
#ifdef __STDC__
main(int argc, char *argv[])
#else
main(argc, argv)
 int	argc;
 char	*argv[];
#endif
{
	extern	char *optarg;
	extern	int optind, opterr;
	int		c;


	prog = basename(argv[0]);

	opterr = 0;

	while ((c = getopt(argc, argv, "n:x")) != -1)
		switch (c) {
		case 'n':		/* How many lines to show */
			nflg++;
			count = atoi(optarg);
			break;
		case 'x':		/* Extended output */
			xflg++;
			extended++;
			break;
		case '?':
			errflg++;
			break;
		}


	if (errflg) {
		fprintf(stderr, "%s: %s\n", prog, USAGE);
		exit(2);
	}
	top();

	exit(0);
}
#endif
