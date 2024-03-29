/* -*- mode: C; c-basic-offset: 4 -*- */
/* ex: set shiftwidth=4 tabstop=4 expandtab: */
/*
 * Copyright (c) 2013, Georgia Tech Research Corporation
 * All rights reserved.
 *
 * Author(s): Neil T. Dantam <ntd@gatech.edu>
 * Georgia Tech Humanoid Robotics Lab
 * Under Direction of Prof. Mike Stilman <mstilman@cc.gatech.edu>
 *
 *
 * This file is provided under the following "BSD-style" License:
 *
 *
 *   Redistribution and use in source and binary forms, with or
 *   without modification, are permitted provided that the following
 *   conditions are met:
 *
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *
 *   * Redistributions in binary form must reproduce the above
 *     copyright notice, this list of conditions and the following
 *     disclaimer in the documentation and/or other materials provided
 *     with the distribution.
 *
 *   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND
 *   CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES,
 *   INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 *   MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 *   DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR
 *   CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF
 *   USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 *   AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 *   LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 *   ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 *   POSSIBILITY OF SUCH DAMAGE.
 *
 */


/*
 * achcop: Watchdog process for ach-using daemons
 */


/* TODO: don't give up for non-catastrophic errors
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifndef _DEFAULT_SOURCE
#define _DEFAULT_SOURCE
#endif

#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>
#include <inttypes.h>
#include <errno.h>
#include <signal.h>
#include <stdarg.h>
#include <unistd.h>
#include <fcntl.h>
#include <syslog.h>
#include <sys/wait.h>
#include "ach.h"
#include "achutil.h"


/* EVENTS:
 *   Message Timeout
 *   Invalid Message
 *   Process Exits
 */

/* Avoiding signal races:
 *
 * This program needs to wait for SIGTERM and child termination.
 * Waiting for children to terminate with wait() would create a race
 * condition on detection of SIGTERMs.  If SIGTERM is received after
 * we check the flag, but before we enter wait(), we would never
 * realize it.  Instead, we block the signals, and sigwait() to
 * synchronously recieve.
 */

/* File locking note:
 *
 * PID files are locked with lockf().  These locks are released if the
 * holding process exits.  When the parent process sticks around to
 * monitor the child, the parent holds the lock.  When the parent
 * directly exec's to the 'child' (parent and child are really the
 * same process) the exec'ed program inherits the lock.  However, the
 * lock will be lost if the file descriptor is closed.  Therefore,
 * exec'ed programs must not close the lock file descriptor.
 */

/* Process creation.
 *
 *   2-4 processes will be created by this program.
 *   If -d flag is given, the following processes are created:
 *       Initial process (GRANDPARENT)
 *       Detach Parent, child of grandparent (PARENT)
 *       Detach Child (child of parent), continues into run() function (WORKER)
 *   Otherwise, the Initial Process continues to the run() function (WORKER)
 *
 *   Then, another process is forked from within run() (CHILD).  This
 *   processes then execs the child given on the command line.
 */


/* Fork Signal Protocol.
 *   Forked processes report success or failure to the parents via signals
 *      SIGUSR1: success
 *      SIGUSR2: failure
 *      SIGALRM (generated by parent): timeout with assumed success or failure
 *
 *   GRANDPARENT will call alarm() and wait for signals.  If it times
 *   out on SIGALARM, it assumes failure.
 *
 *   WORKER will accept SIGUSR1.  If received it will relay (one time
 *   only) to GRANDPARENT (if one exists), If WORKER instead times out
 *   on sigalarm before the CHILD exits, it assumes CHILD is running
 *   correctly and sends SIGUSR1 to the GRANDPARENT. If the CHILD
 *   exits with failure before the SIGALRM timeout, then WORKER
 *   considers it to have failed, sends SIGUSR2 to GRANDPARENT, and
 *   exits itself with failure.
 */

/*
 * If child returns 0: exit normally
 * If child returns !0: restart it
 * If child terminated by signal: restart it
 * If sigterm received, signal child and wait for child to exit
 */


/* Redirect output to newfd to oldfd */
static void redirect(const char *name, int oldfd, int newfd );

/* Error checking open() */
static int open_file(const char *name, int options );

/* Error checking open(name, O_APPEND) */
static int open_out_file(const char *name );

/* Lock fd */
static void lock_pid( int fd );

/* Write pid to fd */
static void write_pid( int fd, pid_t pid);

/* Add arg to end of args, reallocing */
static void child_arg( const char ***args, const char *arg, size_t *n);

/* Main run loop */
static int run( int restart, int fd_pid, const char *file, const char ** args);

/* Fork and exec a child process, writing its PID to fd_pid */
static pid_t start_child( int fd_pid, const char *file, const char ** args);

/* Error checking on kill(SIGTERM) */
static void terminate_child( pid_t pid );

/* Error checking on wait() */
static void waitloop( pid_t pid_child, int *status );


/** Signals for a running achcop */
static int achcop_signals[] = {
    SIGINT,         /**< terminate child and exit, returing child's status */
    SIGTERM,        /**< terminate child and exit, returing child's status */
    SIGCHLD,        /**< check child status. if success, exit, else, restart child */
    ACH_SIG_OK,     /**< child successfully started, */
    ACH_SIG_FAIL,   /**< child failed to start, */
    SIGALRM,        /**< Child timeout waiting for success */
    SIGHUP,         /**< terminate child and restart */
    0}; /* null terminated array */


/** Filename to indicate no stdout/stderr redirection */
#define FILE_NOP_NAME "-"

/** If child exits with failure before this timeout, give up.
 */
#define ACH_CHILD_TIMEOUT_SEC 1

int main( int argc, char **argv ) {
    /* Command line arguments */
    static struct {
        const char *file_cop_pid;     /**< file name for achcop PID */
        const char *file_child_pid;   /**< file name for child PID */
        const char *file_stderr;      /**< file name for stderr redirect */
        const char *file_stdout;      /**< file name for stdout redirect */
        const char **child_args;      /**< arguments to pass to child */
        const char *log_ident;        /**< identifier for openlog() */
        int log_facility;             /**< facility for openlog() */
        size_t n_child_args;          /**< size of child_args */
        int detach;                   /**< detach (daemonize) flag */
        int restart;                  /**< restart failed processes flag */
    } opt = {0};

    /* Some default arguments */
    opt.log_ident = "achcop";
    opt.log_facility = LOG_USER;

    /* Global state */
    static struct {
        int fd_out;                   /**< file descriptor for stdout redirect */
        int fd_err;                   /**< file descriptor for stderr redirect */
        int fd_cop_pid;               /**< file descriptor for achcop PID file */
        int fd_child_pid;             /**< file descriptor for child PID file */
    } cx = {0};

    /* Parse Options */
    int c;
    opterr = 0;
    while( (c = getopt( argc, argv, "P:p:o:e:rvdhi:f:H?V")) != -1 ) {
        switch(c) {
        case 'P': opt.file_cop_pid = optarg; break;
        case 'p': opt.file_child_pid = optarg; break;
        case 'o': opt.file_stdout = optarg; break;
        case 'e': opt.file_stderr = optarg; break;
        case 'd': opt.detach = 1; break;
        case 'r': opt.restart = 1; break;
        case 'i': opt.log_ident = optarg; break;
        case 'f':
            if( 0 == strcasecmp("user", optarg) ) {
                opt.log_facility = LOG_USER;
            } else if ( 0 == strcasecmp("daemon", optarg) ||
                        0 == strcasecmp("demon", optarg) ) {
                opt.log_facility = LOG_DAEMON;
            } else {
                ACH_DIE("Invalid logging facility: `%s', want user|daemon\n", optarg);
            }
            break;
        case 'V':   /* version     */
            ach_print_version("achcop");
            exit(EXIT_SUCCESS);
        case 'v': ach_verbosity++; break;
        case '?':   /* help     */
        case 'h':
        case 'H':
            puts( "Usage: achcop [OPTIONS...] -- child-name [CHILD-OPTIONS]\n"
                  "Watchdog to run and restart ach child processes\n"
                  "\n"
                  "Options:\n"
                  "  -P pid-file,      File for pid of cop process (only valid with -r)\n"
                  "  -p pid-file,      File for pid of child process\n"
                  "  -o out-file,      Redirect stdout to this file\n"
                  "                      ("FILE_NOP_NAME" keeps unchanged, no option closes)\n"
                  "  -e err-file,      Redirect stderr to this file\n"
                  "                      ("FILE_NOP_NAME" keeps unchanged, no option closes)\n"
                  "  -d,               Detach and run in background\n"
                  "  -r,               Restart failed children\n"
                  "  -i ident          Log identifier (default: achop)\n"
                  "  -f facility       Log facility {user|daemon} (default: user)\n"
                  "  -v,               Make output more verbose\n"
                  "  -?,               Give program help list\n"
                  "  -V,               Print program version\n"
                  "\n"
                  "Examples:\n"
                  "  achcop -rd -P /var/run/myd.ppid -p /var/run/myd.pid -- my-daemon -xyz"
                  "\n"
                  "Report bugs to <ntd@gatech.edu>"
                );
            exit(EXIT_SUCCESS);
        default:
            child_arg(&opt.child_args, optarg, &opt.n_child_args);
        }
    }
    while( optind < argc ) {
        child_arg(&opt.child_args, argv[optind++], &opt.n_child_args);
    }
    /* Check args */
    if( 0 == opt.n_child_args || NULL == opt.child_args ) {
        ACH_DIE("No child process given\n");
    }
    child_arg(&opt.child_args, NULL, &opt.n_child_args); /* Null-terminate child arg array */

    /* open PID files */
    /* do this before the detach(), which may chdir() */
    cx.fd_cop_pid = open_file( opt.file_cop_pid, 0 );
    cx.fd_child_pid = open_file( opt.file_child_pid, 0 );
    cx.fd_out = open_out_file( opt.file_stdout );
    cx.fd_err = open_out_file( opt.file_stderr );

    /* Detach */
    if( opt.detach ) {
        openlog("achcop", LOG_PID, LOG_DAEMON);
        ach_pid_notify = ach_detach( ACH_PARENT_TIMEOUT_SEC );
    }

    /* Lock PID files */
    lock_pid( cx.fd_cop_pid );
    lock_pid( cx.fd_child_pid );

    /* Log that we're starting */
    ACH_LOG( LOG_NOTICE, "achcop running `%s'\n", opt.child_args[0] );

    /* Write my PID */
    write_pid( cx.fd_cop_pid, getpid() );

    /* Set environment variable to indicate achcop is the parent */
    if( setenv("ACHCOP", "1", 1 ) ) {
        ACH_LOG( LOG_ERR, "could not set environment: %s\n", strerror(errno) );
    }

    /* Redirect */
    redirect(opt.file_stdout, cx.fd_out, STDOUT_FILENO );
    redirect(opt.file_stderr, cx.fd_err, STDERR_FILENO );

    /* TODO: fork a second child to monitor an ach channel.  Restart
     * first child if second child signals or exits */

    /* Fork child */
    return run( opt.restart, cx.fd_child_pid, opt.child_args[0], opt.child_args );

    /* Should never get here */
    return EXIT_FAILURE;
}


static void child_arg(const char ***args, const char *arg, size_t *n) {
    *args = (const char **)realloc( *args, (*n+1)*sizeof(arg) );
    (*args)[(*n)++] = arg;
}

static void redirect(const char *name, int oldfd, int newfd ) {
    /* check close */
    if( NULL == name ) {
        if( close(newfd) ) {
            ACH_LOG( LOG_ERR, "Couldn't close file descriptor: %s\n", strerror(errno) );
        }
        return;
    }

    /* check no-op */
    if( 0 == strcmp(name, FILE_NOP_NAME) || -1 == oldfd ) {
        return;
    }

    /* dup */
    if( -1 == dup2(oldfd, newfd) ) {
        ACH_LOG( LOG_ERR, "Could not dup output: %s\n", strerror(errno) );
    }
    if( close(oldfd) ) {
        ACH_LOG( LOG_ERR, "Couldn't close dup'ed file descriptor: %s\n", strerror(errno) );
    }
}

static int open_out_file(const char *name ) {
    if( NULL == name || 0 == strcmp(FILE_NOP_NAME, name) ) {
        return -1;
    }
    return open_file( name, O_APPEND );
}
static int open_file(const char *name, int opts) {
    if( NULL == name ) {
        return -1;
    }
    /* open */
    int fd = open( name, O_RDWR|O_CREAT|opts, 0664 );
    if( fd < 0 ) {
        ACH_DIE( "Could not open file %s: %s\n", name, strerror(errno) );
    }
    return fd;
}

static void lock_pid( int fd ) {
    if( -1 == fd ) return;
    /* lock */
    if( lockf(fd, F_TLOCK, 0) ) {
        ACH_DIE( "Could not lock pid file: %s\n", strerror(errno) );
    }
}

static void write_pid( int fd, pid_t pid ) {
    if( -1 == fd ) return;
    /* truncate */
    if( ftruncate(fd, 0) ) {
        ACH_DIE( "Could truncate pid file: %s\n", strerror(errno) );
    }
    /* seek */
    if( lseek(fd, 0, SEEK_SET) ) {
        ACH_LOG( LOG_ERR, "Could seek pid file: %s\n", strerror(errno) );
    }
    /* format */
    char buf[32] = {0}; /* 64 bits should take 21 decimal digits, including '\0' */
    int size = snprintf( buf, sizeof(buf), "%d", pid );
    if( size < 0 ) {
        ACH_LOG(LOG_ERR, "Error formatting pid\n");
        return;
    } else if ( size >= (int)sizeof(buf)) {
        ACH_LOG(LOG_ERR, "PID buffer overflow for pid=%d\n", pid); /* What, you've got 128 bit PIDs? */
        return;
    }
    /* write */
    size_t n = 0;
    while( n < (size_t)size ) {
        ssize_t r = write( fd, buf+n, (size_t)size-n );
        if( r > 0 ) n += (size_t)r;
        else if (EINTR == errno) continue;
        else {
            ACH_LOG( LOG_ERR, "Could not write pid %d: %s\n", pid, strerror(errno) );
            return;
        }
    } while( n < (size_t)size);
    ACH_LOG( LOG_DEBUG, "Write pid %d\n", pid );
}


/* Now it gets hairy... */
struct flags {
    int first_run;
    int restart;
    int force_restart;
    int start;
    int done;
};

static int run( int restart, int fd_pid, const char *file, const char **args) {
    /* Block Signals */
    /* The signal mask is inherited through fork and exec, so we need
     * to unblock these for the child later */
    ach_sig_block_dummy( achcop_signals );

    ACH_LOG( LOG_DEBUG, "starting run loop as pid %d\n", getpid() );

    int exit_status = EXIT_FAILURE;  /* exit status for this process */

    struct flags flags = { .restart = restart,
                           .force_restart = 0,
                           .first_run = 1,
                           .start = 1,
                           .done = 0
    };

    pid_t pid_child = -1;
    while( ! flags.done ) {

        /* start */
        if( flags.start ) {
            pid_child = start_child( fd_pid, file, args );
            /* If child lasts this long, assume it's working */
            if( flags.first_run )
                alarm( ACH_CHILD_TIMEOUT_SEC );
            flags.start = 0;
            flags.force_restart = 0;
        }

        /* wait for signal */
        int sig = ach_sig_wait( achcop_signals );

        /* handle signal */
        switch(sig) {
        case SIGTERM:
        case SIGINT: /* We got a sigterm/sigint */
            /* Kill Child */
            ACH_LOG(LOG_DEBUG, "Killing child\n");
            flags.restart = 0;
            terminate_child( pid_child );
            break;
        case SIGCHLD: /* child died */
            /* Get child status */
            waitloop(pid_child, &exit_status );
            /* Decide whether to restart the child */
            if( flags.force_restart ) {
                /* Child successful */
                ACH_LOG(LOG_NOTICE, "Child returned, restarting\n" );
                flags.start = 1;
            } else if( EXIT_SUCCESS == exit_status ) {
                /* Child successful */
                ACH_LOG(LOG_NOTICE, "Child `%s' returned success, exiting\n", file);
                flags.done = 1;
            } else if (flags.first_run) { /* child never ran successfully */
                /* Child died too quickly first time, give up */
                ACH_LOG(LOG_ERR, "Child `%s' died too fast, exiting\n", file);
                flags.done = 1;
            } else if (flags.restart) {
                ACH_LOG(LOG_NOTICE, "Restarting child\n");
                flags.start = 1;
            } else {
                ACH_LOG(LOG_NOTICE, "Exiting\n");
                flags.done = 1;
            }
            break;
        case SIGHUP: /* someone told us to restart the child */
            ACH_LOG(LOG_DEBUG, "Restarting child on SIGHUP\n");
            flags.force_restart = 1;
            terminate_child( pid_child );
            break;
        case SIGALRM:
        case ACH_SIG_OK: /* child started ok, keep on waiting */
            ACH_LOG( LOG_INFO, "Child %d started ok\n", pid_child );
            flags.first_run = 0;
            ach_notify( ACH_SIG_OK );
            break;
        default:
            /* continue and sigwait() */
            ACH_LOG( LOG_WARNING, "Spurious signal received: %s (%d)\n",
                     strsignal(sig), sig );
        }
    }

    /* Signal status */
    ach_notify( (EXIT_SUCCESS == exit_status) ? ACH_SIG_OK : ACH_SIG_FAIL );
    ACH_LOG( LOG_DEBUG, "run loop done\n");
    /* Done */
    return exit_status;
}

static pid_t start_child( int fd_pid, const char *file, const char **args) {
    pid_t cpid = fork();

    if( 0 == cpid ) { /* child: exec */
        ACH_LOG( LOG_DEBUG, "In child %d\n", getpid() );
        /* Unblock signals for the child */
        ach_sig_dfl_unblock( achcop_signals );
        execvp( file, (char *const*)args );
        ACH_LOG( LOG_ERR, "Could not exec: %s\n", strerror(errno) );
        exit(EXIT_FAILURE);
        assert(0);
    } else if ( cpid > 0 ) { /* parent: record child */
        ACH_LOG( LOG_DEBUG, "In parent: forked off %d\n", cpid );
        write_pid( fd_pid, cpid );
    } else {
        /* TODO: handle EAGAIN */
        ACH_DIE( "Could not fork child: %s\n", strerror(errno) );
    }
    return cpid;
}

static void waitloop( pid_t pid_child, int *exit_status ) {
    *exit_status = EXIT_FAILURE;

    /* wait */
    pid_t p;
    int status;
    while( ( (p = waitpid( pid_child, &status, WNOHANG)) < 0 &&
            EINTR == errno ) );
    if( p < 0 ) ACH_DIE("Could not wait for child: %s\n", strerror(errno) );

    /* if unexpected pid */
    if( p != pid_child ) {
        ACH_DIE( "Unexpected wait pid: %d\n", p );
    }

    /* check status */
    if( WIFEXITED(status) ) { /* child exited */
        ACH_LOG(LOG_DEBUG, "child exited with %d\n", WEXITSTATUS(status));
        *exit_status = WEXITSTATUS(status);
    } else if ( WIFSIGNALED(status) ) { /* child signalled */
        int sig = WTERMSIG(status);
        ACH_LOG( LOG_NOTICE, "child %d signaled: %s (%d)\n",
                 pid_child, strsignal(sig), sig );
    } else { /* something weird happened */
        ACH_LOG( LOG_ERR, "Unexpexted wait status: %d\n", status );
    }
}


static void terminate_child( pid_t pid ) {
    if( kill(pid, SIGTERM) ) {
        ACH_LOG( LOG_ERR, "Couldn't kill child: %s\n", strerror(errno) );
    }
}
