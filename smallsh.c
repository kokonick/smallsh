/* include file for example */
#include "smallsh.h"

/* program buffers and work pointers */
static char inpbuf[MAXBUF], tokbuf[2 * MAXBUF],
    *ptr = inpbuf, *tok = tokbuf;

/* ------------------------------------------------------------------ */
/* job table                                                          */
/* Each job is ONE process that is the leader of its own process      */
/* group, so the process id and the process group id are the same.    */
/* ------------------------------------------------------------------ */
struct job {
    int id;             /* job number shown to user, 0 = slot unused */
    pid_t pid;          /* process id (= process group id) */
    int state;          /* RUNNING or STOPPED */
    char cmd[CMDLEN];   /* command text, for display */
};

static struct job jobs[MAXJOBS];
static int interactive;     /* 1 if stdin is a terminal */

/* find a job using job number */
static struct job *findjob(int id)
{
    int i;

    if (id <= 0) {
        return NULL;
    }
        
    for (i = 0; i < MAXJOBS; i++) {
        if (jobs[i].id == id) {
            return &jobs[i];
        }            
    }
        
    return NULL;
}

/* find a jobusing process id */
static struct job *findpid(pid_t pid)
{
    int i;
    for (i = 0; i < MAXJOBS; i++) {
        if (jobs[i].id != 0 && jobs[i].pid == pid) {
            return &jobs[i];
        }            
    }
        
    return NULL;
}

/* add a job to the table, returns NULL if the table is full */
static struct job *addjob(pid_t pid, int state, char **arg)
{
    int i, id;

    for (i = 0; i < MAXJOBS; i++) {
        if (jobs[i].id == 0) {
            /* use the smallest job number not in use */
            id = 1;
            while (findjob(id) != NULL) {
                id++;
            }
                

            jobs[i].id = id;
            jobs[i].pid = pid;
            jobs[i].state = state;

            /* let's build the command text */
            jobs[i].cmd[0] = '\0';
            for (id = 0; arg[id] != NULL; id++) {
                if (id > 0) {
                     strncat(jobs[i].cmd, " ",
                            CMDLEN - strlen(jobs[i].cmd) - 1);
                }
                   
                strncat(jobs[i].cmd, arg[id],
                        CMDLEN - strlen(jobs[i].cmd) - 1);
            }
            return &jobs[i];
        }
    }
    return NULL;
}

static void deletejob(struct job *j)
{
    j->id = 0;
}

/* turn "%2" or "2" into a job; no argument means the most recent job */
static struct job *getjob(char *spec)
{
    int i;
    struct job *j = NULL;

    if (spec == NULL) {
        for (i = 0; i < MAXJOBS; i++) {
            if (jobs[i].id != 0 && (j == NULL || jobs[i].id > j->id)) {
                j = &jobs[i];
            }                
        }
            
        if (j == NULL) {
            printf("smallsh: no current job\n");
        }
            
        return j;
    }

    if (spec[0] == '%') {
        spec++;
    }
        

    /* must be all digits */
    if (*spec == '\0') {
        printf("smallsh: bad job number\n");
        return NULL;
    }

    for (i = 0; spec[i] != '\0'; i++) {
        if (spec[i] < '0' || spec[i] > '9') {
            printf("smallsh: bad job number\n");
            return NULL;
        }
    }

    j = findjob(atoi(spec));
    if (j == NULL) {
        printf("smallsh: no such job: %s\n", spec);
    }
        
    return j;
}

/* ------------------------------------------------------------------ */
/* input and tokenizing (unchanged)                                   */
/* ------------------------------------------------------------------ */

/*print prompt and read a line */
int userin(char *p)
{
    int c, count;

    /*Initialization for later routines */
    ptr = inpbuf;
    tok = tokbuf;

    /*display prompt.*/
    printf("%s", p);
    fflush(stdout);
    count = 0;

    while (1)
    {

        if ((c = getchar()) == EOF)
            return (EOF);

        if (count < MAXBUF)
            inpbuf[count++] = c;

        if (c == '\n' && count < MAXBUF)
        {
            inpbuf[count] = '\0';
            return count;
        }

        /* if line too long restart */
        if (c == '\n')
        {
            printf("smallsh: input line too long\n");
            count = 0;
            printf("%s", p);
            fflush(stdout);
        }
    }
}

static char special [] = {' ', '\t', '&', ';', '\n', '\0'};
int inarg(char c) {
    char *wrk;

    for(wrk = special; *wrk; wrk++) {
        if(c == *wrk)
            return (0);
    }

    return (1);
}

/* get token, place into tokbuf*/
int gettok (char **outptr) {
    int type;

    /*set the output string to tok */
    *outptr = tok;
    /* strip white space from the buffer containing the tokens*/
    while (*ptr == ' ' || *ptr == '\t')
        ptr++;

    /* set the token pointer to the first token in the buffer*/
    *tok++ = *ptr;
    switch(*ptr++) {
        case '\n' :
            type = EOL;
            break;
        case '&' :
            type = AMPERSAND;
            break;
        case ';':
            type = SEMICOLON;
            break;
        default:
            type = ARG;
            /* keep reading valid ordinary characters */
            while(inarg(*ptr))
                *tok++ = *ptr++;
    }

    *tok++ = '\0';
    return type;
}

/* ------------------------------------------------------------------ */
/* job control                                                        */
/* ------------------------------------------------------------------ */

/*
 * Wait for a job that is in the foreground.
 * The job gets the terminal, so Ctrl+C / Ctrl+Z go to it and not to us.
 * We return when it exits, is killed, or is stopped (Ctrl+Z).
 * Afterwards the shell takes the terminal back.
 */
static void waitfg(struct job *j)
{
    int status;
    pid_t pid = j->pid;

    if (interactive)
        tcsetpgrp(STDIN_FILENO, pid);

    /* a stopped job has to be woken up first */
    if (j->state == STOPPED)
        kill(-pid, SIGCONT);
    j->state = RUNNING;

    if (waitpid(pid, &status, WUNTRACED) == -1) {
        perror("smallsh");
        deletejob(j);
    } else if (WIFSTOPPED(status)) {
        j->state = STOPPED;
        printf("\n[%d] Stopped    %s\n", j->id, j->cmd);
    } else {
        /* the terminal echoes ^C but no newline */
        if (WIFSIGNALED(status) && WTERMSIG(status) == SIGINT)
            printf("\n");
        deletejob(j);
    }

    if (interactive)
        tcsetpgrp(STDIN_FILENO, getpgrp());
}

/*
 * checkjobs collects status changes of background jobs without blocking. 
 */
static void checkjobs(void)
{
    int status;
    pid_t pid;
    struct job *j;

    while ((pid = waitpid(-1, &status,
                          WNOHANG | WUNTRACED | WCONTINUED)) > 0) {
        j = findpid(pid);
        if (j == NULL)
            continue;

        if (WIFSTOPPED(status)) {
            j->state = STOPPED;
            printf("[%d] Stopped    %s\n", j->id, j->cmd);
        } else if (WIFCONTINUED(status)) {
            j->state = RUNNING;
        } else if (WIFSIGNALED(status)) {
            printf("[%d] Terminated %s\n", j->id, j->cmd);
            deletejob(j);
        } else {
            printf("[%d] Done       %s\n", j->id, j->cmd);
            deletejob(j);
        }
    }
}

/* send SIGHUP to every job when the shell exits */
static void cleanup(void)
{
    int i;

    for (i = 0; i < MAXJOBS; i++) {
        if (jobs[i].id != 0) {
            kill(-jobs[i].pid, SIGHUP);
            kill(-jobs[i].pid, SIGCONT);
        }
    }
}

/* ------------------------------------------------------------------ */
/* built-in commands                                                  */
/* ------------------------------------------------------------------ */

static void do_jobs(void)
{
    int i;

    checkjobs();
    for (i = 0; i < MAXJOBS; i++) {
        if (jobs[i].id != 0)
            printf("[%d] %d %-10s %s\n", jobs[i].id, (int) jobs[i].pid,
                   jobs[i].state == STOPPED ? "Stopped" : "Running",
                   jobs[i].cmd);
    }
}

static void do_fg(char **arg)
{
    struct job *j = getjob(arg[1]);

    if (j == NULL)
        return;
    printf("%s\n", j->cmd);
    waitfg(j);
}

static void do_bg(char **arg)
{
    struct job *j = getjob(arg[1]);

    if (j == NULL)
        return;
    if (j->state != STOPPED) {
        printf("smallsh: job %d is already running\n", j->id);
        return;
    }
    if (kill(-j->pid, SIGCONT) == -1) {
        perror("smallsh");
        return;
    }
    j->state = RUNNING;
    printf("[%d] %s &\n", j->id, j->cmd);
}

/* kill [-signum] %job   or   kill [-signum] pid */
static void do_kill(char **arg)
{
    int sig = SIGTERM;
    int i = 1;
    struct job *j;

    if (arg[i] != NULL && arg[i][0] == '-') {
        sig = atoi(arg[i] + 1);
        if (sig <= 0) {
            printf("usage: kill [-signum] %%job | pid\n");
            return;
        }
        i++;
    }
    if (arg[i] == NULL) {
        printf("usage: kill [-signum] %%job | pid\n");
        return;
    }

    if (arg[i][0] == '%') {
        j = getjob(arg[i]);
        if (j == NULL)
            return;
        if (kill(-j->pid, sig) == -1) {
            perror("smallsh");
            return;
        }
        /* a stopped job only acts on these once it is continued */
        if (j->state == STOPPED &&
            (sig == SIGTERM || sig == SIGHUP || sig == SIGINT))
            kill(-j->pid, SIGCONT);
    } else {
        if (kill((pid_t) atoi(arg[i]), sig) == -1)
            perror("smallsh");
    }
}

/* returns 1 if arg[0] was a built-in (and has been run), else 0 */
static int builtin(char **arg)
{
    if (strcmp(arg[0], "jobs") == 0) {
        do_jobs();
        return 1;
    }
    if (strcmp(arg[0], "fg") == 0) {
        do_fg(arg);
        return 1;
    }
    if (strcmp(arg[0], "bg") == 0) {
        do_bg(arg);
        return 1;
    }
    if (strcmp(arg[0], "kill") == 0) {
        do_kill(arg);
        return 1;
    }
    if (strcmp(arg[0], "exit") == 0) {
        cleanup();
        exit(0);
    }
    return 0;
}

/* ------------------------------------------------------------------ */
/* running commands                                                   */
/* ------------------------------------------------------------------ */

/* execute a command, in the foreground or background */
int runcommand(char **cline, int where) {
    pid_t pid;
    struct job *j;

    fflush(stdout);     /* don't let the child copy unflushed output */

    pid = fork();
    if (pid == -1) {
        perror("smallsh");
        return (-1);
    }

    if (pid == 0) {
        /* child: put myself in a new process group */
        setpgid(0, 0);

        /* a foreground job takes the terminal */
        if (where == FOREGROUND && interactive)
            tcsetpgrp(STDIN_FILENO, getpid());

        /* undo the signal settings that the shell uses */
        signal(SIGINT, SIG_DFL);
        signal(SIGQUIT, SIG_DFL);
        signal(SIGTSTP, SIG_DFL);
        signal(SIGTTIN, SIG_DFL);
        signal(SIGTTOU, SIG_DFL);

        execvp(*cline, cline);
        perror(*cline);
        exit(1);
    }

    /* parent: do the same setpgid, whichever process runs first wins */
    setpgid(pid, pid);

    j = addjob(pid, RUNNING, cline);
    if (j == NULL) {
        printf("smallsh: too many jobs\n");
        kill(pid, SIGKILL);
        waitpid(pid, NULL, 0);
        return (-1);
    }

    if (where == BACKGROUND) {
        printf("[%d] %d\n", j->id, (int) pid);
        return (0);
    }

    waitfg(j);
    return (0);
}

void procline(void) { /* process input line*/
    char *arg[MAXARG +1];  /*pointer array for runcommand */
    int toktype;           /* type of token in command */
    int narg;              /* number of arguments so far */
    int type;              /* FOREGROUND or BACKGROUND*/

    narg = 0;

    for(;;) /* loop forever*/
    {
        /*take action according to token type*/
        switch(toktype = gettok(&arg[narg])) {
            case ARG:
                if(narg < MAXARG)
                    narg++;
                break;
            case EOL:
            case SEMICOLON:
            case AMPERSAND:
                if(toktype == AMPERSAND)
                    type = BACKGROUND;
                else
                    type = FOREGROUND;

            if(narg != 0) {
                arg[narg]  = NULL;
                if (!builtin(arg))
                    runcommand(arg, type);
            }

            if(toktype == EOL)
                return;

            narg = 0;
            break;
        }
    }

}

/* smallsh  -- simple command processor with job control */

char *prompt = "Command> ";     /* prompt */

int main(void) {

    interactive = isatty(STDIN_FILENO);

    /* the shell must not be stopped or killed by terminal signals */
    signal(SIGINT, SIG_IGN);
    signal(SIGQUIT, SIG_IGN);
    signal(SIGTSTP, SIG_IGN);
    signal(SIGTTIN, SIG_IGN);
    signal(SIGTTOU, SIG_IGN);

    /* our program  gets its own process group and own the terminal */
    if (interactive) {
        setpgid(0, 0);
        tcsetpgrp(STDIN_FILENO, getpgrp());
    }

    for (;;) {
        /* report/reap background jobs that finished or stopped */
        checkjobs();

        if (userin(prompt) == EOF)
            break;

        procline();
    }

    cleanup();
    return 0;
}