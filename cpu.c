#ifndef _HPUX_SOURCE
#pragma ident "$Id: cpu.c 241 2012-05-22 14:53:03Z dba15 $"
#else
static char *sccsid = "$Id: cpu.c 241 2012-05-22 14:53:03Z dba15 $";
#endif

/*
 * cpu: show processor activity
 * Module: cpu.c
 * Author: Simon Anthony
 */

#include <sys/param.h>
#include <sys/pstat.h>
#include <sys/dk.h>

#include <unistd.h>
#include <stddef.h>
#include <string.h>
#define _XOPEN_SOURCE_EXTENDED
#include <curses.h>

#include <sys/utsname.h>
#include <fenv.h>
#include <math.h>

#ifdef USE_PSTAT_MAX
#define MAX_STATES	PST_MAX_CPUSTATES
#else
#define MAX_STATES	CPUSTATES
#endif
#define MAX_CPUS	128

#define K			1024

#define VT_VLINE	'x'
#define VT_HLINE	'q'
#define VT_ULCORNER	'l'
#define VT_URCORNER	'k'
#define VT_LLCORNER	'm'
#define VT_LRCORNER	'j'

#define Y_CPU	2
#define X_CPU	4
#define LINES_CPU ((sflg ? enb_proc_cnt/2 : enb_proc_cnt) + 1)
								/* number of lines required for CPUs */
#define Y_START	(Y_CPU + LINES_CPU)
								/* start position after CPUs have been drawn */
#define Y_FREE	(LINES - Y_CPU - LINES_CPU)
								/* free screen size starting at Y_START */

static WINDOW *w = NULL;					/* aggregate & top sub-window */
static WINDOW *w1 = NULL;					/* cpu sub-window */
static WINDOW *w2 = NULL;					/* cpu sub-window for split */

#ifdef __STDC__
extern char *getcpuinfo(char **, char **, char **);
extern int top(WINDOW *);
#else
extern char *getcpuinfo();
extern int top();
#endif

extern int interval;
extern int hflg;
extern int sflg;

void
#ifdef __STDC__
closewin(WINDOW *w)
#else
closewin(w)
 WINDOW *w;
#endif
{
	werase(w);
	delwin(w);
	touchwin(stdscr);
	refresh();
	w = NULL;
}

void
#ifdef __STDC__
cpu(void)
#else
cpu()
#endif
{
	struct pst_static pss;
	struct pst_dynamic psd;
	struct pst_processor *psp;
	struct pst_processor *psp_prev;
	struct pst_vminfo psv;
	struct pst_swapinfo psw;
	struct utsname	un;

	size_t	bufsize;
	char	*buffer;
	char	*bstr;
	int		count = 0;
	size_t	nspu = 0;
	chtype  sch = '=';
	chtype  uch = '-';
	chtype  memrch = '=';
	chtype  memuch = '-';
	int		has_alt = 0;
	int		has_col = 0;
	int		show_aggr = 0;
	int		show_info = 0;
	int		show_stats = 0;
	int		show_top = 0;
	int		show_mem = 0;
	int		current = 0;
	char	num[256];
	int		c;
	long int	su, uu, ur, x, y;
	int		enb_proc_cnt;	/* enabled processors index - starting at 0 */

	_T_LONG_T	cpu;
	_T_LONG_T	cpu_tot[MAX_CPUS];
	_T_LONG_T	cpu_tot_prev[MAX_CPUS];
	_T_LONG_T	exec_tot,		exec_tot_prev;
	_T_LONG_T	sysread_tot,	sysread_tot_prev;
	_T_LONG_T	syswrite_tot,	syswrite_tot_prev;
	_T_LONG_T	fsreads_tot,	fsreads_tot_prev;
	_T_LONG_T	fswrites_tot,	fswrites_tot_prev;
	_T_LONG_T	nfsreads_tot,	nfsreads_tot_prev;
	_T_LONG_T	nfswrites_tot,	nfswrites_tot_prev;
	_T_LONG_T	phread_tot,		phread_tot_prev;
	_T_LONG_T	phwrite_tot,	phwrite_tot_prev;
	_T_LONG_T	runocc_tot,		runocc_tot_prev;
	_T_LONG_T	runque_tot,		runque_tot_prev;
	_T_LONG_T	forks_prev = 0;

	_T_LONG_T	tot[MAX_STATES]; 

	if (uname(&un) != 0) {
		perror("uname");
		exit(1);
	}

	fesetround(FE_UPWARD);

	slk_init(1);

	initscr();
	noecho();
	cbreak();
	nonl();
	idlok(stdscr, TRUE);

	keypad(stdscr, TRUE);

	slk_set(1, "HELP",  1);
	slk_set(3, "MEM",   1);
	slk_set(4, "TOP",   1);
	slk_set(5, "STATS", 1);
	slk_set(6, "INFO",  1);
	slk_set(7, "HIST",  1);
	slk_set(8, "QUIT",  1);
	slk_attron(A_BOLD);

	slk_touch();
	slk_refresh();

	if (termattrs()&A_ALTCHARSET) {
		has_alt++;
		sch = 'a';		/* A_ALTCHARSET */
		uch = 'x';
		memuch = 'x';
		memrch = 'a';
	}
	if (has_colors()) {
		has_col++;
		start_color();
		init_pair(1, COLOR_YELLOW, COLOR_BLACK);
	}

	bstr = (char *) malloc(COLS);				/* blank */
	memset(bstr, ' ', COLS-1);

	/* Header */
	mvhline(0, 0, ' '|A_REVERSE, COLS);

	attron(A_REVERSE);

	mvprintw(0, 0, "%s", un.nodename);

	bufsize = confstr(_CS_MACHINE_MODEL, NULL, (size_t)0);
	buffer = (char *)malloc(bufsize);
	confstr(_CS_MACHINE_MODEL, buffer, bufsize);

	printw(" %s", buffer);
	free(buffer);

	attroff(A_REVERSE);

	if (pstat_getstatic(&pss, sizeof(pss), (size_t)1, 0) == -1)
		perror("pstat_static");

	if (pstat_getswap(&psw, sizeof(psw), (size_t)1, 0) == -1)
		perror("pstat_swap");


	for (;;) {
		if (nodelay(stdscr, TRUE) != OK)
			 mvprintw(2, 1, "DEBUG : failed to set NODELAY");
		switch (c = wgetch(stdscr)) {
		case '\006':	/* ^F - soft function keys */
			break;
		case '\004':	/* ^D */
		case KEY_DOWN:
			break;
		case '\025':	/* ^U */
		case KEY_UP:
			break;
		case KEY_RIGHT:
		case '\t':		/* tab */
			break;
		case KEY_LEFT:
		case KEY_BTAB:
			break;
		case '\040':	/* space */
			wrefresh(stdscr);
			break;
		case '\b':
		case KEY_BACKSPACE:
		case KEY_DC:
		case '\177':
			break;
		case '\014':	/* ^L */
			redrawwin(stdscr);
			wrefresh(stdscr);
			doupdate();
			continue;
		case '\n':
		case '\r':
			break;
		f1:
		case KEY_F(1):
		case '\013':	/* ^K */
			/*displayhelp();
			wrefresh(stdscr); */
			break;
		f3:
		case KEY_F(3):
		case 'm':
		case 'M':
			if (Y_FREE < 6)
				break;
			if (show_mem) {
				show_mem = 0;
				closewin(w);
				/* clear label */
				mvaddstr(Y_CPU + enb_proc_cnt + 4, 0, "    ");
				mvaddstr(LINES - 1, 0, "    ");
				current = 0;
			} else if (count) {
				show_aggr = 0; show_info = 0; show_stats = 0; show_top = 0;
				show_mem = 1;

				if (w) closewin(w);

				current = 0;

				w = subwin(stdscr, 6, COLS, 2 + LINES_CPU, 0);

				if (has_alt) {
					wattron(w, A_ALTCHARSET);
					wborder(w, VT_VLINE, VT_VLINE, VT_HLINE, VT_HLINE,
								VT_ULCORNER, VT_URCORNER, VT_LLCORNER, VT_LRCORNER);
				} else
					wborder(w, '|', '|', '-', '-', 0, 0, 0, 0);
				if (has_alt) wattroff(w, A_ALTCHARSET);
				touchwin(stdscr);
				wrefresh(w);
			}
			break;
		f4:
		case KEY_F(4):
		case 't':
		case 'T':
			if (Y_FREE < 4)
				break;
			if (show_top) {
				show_top = 0;
				closewin(w);
				/* clear label */
				mvaddstr(Y_CPU + enb_proc_cnt + 4, 0, "    ");
				mvaddstr(LINES - 1, 0, "    ");
				current = 0;
			} else if (count) {
				show_aggr = 0; show_info = 0; show_stats = 0; show_top = 1;
				show_mem = 0;

				if (w) closewin(w);

				current = 0;

				w = subwin(stdscr, Y_FREE, COLS, 2 + LINES_CPU, 0);

				if (has_alt) {
					wattron(w, A_ALTCHARSET);
					wborder(w, VT_VLINE, VT_VLINE, VT_HLINE, VT_HLINE,
								VT_ULCORNER, VT_URCORNER, VT_LLCORNER, VT_LRCORNER);
				} else
					wborder(w, '|', '|', '-', '-', 0, 0, 0, 0);
				if (has_alt) wattroff(w, A_ALTCHARSET);
				touchwin(stdscr);
				wrefresh(w);
			}
			break;
		f5:
		case KEY_F(5):
		case 's':
		case 'S':
			if (Y_FREE < 8)
				break;
			if (show_stats) {
				show_stats = 0;
				closewin(w);
			} else if (count) {
				if (show_aggr) {
					mvaddstr(Y_START, 0, "    ");	/* clear label */
					mvaddstr(LINES - 1, 0, "    ");
				}
				show_aggr = 0; show_info = 0; show_stats = 1; show_top = 0;
				show_mem = 0;

				if (w) closewin(w);

				w = subwin(stdscr, 8, COLS, Y_START, 0);

				if (has_alt) {
					wattron(w, A_ALTCHARSET);
					wborder(w, VT_VLINE, VT_VLINE, VT_HLINE, VT_HLINE,
								VT_ULCORNER, VT_URCORNER, VT_LLCORNER, VT_LRCORNER);
				} else
					wborder(w, '|', '|', '-', '-', 0, 0, 0, 0);
				if (has_alt) wattroff(w, A_ALTCHARSET);
				touchwin(stdscr);
				wrefresh(w);
			}
			break;
		f6:
		case KEY_F(6):
		case 'i':
		case 'I':
			if (Y_FREE < 4)
				break;
			if (show_info) {
				show_info = 0;
				closewin(w);
			} else if (count) {
				if (show_aggr) {
					/* clear label */
					mvaddstr(Y_CPU + psd.psd_max_proc_cnt, 0, "    ");
					mvaddstr(LINES - 1, 0, "    ");
				}
				show_aggr = 0; show_info = 1; show_stats = 0; show_top = 0;
				show_mem = 0;

				if (w) closewin(w);

				if ((w = newpad(nspu + 1, COLS)) == NULL) {
					fprintf(stderr, "PAD Failed\n");
					endwin();
					exit(54);
				}
			}
			break;
		f7:
		case KEY_F(7):
		case 'h':
		case 'H':
			if (Y_FREE < 4)
				break;
			if (show_aggr) {
				show_aggr = 0;
				closewin(w);
				mvaddstr(Y_START, 0, "    ");	/* clear label */
				mvaddstr(LINES - 1, 0, "    ");
				current = 0;
			} else if (count) {
				show_aggr = 1; show_info = 0; show_stats = 0; show_top = 0;
				show_mem = 0;

				if (w) closewin(w);

				current = 0;

				w = subwin(stdscr, Y_FREE, COLS-4, Y_START, 4);
				if (has_alt) {
					wattron(w, A_ALTCHARSET);
					wborder(w, VT_VLINE, VT_VLINE, VT_HLINE, VT_HLINE,
								VT_ULCORNER, VT_URCORNER, VT_LLCORNER, VT_LRCORNER);
				} else
					wborder(w, '|', '|', '-', '-', 0, 0, 0, 0);
				if (has_alt) wattroff(w, A_ALTCHARSET);

				mvaddstr(Y_START, 0, "100%");			/* draw label */
				mvaddstr(LINES - 1, 0, "  0%");
				touchwin(stdscr);
				wrefresh(w);
			}
			break;
		f8:
		case KEY_F(8):
		case 'q':
		case 'Q':
			//clear();
			//refresh();
			slk_clear();
			endwin();
			exit(0);
		default:
			break;
		}
		nodelay(stdscr, FALSE);

		if (pstat_getdynamic(&psd, sizeof(psd), (size_t)1, 0) != -1) {
			if (nspu && nspu != psd.psd_max_proc_cnt) {
				nspu = 0; 	/* processor count has changed */
				if (psp) free(psp);
				if (psp_prev) free(psp_prev);
			}
			if (!nspu) {
				nspu = psd.psd_max_proc_cnt;
				psp = (struct pst_processor *)
					malloc(nspu * sizeof(struct pst_processor));

				psp_prev = (struct pst_processor *)
					malloc(nspu * sizeof(struct pst_processor));
			}

			mvprintw(1, 1, "load average: %0.2f, %0.2f, %0.2f",
				psd.psd_avg_1_min, psd.psd_avg_5_min, psd.psd_avg_15_min);
			attron(A_REVERSE);
			mvprintw(0, COLS-18, "Processors:%3d/%-3d", psd.psd_proc_cnt, psd.psd_max_proc_cnt);
			attroff(A_REVERSE);
		} else
			perror("pstat_getdynamic");

		if (pstat_getprocessor(psp,
								sizeof(struct pst_processor), nspu, 0) != -1) {
			int cpu, state;

			if (count == 0)
				goto nodraw;

			if ((!hflg && nspu >= 20) || enb_proc_cnt + 1 >= 20)
				sflg++;

			if (count == 1) {
				if (LINES < (sflg ? (enb_proc_cnt + 1)/2 : enb_proc_cnt + 1) + Y_CPU) {
					slk_clear();
					endwin();
					fprintf(stderr, "cpu: too few lines [%d] for CPUs [%d]\n",
						LINES, hflg ? enb_proc_cnt + 1 : nspu);
					exit(1);
				}
			}

			if (!w1) {
				if (sflg)
					w1 = subwin(stdscr, (enb_proc_cnt + 1)/2, COLS/2, 2, 0);
				else
					w1 = subwin(stdscr, enb_proc_cnt + 1, COLS, 2, 0);
#ifdef BORDER
				if (has_alt) {
					wattron(w1, A_ALTCHARSET);
					wborder(w1, VT_VLINE, VT_VLINE, VT_HLINE, VT_HLINE,
								VT_ULCORNER, VT_URCORNER, VT_LLCORNER, VT_LRCORNER);
				} else
					wborder(w1, '|', '|', '-', '-', 0, 0, 0, 0);
				if (has_alt) wattroff(w1, A_ALTCHARSET);
#endif			
				touchwin(stdscr);
			}

			if (sflg) {
				if (!w2)
					w2 = subwin(stdscr, (enb_proc_cnt + 1)/2, COLS/2, 2, COLS/2);
#ifdef BORDER
				if (has_alt) {
					wattron(w2, A_ALTCHARSET);
					wborder(w2, VT_VLINE, VT_VLINE, VT_HLINE, VT_HLINE,
								VT_ULCORNER, VT_URCORNER, VT_LLCORNER, VT_LRCORNER);
				} else
					wborder(w2, '|', '|', '-', '-', 0, 0, 0, 0);
				if (has_alt) wattroff(w1, A_ALTCHARSET);
#endif
				touchwin(stdscr);
			}
		nodraw:	
			enb_proc_cnt = -1;

			/* Per CPU */

			exec_tot = 0;				/* for all cpus */
			exec_tot_prev = 0;

			getmaxyx(w, y, x);

			if (++current >= x-1)			/* bigger than sub-window? */
				current = 0;

			sysread_tot = 0;	sysread_tot_prev = 0;
			syswrite_tot = 0;	syswrite_tot_prev = 0;

			fsreads_tot = 0;	fsreads_tot_prev = 0;
			fswrites_tot = 0;	fswrites_tot_prev = 0;

			nfsreads_tot = 0;	nfsreads_tot_prev = 0;
			nfswrites_tot = 0;	nfswrites_tot_prev = 0;

			phread_tot = 0;		phread_tot_prev = 0;
			phwrite_tot = 0;	phwrite_tot_prev = 0;

			runocc_tot = 0,		runocc_tot_prev = 0;
			runque_tot = 0,		runque_tot_prev = 0;


			for (state = 0; state < MAX_STATES; state++)
				tot[state] = 0;

			for (cpu = 0; cpu < nspu; cpu++) {
				int	X, Y;
				int	altwin = 0;

#ifdef _ICOD_BASE_INFO
				if (hflg) {
					if (psp[cpu].psp_processor_state == PSP_SPU_ENABLED)
						enb_proc_cnt++;
				} else
					enb_proc_cnt++;
#endif
				if (count == 0)
					goto next;

				exec_tot += psp[cpu].psp_sysexec;
				exec_tot_prev += psp_prev[cpu].psp_sysexec;

				sysread_tot += psp[cpu].psp_sysread;
				sysread_tot_prev += psp_prev[cpu].psp_sysread;

				syswrite_tot += psp[cpu].psp_syswrite;
				syswrite_tot_prev += psp_prev[cpu].psp_syswrite;

				fsreads_tot += psp[cpu].psp_fsreads;
				fsreads_tot_prev += psp_prev[cpu].psp_fsreads;

				fswrites_tot += psp[cpu].psp_fswrites;
				fswrites_tot_prev += psp_prev[cpu].psp_fswrites;

				nfsreads_tot += psp[cpu].psp_nfsreads;
				nfsreads_tot_prev += psp_prev[cpu].psp_nfsreads;

				nfswrites_tot += psp[cpu].psp_nfswrites;
				nfswrites_tot_prev += psp_prev[cpu].psp_nfswrites;

				phread_tot += psp[cpu].psp_phread;
				phread_tot_prev += psp_prev[cpu].psp_phread;

				phwrite_tot += psp[cpu].psp_phwrite;
				phwrite_tot_prev += psp_prev[cpu].psp_phwrite;

				runocc_tot += psp[cpu].psp_runocc;
				runocc_tot_prev += psp_prev[cpu].psp_runocc;

				runque_tot += psp[cpu].psp_runque;
				runque_tot_prev += psp_prev[cpu].psp_runque;

				cpu_tot[cpu] = 0;		/* per cpu */
				cpu_tot_prev[cpu] = 0;

				for (state = 0; state < MAX_STATES; state++) {
					cpu_tot[cpu] += psp[cpu].psp_cpu_time[state];
					cpu_tot_prev[cpu] += psp_prev[cpu].psp_cpu_time[state]; 

#ifdef _ICOD_BASE_INFO
					if (psp[cpu].psp_processor_state == PSP_SPU_ENABLED)
						tot[state] += 
							psp[cpu].psp_cpu_time[state] - psp_prev[cpu].psp_cpu_time[state];
#else
					tot[state] += 
						psp[cpu].psp_cpu_time[state] - psp_prev[cpu].psp_cpu_time[state];
#endif
				}

				if (hflg && psp[cpu].psp_processor_state != PSP_SPU_ENABLED)
					goto next;

				/* Draw the CPU */

				getmaxyx(w1, Y, X);

				altwin = sflg && enb_proc_cnt > Y - 1;

				/* clear line */
				mvwaddnstr(altwin ? w2 : w1,
					0 + enb_proc_cnt - (altwin ? Y : 0), 2, bstr, -1);

				wattron(altwin ? w2 : w1, A_BOLD);

				/* label the cpu# */
#ifdef _ICOD_BASE_INFO
				if (psp[cpu].psp_processor_state == PSP_SPU_DISABLED ||
					psp[cpu].psp_processor_state == PSP_SPU_HW_FAIL_DISABLED)
					wattroff(altwin ? w2 : w1, A_BOLD);

				mvwprintw(altwin ? w2 : w1,
					0 + enb_proc_cnt - (altwin ? Y : 0), 1,
					"%2d:", psp[cpu].psp_logical_id);

				wattron(altwin ? w2 : w1, A_BOLD);
#else
				mvwprintw(altwin ? w2 : w1,
					0 + enb_proc_cnt - (altwin ? Y : 0), 1,
					"%2d:", psp[cpu].psp_idx);
#endif
				wattroff(altwin ? w2 : w1, A_BOLD);

				/* draw system time */
				if (has_alt) wattron(altwin ? w2 : w1, A_ALTCHARSET);
				if (has_col) wattron(altwin ? w2 : w1, COLOR_PAIR(1));

				feclearexcept(FE_ALL_EXCEPT);

				su = lrint((double)(psp[cpu].psp_cpu_time[CP_SYS]-
									psp_prev[cpu].psp_cpu_time[CP_SYS]) /
								(double)(cpu_tot[cpu]-
									cpu_tot_prev[cpu])*X);

				if (fetestexcept(FE_INVALID)) su = 0;

				for (x = 4, y = 0 + enb_proc_cnt - (altwin ? Y : 0); x < 4 + su; x++)
					mvwaddch(altwin ? w2 : w1, y, x, sch);

				/* draw user time */
				if (has_col) wattroff(altwin ? w2 : w1, COLOR_PAIR(1));

				feclearexcept(FE_ALL_EXCEPT);

				uu = lrint((double)(psp[cpu].psp_cpu_time[CP_USER]-
									psp_prev[cpu].psp_cpu_time[CP_USER]) /
							(double)(cpu_tot[cpu]-
									cpu_tot_prev[cpu])*X);

				if (fetestexcept(FE_INVALID)) su = 0;


				for (x = 4 + su, y = 0 + enb_proc_cnt - (altwin ? Y : 0); x < 4 + su + uu; x++)
					mvwaddch(altwin ? w2 : w1, y, x, uch);

				if (has_alt) wattroff(altwin ? w2 : w1, A_ALTCHARSET);
			next:
				/* save values */

				for (state = 0; state < MAX_STATES; state++) 
					psp_prev[cpu].psp_cpu_time[state] =
						psp[cpu].psp_cpu_time[state];

				psp_prev[cpu].psp_sysexec = psp[cpu].psp_sysexec;

				psp_prev[cpu].psp_sysread = psp[cpu].psp_sysread;
				psp_prev[cpu].psp_syswrite = psp[cpu].psp_syswrite;

				psp_prev[cpu].psp_fsreads = psp[cpu].psp_fsreads;
				psp_prev[cpu].psp_fswrites = psp[cpu].psp_fswrites;

				psp_prev[cpu].psp_nfsreads = psp[cpu].psp_nfsreads;
				psp_prev[cpu].psp_nfswrites = psp[cpu].psp_nfswrites;

				psp_prev[cpu].psp_phread = psp[cpu].psp_phread;
				psp_prev[cpu].psp_phwrite = psp[cpu].psp_phwrite;

				psp_prev[cpu].psp_runocc = psp[cpu].psp_runocc;
				psp_prev[cpu].psp_runque = psp[cpu].psp_runque;
			}
		} else
			perror("pstat_getprocessor");

		if (w1)
			wrefresh(w1);
		if (w2)
			wrefresh(w2);

#ifndef MEM
		if (count && show_mem) {
			int		Y, X, y, x;

			getmaxyx(w, Y, X);
			if (pstat_getvminfo(&psv, sizeof(psv), (size_t)1, 0) != -1) {
				_T_LONG_T	total_used, total_avail, total_free;

				/* Draw the memory */

				total_avail = psv.psv_swapspc_max + psv.psv_swapmem_max;
				total_used = total_avail - psv.psv_swapspc_cnt - psv.psv_swapmem_cnt;
				total_free = total_avail - total_used;

				wattron(w, A_REVERSE);

				if (has_alt) wattron(w, A_ALTCHARSET);

				for (x = 1, y = 1; x < X-1; x++)		/* VM total bar */
					mvwaddch(w, y, x, ' ');

				ur = lrint((double)psd.psd_rm / (double)total_avail * (X-1));

				wattron(w, A_BLINK);

				for (x = 1, y = 1; x < 1 + ur; x++)			/* %real */
					mvwaddch(w, y, x, 'r');

				wattroff(w, A_REVERSE);

				uu = lrint((double)total_used / (double)total_avail * (X-1));

				for (x = 1, y = 1; x < 1 + uu; x++) {		/* %used */
					if (x > ur)
						wattroff(w, A_BLINK);
					mvwaddch(w, y, x, memuch); 
				}

				wattroff(w, A_BLINK);

				if (has_alt) wattroff(w, A_ALTCHARSET);

				mvwaddnstr(w, 2, 1, bstr, X-2); /* clear line to border */
				mvwprintw(w, 2, 1, "Mem: %.0fK (%.0fK) real, %.0fK (%.0fK) virtual, %.0fK free",
					(float)psd.psd_rm * pss.page_size / K,
					(float)psd.psd_arm * pss.page_size / K,
					(float)psd.psd_vm * pss.page_size / K,
					(float)psd.psd_avm * pss.page_size / K,
					(float)psd.psd_free * pss.page_size / K);

				mvwaddnstr(w, 3, 1, bstr, X-2); /* clear line to border */
				mvwprintw(w, 3, 1, "VM: %.0fK avail, %.0fK used, %.0fK free",
					(float)total_avail * pss.page_size / K,
					(float)total_used * pss.page_size / K,
					(float)total_free * pss.page_size / K);

				if (has_alt) wattron(w, A_ALTCHARSET);
				wattron(w, A_REVERSE);
				mvwprintw(w, 4, 2, "  ");
				wattroff(w, A_REVERSE);
				wattron(w, A_BLINK);
				mvwprintw(w, 4, 14, "  ");
				wattroff(w, A_BLINK);
				mvwprintw(w, 4, 26, "%c%c", memuch, memuch);
				if (has_alt) wattroff(w, A_ALTCHARSET);

				mvwprintw(w, 4,  5, "Free");
				mvwprintw(w, 4, 17, "Real");
				mvwprintw(w, 4, 29, "Used");

			} else
				perror("pstat_getvminfo");

			wrefresh(w);
		}
#endif

		if (count && show_stats) {
			int		Y, X, y, x;

			getmaxyx(w, Y, X);

			mvwprintw(w, 1, 1, "Forks  %8d Execs   %8d", 
				psv.psv_cntfork - forks_prev,
				(exec_tot - exec_tot_prev)/interval);
			forks_prev = psv.psv_cntfork;

			mvwprintw(w, 2, 1, "Faults %8d Intr    %8d", 
				psv.psv_rfaults,
				psv.psv_rintr);

			mvwprintw(w, 3, 1, "RdSys  %8d WrSys   %8d",
				(sysread_tot - sysread_tot_prev)/interval,
				(syswrite_tot - syswrite_tot_prev)/interval);

			mvwprintw(w, 4, 1, "PageIn %8d SwapIn  %8d",
				psv.psv_rpgin, /* psv_rpgin or psv.psv_rpgpgin? */
				psv.psv_rpswpin); /* psv_rswpin or psv_rpswpin? */

			mvwprintw(w, 5, 1, "PageOut%8d SwapOut %8d",
				psv.psv_rpgout, /* psv_rpgout or psv.psv_rpgpgout? */
				psv.psv_rpswpout); /* psv_rswpout or psv_rpswpout? */

			mvwprintw(w, 6, 1, "PageRec%8d", psv.psv_rpgrec);

			mvwprintw(w, 1, 34, "ReadFS %8d WritFS  %8d",
				(fsreads_tot - fsreads_tot_prev)/interval,
				(fswrites_tot - fswrites_tot_prev)/interval);

			mvwprintw(w, 2, 34, "ReadNFS%8d WritNFS %8d",
				(nfsreads_tot - nfsreads_tot_prev)/interval,
				(nfswrites_tot - nfswrites_tot_prev)/interval);

			mvwprintw(w, 3, 34, "ReadRaw%8d WritRaw %8d",
				(phread_tot - phread_tot_prev)/interval,
				(phwrite_tot - phwrite_tot_prev)/interval);

			mvwprintw(w, 4, 34, "RunOcc %8d RunQue  %8d", 
				runocc_tot - runocc_tot_prev,
				runque_tot - runque_tot_prev);

			mvwprintw(w, 5, 34, "Active %8d Sleep   %8d",
				psd.psd_activeprocs,
				psd.psd_sl);

			mvwprintw(w, 6, 34, "Wait   %8d RunQ    %8d",
				psd.psd_dw + psd.psd_pw + psd.psd_sw, psd.psd_rq);

			wrefresh(w);
		}

		if (count && show_aggr) {
			int		Y, X, y, x;
			_T_LONG_T total = 0;
			int		state;

			getmaxyx(w, Y, X);

			x = current;

			if (x == 1) {
				wmove (w, 0, 0);
				wclrtobot(w);
				if (has_alt) {
					wattron(w, A_ALTCHARSET);
					wborder(w, VT_VLINE, VT_VLINE, VT_HLINE, VT_HLINE,
								VT_ULCORNER, VT_URCORNER, VT_LLCORNER, VT_LRCORNER);
				} else
					wborder(w, '|', '|', '-', '-', 0, 0, 0, 0);
				if (has_alt) wattroff(w, A_ALTCHARSET);
			}

			for (state = 0; state < MAX_STATES; state++)
				total += tot[state];

			if (has_alt) wattron(w, A_ALTCHARSET);

			su = lrint((double)(tot[CP_SYS]/(double)total)*(Y-2));

			for (y = Y-2; y > Y-2-su ; y--)
				mvwaddch(w, y, x, sch);

			uu = lrint((double)(tot[CP_USER]/(double)total)*(Y-2));

			for (y = Y-2 - su; y > Y-2-su-uu ; y--)
				mvwaddch(w, y, x, uch);

			if (has_alt) wattroff(w, A_ALTCHARSET);

			wrefresh(w);
		}

		if (count && show_top) {
			wmove (w, 0, 0);
			wclrtobot(w);
			if (has_alt) {
				wattron(w, A_ALTCHARSET);
				wborder(w, VT_VLINE, VT_VLINE, VT_HLINE, VT_HLINE,
							VT_ULCORNER, VT_URCORNER, VT_LLCORNER, VT_LRCORNER);
			} else
				wborder(w, '|', '|', '-', '-', 0, 0, 0, 0);
			if (has_alt) wattroff(w, A_ALTCHARSET);
			touchwin(stdscr);
			top(w);
			wrefresh(w);
		}

		if (count && show_info) {
			char	*model, *version, *features;
			int		Y, X, y, x;
			int		pminrow = 0;

			getmaxyx(w, Y, X);

			/* Header (in stdscr) */
			mvhline(Y_START, 0, ' '|A_REVERSE, X);

			attron(A_REVERSE);
			mvprintw(Y_START, 0, "%3s %7s %-11s %-24s %5s %-9s %-9s %-3s",
				"CPU", "Model", "Version", "Features", "Speed", "CoProc", "State", "I/O");
			attroff(A_REVERSE);

			/* Put all the CPU information into the pad */

			for (cpu = 0; cpu < psd.psd_max_proc_cnt; cpu++) {
				getcpuinfo(&model, &version, &features);
				mvwprintw(w, cpu+0, 0,
					"%3ld: %6s %-11s %-24s %5ld",
						cpu, model, version, features,
						lrint((double)psp[cpu].psp_iticksperclktick / 10000L));

				if (psp[cpu].psp_coprocessor.psc_present & PS_PA83_FPU)
					mvwprintw(w, cpu+0, 55, "%-5s", "PA83/");
				else
					mvwprintw(w, cpu+0, 55, "%-5s", "    /");

				if (psp[cpu].psp_coprocessor.psc_present & PS_PA89_FPU)
					mvwprintw(w, cpu+0, 60, "%-5s", "PA89");
				else
					mvwprintw(w, cpu+0, 60, "%-5s", "    ");

#ifdef _ICOD_BASE_INFO
				switch(psp[cpu].psp_processor_state) {
				case PSP_SPU_ENABLED:
					mvwprintw(w, cpu+0, 65, "%-10s", "Enabled");
					break;
				case PSP_SPU_DISABLED:
					mvwprintw(w, cpu+0, 65, "%-10s", "Disabled");
					break;
				case PSP_SPU_INTRANSITION:
					mvwprintw(w, cpu+0, 65, "%-10s", "Transitn");
					break;
				case PSP_SPU_HW_FAIL_DISABLED:
					mvwprintw(w, cpu+0, 65, "%-10s", "Failed");
					break;
				default:
					break;
				}

				if (psp[cpu].psp_processor_state == PSP_SPU_ENABLED)
					switch(psp[cpu].psp_flags) {
					case PSP_INTERRUPT_ENABLED:
						mvwprintw(w, cpu+0, 75, "%-3s", "En");
						break;
					case PSP_INTERRUPT_DISABLED:
						mvwprintw(w, cpu+0, 75, "%-3s", "Dis");
						break;
					}
#endif
			}
			slk_set(1, "",  1);
			slk_set(3, "",   1);
			slk_set(4, "",   1);
			slk_set(5, "", 1);
			slk_set(6, "INFO",  1);
			slk_set(7, "",  1);
			slk_set(8, "QUIT",  1);
			slk_touch();
			slk_refresh();

			for (;;) {
				int	i;
				nodelay(stdscr, TRUE);
				switch (c = wgetch(stdscr)) {
				case '\006':	/* ^F - soft function keys */
					break;
				case '\004':	/* ^D */
				case KEY_DOWN:
					if (pminrow < (nspu + 1 - Y_FREE))
						pminrow++;
					for (i = Y_START + 1;
						 i < LINES; i++) {
						move(i, 0);
						clrtoeol();
					}
					break;
				case '\025':	/* ^U */
				case KEY_UP:
					if (pminrow > 0)
						pminrow--;
					break;
				case 'I':
				case 'i':
					closewin(w);
					show_info = 0;
					slk_set(1, "HELP",  1);
					slk_set(3, "MEM",   1);
					slk_set(4, "TOP",   1);
					slk_set(5, "STATS", 1);
					slk_set(6, "INFO",  1);
					slk_set(7, "HIST",  1);
					slk_set(8, "QUIT",  1);
					slk_attron(A_BOLD);

					slk_touch();
					slk_refresh();
					move(Y_START, 0);
					clrtoeol();
					break;
				case 'Q':
				case 'q':
					slk_clear();
					endwin();
					exit(0);
				default:
					break;
				}
				nodelay(stdscr, FALSE);
				if (!show_info)
					break;
				prefresh(w, pminrow, 0,
					Y_START + 1, 0,
					Y_FREE - 1, COLS);
				doupdate();
			}
		}

		count++; 
		refresh();
		doupdate();
		sleep(interval);
	}
	endwin();
	exit(0);
}
