
CC = /usr/pubsw/bin/gcc
CFLAGS = -g -DDEBUG -Wall -ansi
INCLUDES = -I/usr/pubsw/include
LIBPATHS = -R/usr/pubsw/lib
LIBOBJS = -lgssapi_krb5 -lsocket -lnsl -lresolv

STATIC_FLAG = -static

all: remctld remctl

remctld: remctld.o gss-utils.o vector.o
	$(CC) $(CFLAGS) $(INCLUDES) $(LIBPATHS) $(LIBOBJS) -o remctld remctld.o gss-utils.o vector.o

remctl: remctl.o gss-utils.o
	$(CC) $(CFLAGS) $(INCLUDES) $(LIBPATHS) $(LIBOBJS) -o remctl remctl.o gss-utils.o

vector.o: vector.c vector.h Makefile

gss-utils.o: gss-utils.c gss-utils.h Makefile

remctld.o: remctld.c gss-utils.h Makefile

remctl.o: remctl.c gss-utils.h Makefile

clean:
	rm -f *.o remctld remctl
