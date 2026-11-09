/* smallsh.h  -- defs for smallsh command processor */

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>

#define EOL         1 /* end of line */
#define ARG         2 /* normal arguments */
#define AMPERSAND   3
#define SEMICOLON   4

#define MAXARG      512 /* max. no. command args */
#define MAXBUF      512 /* max. length input line */

#define FOREGROUND  1
#define BACKGROUND  2

/* job table */
#define MAXJOBS     64  /* max. number of jobs tracked */
#define CMDLEN      256 /* max. saved command text per job */

/* job states */
#define RUNNING     1
#define STOPPED     2

int userin(char *p);
int gettok(char **outptr);
int inarg(char c);
void procline(void);
int runcommand(char **cline, int where);