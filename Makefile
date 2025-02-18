# Generated automatically from Makefile.in by configure.
SHELL= /bin/sh

srcdir= .

CC= cc
INSTALL= /usr/bin/install -c
LN= ln -sf

prefix= /arbornet
exec_prefix= ${prefix}
bindir= ${exec_prefix}/bin
mandir= ${prefix}/man
datadir= ${prefix}/share
sysconfdir= ${prefix}/etc
localstatedir= ${prefix}/var

groupid= tty

CPPFLAGS= 
CFLAGS= -g -O2
LDFLAGS= 
LIB= -lcrypt 

all: write mesg amin huh helpers

WSRC= wrt_main.c wrt_type.c wrt_him.c wrt_me.c wrt_opt.c wrt_sig.c \
	wrt_tty.c wrt_hist.c lib_common.c getutent.c
WOBJ= wrt_main.o wrt_type.o wrt_him.o wrt_me.o wrt_opt.o wrt_sig.o \
	wrt_tty.o wrt_hist.o lib_common.o getutent.o

write: $(WOBJ)
	$(CC) $(CFLAGS) -o write $(WOBJ) $(LIB)

wrt_main.o: wrt_main.c wrttmp.h write.h config.h orville.h
wrt_type.o: wrt_type.c wrttmp.h write.h config.h orville.h
wrt_him.o: wrt_him.c lib_common.h getutent.h wrttmp.h write.h config.h orville.h
wrt_me.o: wrt_me.c lib_common.h getutent.h wrttmp.h write.h config.h orville.h
wrt_opt.o: wrt_opt.c wrttmp.h write.h config.h orville.h
wrt_sig.o: wrt_sig.c wrttmp.h write.h config.h orville.h
wrt_tty.o: wrt_tty.c wrttmp.h write.h config.h orville.h
wrt_hist.o: wrt_hist.c wrttmp.h write.h config.h orville.h
lib_common.o:lib_common.c getutent.h lib_common.h wrttmp.h config.h orville.h
getutent.o: getutent.c getutent.h wrttmp.h config.h orville.h
mesg.o: mesg.c wrttmp.h lib_common.h config.h orville.h
amin.o: amin.c wrttmp.h lib_common.h config.h orville.h
huh.o: huh.c wrttmp.h lib_common.h config.h orville.h
helpers.o: helpers.c wrttmp.h getutent.h config.h orville.h
wt.o: wt.c wrttmp.h config.h orville.h

mesg: mesg.o lib_common.o getutent.o
	$(CC) $(LDFLAGS) -o mesg mesg.o lib_common.o getutent.o

amin: amin.o lib_common.o getutent.o
	$(CC) $(LDFLAGS) -o amin amin.o lib_common.o getutent.o

huh: huh.o lib_common.o getutent.o
	$(CC) $(LDFLAGS) -o huh huh.o lib_common.o getutent.o

helpers: helpers.o getutent.o
	$(CC) $(LDFLAGS) -o helpers helpers.o getutent.o lib_common.o

wt: wt.o
	$(CC) $(LDFLAGS) -o wt wt.c lib_common.o getutent.o

install: write mesg amin huh helpers
	./install-sh -d $(bindir)
	$(INSTALL) -o root -g $(groupid) -m 6711 write $(bindir)
	-(cd $(bindir); $(LN) write jot; $(LN) write tel; $(LN) write telegram)
	$(INSTALL) -o root -m 4711 mesg $(bindir)
	$(INSTALL) -o root -m 4711 amin $(bindir)
	$(INSTALL) -o root -m 4711 huh $(bindir)
	$(INSTALL) -m 4711 helpers $(bindir)
	./install-sh -d $(sysconfdir)
	$(INSTALL) -m 644 orville.conf $(sysconfdir)
	-rm -f $(sysconfdir)/wrttmp
	touch $(sysconfdir)/wrttmp
	chown root $(sysconfdir)/wrttmp
	chmod 600 $(sysconfdir)/wrttmp
	-rm -f $(sysconfdir)/wrthist
	touch $(sysconfdir)/wrthist
	chmod 600 $(sysconfdir)/wrthist
	./install-sh -d $(mandir)/man1
	$(INSTALL) -m 644 write.1 $(mandir)/man1
	$(INSTALL) -m 644 mesg.1 $(mandir)/man1
	$(INSTALL) -m 644 amin.1 $(mandir)/man1
	$(INSTALL) -m 644 huh.1 $(mandir)/man1
	$(INSTALL) -m 644 helpers.1 $(mandir)/man1

write.tar: README INSTALLATION CHANGES $(WSRC) wrttmp.h write.h orville.h \
	lib_common.h getutent.h mesg.c amin.c huh.c amin.1 write.1 mesg.1 \
	huh.1 wrttab wt.c say Makefile.in configure.in configure install-sh \
	config.h.in orville.conf
	tar cvf write.tar README INSTALLATION CHANGES $(WSRC) write.h \
	wrttmp.h orville.h lib_common.h getutent.h mesg.c amin.c huh.c amin.1 \
	write.1 mesg.1 huh.1 wrttab wt.c helpers.c helpers.1 say \
	Makefile.in configure.in configure install-sh config.h.in orville.conf

clean:
	-rm -f wt core *.o
