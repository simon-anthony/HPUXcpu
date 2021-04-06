#ident $Id: Makefile 240 2012-05-22 13:39:38Z dba15 $

CMDS = cpu top

CFLAGS = -g -D_HPUX_SOURCE -DETCDIR=/etc -D_ICOD_BASE_INFO -D_PSTAT64 -Aa +e -lm
LDLIBS = -lgen

cpu: cpu.c main.c ctop.o getcpuinfo.c
	$(CC) $(CFLAGS) $@.c main.c getcpuinfo.c ctop.o -lcurses -lm -o $@

ctop.o: ctop.c
	$(CC) -D_HPUX_SOURCE -D_PSTAT64 $*.c -c $@

top: ctop.c
	$(CC) -D_HPUX_SOURCE -D_PSTAT64 -DMAIN ctop.c -lm -o top

clean:
	rm -f $(CMDS) *.o

ignore:
	@echo $(CMDS) *.o | xargs -n1 printf "%s\n" > .ignores
	@svn propset svn:$@ -F .ignores .
	@rm -f .ignores

keywords:
	svn propset svn:$@ "Id Header" `svn list --depth files`
