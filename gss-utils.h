/*

   The include file for a "K5 sysctl" - a service for remote execution of 
   predefined commands. Access is authenticated via GSSAPI Kerberos 5, 
   authorized via aclfiles. 

   Written by Anton Ushakov <antonu@stanford.edu>
   Copyright 2002 Board of Trustees, Leland Stanford Jr. University

*/

#ifndef _GSSMISC_H_
#define _GSSMISC_H_

#include <gssapi/gssapi_generic.h>
#include <stdio.h>
#include <stdarg.h>

extern FILE *log;

void* smalloc(int);
void lowercase(char string[]);

int gss_sendmsg
        PROTOTYPE( (gss_ctx_id_t context, int flags, char* msg, int msglength) );

int gss_recvmsg
        PROTOTYPE( (gss_ctx_id_t context, int* token_flags, char** msg, int* msglength) );

int send_token
	PROTOTYPE( (int flags, gss_buffer_t tok) );
int recv_token
	PROTOTYPE( (int *flags, gss_buffer_t tok) );
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
#define TOKEN_ERROR		(1<<3)
#define TOKEN_MIC		(1<<4)

/* Token flags */
#define TOKEN_CONTEXT_NEXT	(1<<5)
#define TOKEN_SEND_MIC		(1<<6)

#define MAXCMDLINE              64000

extern gss_buffer_t empty_token;

#endif
