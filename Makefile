
CC = /usr/pubsw/bin/gcc
CFLAGS = -g -DDEBUG -Wall -ansi
LDFLAGS = -lgssapi_krb5 -lsocket -lnsl -lresolv
STATIC_FLAG = -static

all: remctld remctl

remctld: remctld.o gss-utils.o
	$(CC) $(CFLAGS) $(LDFLAGS) -o remctld remctld.o gss-utils.o

remctl: remctl.o gss-utils.o
	$(CC) $(CFLAGS) $(LDFLAGS) -o remctl remctl.o gss-utils.o

gss-utils.o: gss-utils.c gss-utils.h Makefile

remctld.o: remctld.c gss-utils.h Makefile

remctl.o: remctl.c gss-utils.h Makefile

clean:
	rm -f *.o remctld remctl
