/*

   The include file for a "K5 sysctl" - a service for remote execution of 
   predefined commands. Access is authenticated via GSSAPI Kerberos 5, 
   authorized via aclfiles. 

   Written by Anton Ushakov <antonu@stanford.edu>
   Copyright 2002 Board of Trustees, Leland Stanford Jr. University

*/

#ifndef _GSSUTILS_H_
#define _GSSUTILS_H_

#include <gssapi/gssapi_generic.h>
#include <stdio.h>
#include <stdarg.h>

void* smalloc(int);
void* srealloc(void* ptr, size_t size);
void* sstrdup(const char* s1);
void lowercase(char string[]);

int gss_sendmsg
        PROTOTYPE( (gss_ctx_id_t context, int flags, char* msg, OM_uint32 msglength) );

int gss_recvmsg
        PROTOTYPE( (gss_ctx_id_t context, int* token_flags, char** msg, OM_uint32* msglength) );

int send_token
	PROTOTYPE( (int flags, gss_buffer_t tok) );
int recv_token
	PROTOTYPE( (int *flags, gss_buffer_t tok) );

int write_all
	PROTOTYPE( (unsigned short writefd, char *buf, unsigned int nbyte) );
int read_all
	PROTOTYPE( (unsigned short readfd, char *buf, unsigned int nbyte) );
int read_two
	PROTOTYPE( (unsigned short readfd1, unsigned short readfd2, char *buf1, char *buf2, unsigned int nbyte1, unsigned int nbyte2) );

void display_status
	PROTOTYPE( (char *msg, OM_uint32 maj_stat, OM_uint32 min_stat) );
void display_ctx_flags
	PROTOTYPE( (OM_uint32 flags) );
void print_token
	PROTOTYPE( (gss_buffer_t tok) );

/* Token types */
#define TOKEN_NOOP		(1<<0)
#define TOKEN_CONTEXT		(1<<1)
#define TOKEN_DATA		(1<<2)
#define TOKEN_MIC		(1<<3)

/* Token flags */
#define TOKEN_CONTEXT_NEXT	(1<<4)
#define TOKEN_SEND_MIC		(1<<5)

/* Used as the default max buffer for the argv passed into the server, and for 
   the return message from the server. */
#define MAXBUFFER               64000  

/* The maximum size of argc passed to the server */
#define MAXCMDARGS              64

/* Maximum encrypted token size. I notices it was roughly 6 - 7 times the clear
   text, so on the safe size, it's 10 * MAXBUFFER */
#define MAXENCRYPT              640000

/* Number of environment variables passed to the exec'ed process */
#define MAXENV                  256

#endif
