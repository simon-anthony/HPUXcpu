/* @(#)getcpuinfo.c	1.3
 * Copyright (c) 2002 Hewlett-Packard Company.  All Rights Reserved.
 *
 * Permission to use, copy, and modify this software and its documentation
 * is hereby granted under the following terms and conditions.  The above
 * copyright notice, this permission notice, and the following disclaimer
 * must appear in all copies of the software, derivative works or modified
 * versions.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND HEWLETT-PACKARD CO. DISCLAIMS ALL
 * WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS.   IN NO EVENT SHALL HEWLETT-PACKARD
 * COMPANY BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
 * PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS
 * ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
 * SOFTWARE.
 */

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>

#include <sys/param.h>
#include <sys/pstat.h>
#include <sys/utsname.h>

char *vendor = "Hewlett-Packard Company";

/* N.B. This program will compile in a non-AnsiC environment */

/*
 * getcpuinfo(model, version) - return CPU model/version/feature strings.
 */
void
#ifdef __STDC__
getcpuinfo(char **mod_p, char **ver_p, char **feat_p)
#else
getcpuinfo(mod_p, ver_p, feat_p)
	char **mod_p, **ver_p, **feat_p;
#endif
{
	static int firstime = 1;
	static char model[128], version[128], feature[128];
	struct utsname uts;
	char *vbuf1, *vbuf2, *cp, line[256];
	FILE *fp;
	long confret;

	/* It would appear that HP platforms do not support mixed CPUs */
	if (firstime == 0)
		goto usecache;

	firstime = 0;
	*model = *version = *feature = '\0';

	/* Get model information */
	if (uname(&uts) >= 0) {
		if ((vbuf1 = strchr(uts.machine, '/')) == NULL) {
			strncpy(model, uts.machine, sizeof model);
		} else {
			vbuf1++;
			if ((fp = fopen("/opt/langtools/lib/sched.models",
						"r")) != NULL ||
			    (fp = fopen("/usr/lib/sched.models",
						"r")) != NULL) {
				while (fgets(line, sizeof line, fp) != NULL) {
					cp = line;
					if (*cp==';' || *cp=='#' || *cp=='\n')
						continue;

#define	GETFIELD(_cp)							\
					while (*_cp != '\0' && !isspace(*_cp)) \
						_cp++;			\
					if (*_cp == '\0')		\
						continue;		\
					*_cp++ = '\0'
#define	NXTFIELD(_cp)							\
					while (isspace(*_cp))		\
						_cp++

					GETFIELD(cp);
					if (strcmp(line, vbuf1) != 0)
						continue;
					NXTFIELD(cp);
					GETFIELD(cp);
					NXTFIELD(cp);
					vbuf2 = cp;
					GETFIELD(cp);
					strncpy(model, vbuf2, sizeof model);
					break;
#undef NXTFIELD
#undef GETFIELD
				}
				fclose(fp);
			}
			if (*model == '\0')	/* cant match this model */
				snprintf(model, sizeof model, "(%s)", vbuf1);
		}
	}

	/* Get CPU version information */
	if ((confret = sysconf(_SC_CPU_VERSION)) != -1) {
		vbuf1 = ((confret & CPU_PA_RISC_MAX) != confret)?
			"Intel": (confret < CPU_PA_RISC1_0)?
			"Motorola": "PA-RISC";
		switch(confret) {
		    case CPU_HP_MC68020:	vbuf2 = "MC68020"; break;
		    case CPU_HP_MC68030:	vbuf2 = "MC68030"; break;
		    case CPU_HP_MC68040:	vbuf2 = "MC68040"; break;
		    case CPU_PA_RISC1_0:	vbuf2 = "1.0"; break;
		    case CPU_PA_RISC1_1:	vbuf2 = "1.1"; break;
		    case CPU_PA_RISC1_2:	vbuf2 = "1.2"; break;
#ifdef CPU_PA_RISC2_0
		    case CPU_PA_RISC2_0:
#else
		    case 0x214:
#endif
						vbuf2 = "2.0"; break;
#ifdef CPU_HP_INTEL_EM_1_0
		    case CPU_HP_INTEL_EM_1_0:
#else
		    case 0x300:
#endif
						vbuf2 = "1.0"; break;
#ifdef CPU_HP_INTEL_EM_1_1	/* just guessing... */
		    case CPU_HP_INTEL_EM_1_1:	vbuf2 = "1.1"; break;
#endif
#ifdef CPU_HP_INTEL_EM_1_2	/* just guessing... */
		    case CPU_HP_INTEL_EM_1_2:	vbuf2 = "1.2"; break;
#endif
#ifdef CPU_HP_INTEL_EM_2_0	/* just guessing... */
		    case CPU_HP_INTEL_EM_2_0:	vbuf2 = "2.0"; break;
#endif
		    default:			vbuf2 = "?.?"; break;
		}
		sprintf(version, "%s %s", vbuf1, vbuf2);
	}

	/* Get any special processor features */
	if ((confret = sysconf(_SC_KERNEL_BITS)) != -1)
		sprintf(feature, "%ld-bit", confret);

	if ((confret = sysconf(_SC_CPU_KEYBITS1)) != -1) {
		if ((confret & HARITH) != 0)
			strcat(feature, " HARITH");
		if ((confret & HSHIFTADD) != 0)
			strcat(feature, " HSHIFTADD");
	}

usecache:
	*mod_p = model;
	*ver_p = version;
	*feat_p = feature;
}

