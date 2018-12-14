/* -*- mode: C; c-basic-offset: 4 -*- */
/* ex: set shiftwidth=4 tabstop=4 expandtab: */
/*
 * Copyright (c) 2008-2013, Georgia Tech Research Corporation
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


/** \file ach.c
 *  \author Neil T. Dantam
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifndef _DEFAULT_SOURCE
#define _DEFAULT_SOURCE
#endif

#include <time.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <pthread.h>
#include <unistd.h>
#include <ctype.h>
#include <stdbool.h>
#include <sys/stat.h>

#include <string.h>
#include <inttypes.h>

#include "ach.h"


#ifdef NDEBUG

#define IFDEBUG(test, x )
#define DEBUGF( ... )
#define DEBUG_PERROR(a)

#else /* enable debugging */

#define IFDEBUG( test, x ) if(test) { (x); }
#define DEBUGF( ... ) fprintf(stderr, __VA_ARGS__ )
#define DEBUG_PERROR(a) perror(a)

#endif /* NDEBUG */

size_t ach_channel_size = sizeof(ach_channel_t);
size_t ach_attr_size = sizeof(ach_attr_t);



static size_t oldest_index_i( ach_header_t *shm ) {
    return (shm->index_head + shm->index_free)%shm->index_cnt;
}

static size_t last_index_i( ach_header_t *shm ) {
    return (shm->index_head + shm->index_cnt -1)%shm->index_cnt;
}

const char *ach_result_to_string(ach_status_t result) {

    switch(result) {
    case ACH_OK: return "ACH_OK";
    case ACH_OVERFLOW: return "ACH_OVERFLOW";
    case ACH_INVALID_NAME: return "ACH_INVALID_NAME";
    case ACH_BAD_SHM_FILE: return "ACH_BAD_SHM_FILE";
    case ACH_FAILED_SYSCALL: return "ACH_FAILED_SYSCALL";
    case ACH_STALE_FRAMES: return "ACH_STALE_FRAMES";
    case ACH_MISSED_FRAME: return "ACH_MISSED_FRAME";
    case ACH_TIMEOUT: return "ACH_TIMEOUT";
    case ACH_CLOSED: return "ACH_CLOSED";
    case ACH_EEXIST: return "ACH_EEXIST";
    case ACH_ENOENT: return "ACH_ENOENT";
    case ACH_BUG: return "ACH_BUG";
    case ACH_EINVAL: return "ACH_EINVAL";
    case ACH_CORRUPT: return "ACH_CORRUPT";
    case ACH_BAD_HEADER: return "ACH_BAD_HEADER";
    case ACH_EACCES: return "ACH_EACCES";
    case ACH_CANCELED: return "ACH_CANCELED";
    }
    return "UNKNOWN";

}

static enum ach_status
check_errno() {
    switch(errno) {
    case EEXIST: return ACH_EEXIST;
    case ENOENT: return ACH_ENOENT;
    case EACCES: return ACH_EACCES;
    default: return ACH_FAILED_SYSCALL;
    }
}

static enum ach_status
check_guards( ach_header_t *shm ) {
    if( ACH_SHM_MAGIC_NUM != shm->magic ||
        ACH_SHM_GUARD_HEADER_NUM != *ACH_SHM_GUARD_HEADER(shm) ||
        ACH_SHM_GUARD_INDEX_NUM != *ACH_SHM_GUARD_INDEX(shm) ||
        ACH_SHM_GUARD_DATA_NUM != *ACH_SHM_GUARD_DATA(shm)  )
    {
        return ACH_CORRUPT;
    } else {
        return ACH_OK;
    }
}


/* returns 0 if channel name is bad */
static int channel_name_ok( const char *name ) {
    size_t len;
    /* check size */
    if( (len = strlen( name )) >= ACH_CHAN_NAME_MAX )
        return 0;
    /* check hidden file */
    if( name[0] == '.' ) return 0;
    /* check bad characters */
    size_t i;
    for( i = 0; i < len; i ++ ) {
        if( ! ( isalnum( name[i] )
                || (name[i] == '-' )
                || (name[i] == '_' )
                || (name[i] == '.' ) ) )
            return 0;
    }
    return 1;
}

static enum ach_status
shmfile_for_channel_name( const char *name, char *buf, size_t n ) {
    if( n < ACH_CHAN_NAME_MAX + 16 ) return ACH_BUG;
    if( !channel_name_ok(name)   ) return ACH_INVALID_NAME;
    strcpy( buf, ACH_CHAN_NAME_PREFIX );
    strncat( buf, name, ACH_CHAN_NAME_MAX );
    return ACH_OK;
}

/** Opens shm file descriptor for a channel.
    \pre name is a valid channel name
*/
static int fd_for_channel_name( const char *name, int oflag ) {
    char shm_name[ACH_CHAN_NAME_MAX + 16];
    int r = shmfile_for_channel_name( name, shm_name, sizeof(shm_name) );
    if( 0 != r ) return ACH_BUG;
    int fd;
    int i = 0;
    do {
        fd = shm_open( shm_name, O_RDWR | oflag, 0666 );
    }while( -1 == fd && EINTR == errno && i++ < ACH_INTR_RETRY);
    return fd;
}



/*! \page synchronization Synchronization
 *
 * Synchronization currently uses a simple mutex+condition variable
 * around the whole shared memory block
 *
 * Some idea for more complicated synchronization:
 *
 * Our synchronization for shared memory could work roughly like a
 * read-write lock with one one additional feature.  A reader may
 * choose to block until the next write is performed.
 *
 * This behavior could be implemented with a a state variable, a
 * mutex, two condition variables, and three counters.  One condition
 * variable is for writers, and the other for readers.  We count
 * active readers, waiting readers, and waiting writers.  If a writer
 * is waiting, readers will block until it finishes.
 *
 * It may be possible to make these locks run faster by doing some
 * CASs on the state word to handle the uncontended case.  Of course,
 * figuring out how to make this all lock free would really be
 * ideal...
 *
 *  \bug synchronization should be robust against processes terminating
 *
 * Mostly Lock Free Synchronization:
 * - Have a single word atomic sync variable
 * - High order bits are counts of writers, lower bits are counts of readers
 * - Fast path twiddles the counts.  Slow path deals with a mutex and cond-var.
 * - downside: maybe no way for priority inheritance to happen...
 *
 * Other Fancy things:
 * - Use futexes for waiting readers/writers
 * - Use eventfd to signal new data
 */

static enum ach_status
check_lock( int lock_result, ach_channel_t *chan, int is_cond_check ) {
    switch( lock_result ) {
    case ETIMEDOUT:
        if( is_cond_check ) {
            /* release mutex if cond_wait times out */
            pthread_mutex_unlock( &chan->shm->sync.mutex );
        }
        return ACH_TIMEOUT;
    case ENOTRECOVERABLE:
        /* Shouldn't actually get this because we always mark the
         * mutex as consistent. */
        /* We do not hold the mutex at this point */
        return ACH_CORRUPT;
    case EOWNERDEAD: /* mutex holder died */
        /* We use the dirty bit to detect unrecoverable (for now)
         * errors.  Unconditionally mark the mutex as consistent.
         * Others will detect corruption based on the dirty bit.
         */
        pthread_mutex_consistent( &chan->shm->sync.mutex );
        /* continue to check the dirty bit */
    case 0: /* ok */
        if( chan->shm->sync.dirty ) {
            pthread_mutex_unlock( &chan->shm->sync.mutex );
            return ACH_CORRUPT;
        } else return ACH_OK; /* it's ok, channel is consistent */
    default:
        if( is_cond_check ) {
            /* release mutex if cond_wait fails */
            pthread_mutex_unlock( &chan->shm->sync.mutex );
        }
        return ACH_FAILED_SYSCALL;
    }

    return ACH_BUG;
}

static enum ach_status
chan_lock( ach_channel_t *chan ) {
    int i = pthread_mutex_lock( & chan->shm->sync.mutex );
    return check_lock( i, chan, 0 );
}

static enum ach_status rdlock(ach_channel_t *chan, int wait,
                              const struct timespec *abstime) {
  ach_header_t *shm = chan->shm;
  {
    enum ach_status r = chan_lock(chan);
    if (ACH_OK != r) return r;
  }
  enum ach_status r = ACH_BUG;

  while (ACH_BUG == r) {
    if (chan->cancel) { /* check operation cancelled */
      pthread_mutex_unlock(&shm->sync.mutex);
      r = ACH_CANCELED;
    } else if (!wait)
      r = ACH_OK; /* check no wait */
    else if (chan->seq_num != shm->last_seq)
      r = ACH_OK; /* check if got a frame */
    /* else condition wait */
    else {
      int i = abstime ? pthread_cond_timedwait(&shm->sync.cond,
                                               &shm->sync.mutex, abstime)
                      : pthread_cond_wait(&shm->sync.cond, &shm->sync.mutex);
      enum ach_status c = check_lock(i, chan, 1);
      if (ACH_OK != c) r = c;
      /* check r and condition next iteration */
    }
  }

  return r;
}

static enum ach_status rdlock_loud(ach_channel_t *chan, int wait,
                                   const struct timespec *abstime) {
  fprintf(stdout, "\n6.1 ");
  fflush(stdout);
  ach_header_t *shm = chan->shm;
  {
    fprintf(stdout, "6.2 ");
    fflush(stdout);
    enum ach_status r = chan_lock(chan);
    fprintf(stdout, "6.3 ");
    fflush(stdout);
    if (ACH_OK != r) return r;
  }
  fprintf(stdout, "6.4 ");
  fflush(stdout);
  enum ach_status r = ACH_BUG;

  while (ACH_BUG == r) {
    if (chan->cancel) { /* check operation cancelled */
      fprintf(stdout, "6.401 ");
      fflush(stdout);
      pthread_mutex_unlock(&shm->sync.mutex);
      fprintf(stdout, "6.402 ");
      fflush(stdout);
      r = ACH_CANCELED;
    } else if (!wait) {
      fprintf(stdout, "6.411 ");
      fflush(stdout);
      r = ACH_OK; /* check no wait */
    } else if (chan->seq_num != shm->last_seq) {
      fprintf(stdout, "6.421 ");
      fflush(stdout);
      r = ACH_OK; /* check if got a frame */
      /* else condition wait */
    } else {
      fprintf(stdout, "6.431 ");
      fflush(stdout);
      int i;
      if (abstime) {
        fprintf(stdout, "6.43101 ");
        fflush(stdout);
        struct timespec curr;
        clock_gettime(CLOCK_MONOTONIC, &curr);
        fprintf(stdout, "(curr: %lds:%ldns, abs: %lds:%ldns) ", curr.tv_sec,
                curr.tv_nsec, abstime->tv_sec, abstime->tv_nsec);
        fflush(stdout);
        i = pthread_cond_timedwait(&shm->sync.cond, &shm->sync.mutex, abstime);
      } else {
        fprintf(stdout, "6.43111 ");
        fflush(stdout);
        i = pthread_cond_wait(&shm->sync.cond, &shm->sync.mutex);
      }
      fprintf(stdout, "6.432 ");
      fflush(stdout);
      enum ach_status c = check_lock(i, chan, 1);
      fprintf(stdout, "6.433 ");
      fflush(stdout);
      if (ACH_OK != c) r = c;
      /* check r and condition next iteration */
    }
  }

  fprintf(stdout, "6.5 ");
  fflush(stdout);
  return r;
}

static enum ach_status unrdlock( ach_header_t *shm ) {
    assert( 0 == shm->sync.dirty );
    if ( pthread_mutex_unlock( & shm->sync.mutex ) )
        return ACH_FAILED_SYSCALL;
    else return ACH_OK;
}

static enum ach_status wrlock( ach_channel_t *chan ) {

    enum ach_status r = chan_lock(chan);
    if( ACH_OK != r ) return r;

    assert( 0 == chan->shm->sync.dirty );

    chan->shm->sync.dirty = 1;

    return r;
}

static ach_status_t unwrlock( ach_header_t *shm ) {
    /* mark clean */
    assert( 1 == shm->sync.dirty );
    shm->sync.dirty = 0;

    /* unlock */
    if( pthread_mutex_unlock( & shm->sync.mutex ) )
        return ACH_FAILED_SYSCALL;

    /* broadcast to wake up waiting readers */
    if( pthread_cond_broadcast( & shm->sync.cond ) )
        return ACH_FAILED_SYSCALL;

    return ACH_OK;
}

static ach_status_t unwrlock_loud( ach_header_t *shm ) {
    /* mark clean */
    fprintf(stdout, "\n19.1 "); fflush(stdout);
    assert( 1 == shm->sync.dirty );
    shm->sync.dirty = 0;

    /* unlock */
    fprintf(stdout, "19.2 "); fflush(stdout);
    int r = pthread_mutex_unlock( & shm->sync.mutex );
    if( r ) {
        fprintf(stdout, "19.3 "); fflush(stdout);
        return ACH_FAILED_SYSCALL;
    }

    /* broadcast to wake up waiting readers */
    fprintf(stdout, "19.4 "); fflush(stdout);
    r = pthread_cond_broadcast( & shm->sync.cond );
    if( r ) {
        fprintf(stdout, "19.5 "); fflush(stdout);
        return ACH_FAILED_SYSCALL;
    }

    fprintf(stdout, "19.6 "); fflush(stdout);
    return ACH_OK;
}


void ach_create_attr_init( ach_create_attr_t *attr ) {
    memset( attr, 0, sizeof( ach_create_attr_t ) );
}

enum ach_status
ach_create( const char *channel_name,
            size_t frame_cnt, size_t frame_size,
            ach_create_attr_t *attr) {
    ach_header_t *shm;
    int fd;
    size_t len;
    /* fixme: truncate */
    /* open shm */
    {
        len = sizeof( ach_header_t) +
            frame_cnt*sizeof( ach_index_t ) +
            frame_cnt*frame_size +
            3*sizeof(uint64_t);

        if( attr && attr->map_anon ) {
            /* anonymous (heap) */
            shm = (ach_header_t *) malloc( len );
            fd = -1;
        }else {
            int oflag = O_EXCL | O_CREAT;
            /* shm */
            if( ! channel_name_ok( channel_name ) )
                return ACH_INVALID_NAME;
            if( attr ) {
                if( attr->truncate ) oflag &= ~O_EXCL;
            }
            if( (fd = fd_for_channel_name( channel_name, oflag )) < 0 ) {
                return check_errno();;
            }

            { /* make file proper size */
                /* FreeBSD needs ftruncate before mmap, Linux can do either order */
                int r;
                int i = 0;
                do {
                    r = ftruncate( fd, (off_t) len );
                }while(-1 == r && EINTR == errno && i++ < ACH_INTR_RETRY);
                if( -1 == r ) {
                    DEBUG_PERROR( "ftruncate");
                    return ACH_FAILED_SYSCALL;
                }
            }

            /* mmap */
            if( (shm = (ach_header_t *)mmap( NULL, len, PROT_READ|PROT_WRITE,
                                             MAP_SHARED, fd, 0) )
                == MAP_FAILED ) {
                DEBUG_PERROR("mmap");
                DEBUGF("mmap failed %s, len: %"PRIuPTR", fd: %d\n", strerror(errno), len, fd);
                return ACH_FAILED_SYSCALL;
            }

        }

        memset( shm, 0, len );
        shm->len = len;
    }

    { /* initialize synchronization */
        { /* initialize condition variables */
            int r;
            pthread_condattr_t cond_attr;
            if( (r = pthread_condattr_init(&cond_attr)) ) {
                DEBUG_PERROR("pthread_condattr_init");
                return ACH_FAILED_SYSCALL;
            }
            /* Process Shared */
            if( ! (attr && attr->map_anon) ) {
                /* Set shared if not anonymous mapping
                   Default will be private. */
                if( (r = pthread_condattr_setpshared(&cond_attr, PTHREAD_PROCESS_SHARED)) ) {
                    DEBUG_PERROR("pthread_condattr_setpshared");
                    return ACH_FAILED_SYSCALL;
                }
            }
            /* Clock */
            if( attr && attr->set_clock ) {
                if( (r = pthread_condattr_setclock(&cond_attr, attr->clock)) ) {
                    DEBUG_PERROR("pthread_condattr_setclock");
                    return ACH_FAILED_SYSCALL;
                }
            } else {
                if( (r = pthread_condattr_setclock(&cond_attr, ACH_DEFAULT_CLOCK)) ) {
                    DEBUG_PERROR("pthread_condattr_setclock");
                    return ACH_FAILED_SYSCALL;
                }
            }

            if( (r = pthread_cond_init(&shm->sync.cond, &cond_attr)) ) {
                DEBUG_PERROR("pthread_cond_init");
                return ACH_FAILED_SYSCALL;
            }

            if( (r = pthread_condattr_destroy(&cond_attr)) ) {
                DEBUG_PERROR("pthread_condattr_destroy");
                return ACH_FAILED_SYSCALL;
            }
        }
        { /* initialize mutex */
            int r;
            pthread_mutexattr_t mutex_attr;
            if( (r = pthread_mutexattr_init(&mutex_attr)) ) {
                DEBUG_PERROR("pthread_mutexattr_init");
                return ACH_FAILED_SYSCALL;
            }
            if( (r = pthread_mutexattr_setpshared(&mutex_attr,
                                                  PTHREAD_PROCESS_SHARED)) ) {
                DEBUG_PERROR("pthread_mutexattr_setpshared");
                return ACH_FAILED_SYSCALL;
            }
#ifdef HAVE_MUTEX_PRIORITY_INHERIT
            /* Priority Inheritance Mutex */
            if( (r = pthread_mutexattr_setprotocol(&mutex_attr,
                                                   PTHREAD_PRIO_INHERIT)) ) {
                DEBUG_PERROR("pthread_mutexattr_setprotocol");
                return ACH_FAILED_SYSCALL;
            }
#endif
#ifdef HAVE_MUTEX_ROBUST
            /* Robust Mutex */
            if( (r = pthread_mutexattr_setrobust(&mutex_attr,
                                                 PTHREAD_MUTEX_ROBUST)) ) {
                DEBUG_PERROR("pthread_mutexattr_setrobust");
                return ACH_FAILED_SYSCALL;
            }
#endif
#ifndef NDEBUG
#ifdef HAVE_MUTEX_ERROR_CHECK
            /* Error Checking Mutex */
            if( (r = pthread_mutexattr_settype(&mutex_attr,
                                               PTHREAD_MUTEX_ERRORCHECK)) ) {
                DEBUG_PERROR("pthread_mutexattr_settype");
                return ACH_FAILED_SYSCALL;
            }
#endif /* HAVE_MUTEX_ERROR_CHECK */
#endif /* NDEBUG */
            if( (r = pthread_mutex_init(&shm->sync.mutex, &mutex_attr)) ) {
                DEBUG_PERROR("pthread_mutexattr_init");
                return ACH_FAILED_SYSCALL;
            }

            if( (r = pthread_mutexattr_destroy(&mutex_attr)) ) {
                DEBUG_PERROR("pthread_mutexattr_destroy");
                return ACH_FAILED_SYSCALL;
            }
        }
    }
    /* initialize name */
    strncpy( shm->name, channel_name, ACH_CHAN_NAME_MAX );
    /* initialize counts */
    shm->index_cnt = frame_cnt;
    shm->index_head = 0;
    shm->index_free = frame_cnt;
    shm->data_head = 0;
    shm->data_free = frame_cnt * frame_size;
    shm->data_size = frame_cnt * frame_size;
    assert( sizeof( ach_header_t ) +
            shm->index_free * sizeof( ach_index_t ) +
            shm->data_free + 3*sizeof(uint64_t) ==  len );

    *ACH_SHM_GUARD_HEADER(shm) = ACH_SHM_GUARD_HEADER_NUM;
    *ACH_SHM_GUARD_INDEX(shm) = ACH_SHM_GUARD_INDEX_NUM;
    *ACH_SHM_GUARD_DATA(shm) = ACH_SHM_GUARD_DATA_NUM;
    shm->magic = ACH_SHM_MAGIC_NUM;

    if( attr && attr->map_anon ) {
        attr->shm = shm;
    } else {
        int r;
        /* remove mapping */
        r = munmap(shm, len);
        if( 0 != r ){
            DEBUG_PERROR("munmap");
            return ACH_FAILED_SYSCALL;
        }
        /* close file */
        int i = 0;
        do {
            IFDEBUG( i, DEBUGF("Retrying close()\n") )
            r = close(fd);
        }while( -1 == r && EINTR == errno && i++ < ACH_INTR_RETRY );
        if( -1 == r ){
            DEBUG_PERROR("close");
            return ACH_FAILED_SYSCALL;
        }
    }
    return ACH_OK;
}

enum ach_status
ach_open( ach_channel_t *chan, const char *channel_name,
          ach_attr_t *attr ) {
    ach_header_t * shm;
    size_t len;
    int fd = -1;

    if( attr ) memcpy( &chan->attr, attr, sizeof(chan->attr) );
    else memset( &chan->attr, 0, sizeof(chan->attr) );

    if( attr && attr->map_anon ) {
        shm = attr->shm;
        len = sizeof(ach_header_t) + sizeof(ach_index_t)*shm->index_cnt + shm->data_size;
    }else {
        if( ! channel_name_ok( channel_name ) )
            return ACH_INVALID_NAME;
        /* open shm */
        if( ! channel_name_ok( channel_name ) ) return ACH_INVALID_NAME;
        if( (fd = fd_for_channel_name( channel_name, 0 )) < 0 ) {
            return check_errno();
        }
        if( (shm = (ach_header_t*) mmap (NULL, sizeof(ach_header_t),
                                         PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0ul) )
            == MAP_FAILED )
            return ACH_FAILED_SYSCALL;
        if( ACH_SHM_MAGIC_NUM != shm->magic )
            return ACH_BAD_SHM_FILE;

        /* calculate mmaping size */
        len = sizeof(ach_header_t) + sizeof(ach_index_t)*shm->index_cnt + shm->data_size;

        /* remap */
        if( -1 ==  munmap( shm, sizeof(ach_header_t) ) )
            return check_errno();

        if( (shm = (ach_header_t*) mmap( NULL, len, PROT_READ|PROT_WRITE,
                                         MAP_SHARED, fd, 0ul) )
            == MAP_FAILED )
            return check_errno();
    }

    /* Check guard bytes */
    {
        enum ach_status r = check_guards(shm);
        if( ACH_OK != r ) return r;
    }

    /* initialize struct */
    chan->fd = fd;
    chan->len = len;
    chan->shm = shm;
    chan->seq_num = 0;
    chan->next_index = 1;
    chan->cancel = 0;

    return ACH_OK;
}


/** Copies frame pointed to by index entry at index_offset.

    \pre hold read lock on the channel

    \pre on success, buf holds the frame seq_num and next_index fields
    are incremented. The variable pointed to by size_written holds the
    number of bytes written to buf (0 on failure).
*/
static enum ach_status
ach_get_from_offset( ach_channel_t *chan, size_t index_offset,
                     char *buf, size_t size, size_t *frame_size ) {
    ach_header_t *shm = chan->shm;
    assert( index_offset < shm->index_cnt );
    ach_index_t *idx = ACH_SHM_INDEX(shm) + index_offset;
    /* assert( idx->size ); */
    assert( idx->seq_num );
    assert( idx->offset < shm->data_size );
    /* check idx */
    if( chan->seq_num > idx->seq_num ) {
        fprintf(stderr,
                "ach bug: chan->seq_num (%"PRIu64") > idx->seq_num (%"PRIu64")\n"
                "ach bug: index offset: %"PRIuPTR"\n",
                chan->seq_num, idx->seq_num,
                index_offset );
        return ACH_BUG;
    }

    if(  idx->size > size ) {
        /* buffer overflow */
        *frame_size = idx->size;
        return ACH_OVERFLOW;
    } else {
        /* good to copy */
        uint8_t *data_buf = ACH_SHM_DATA(shm);
        if( idx->offset + idx->size < shm->data_size ) {
            /* simple memcpy */
            memcpy( (uint8_t*)buf, data_buf + idx->offset, idx->size );
        }else {
            /* wraparound memcpy */
            size_t end_cnt = shm->data_size - idx->offset;
            memcpy( (uint8_t*)buf, data_buf + idx->offset, end_cnt );
            memcpy( (uint8_t*)buf + end_cnt, data_buf, idx->size - end_cnt );
        }
        *frame_size = idx->size;
        chan->seq_num = idx->seq_num;
        chan->next_index = (index_offset + 1) % shm->index_cnt;
        return ACH_OK;
    }
}

enum ach_status
ach_get( ach_channel_t *chan, void *buf, size_t size,
         size_t *frame_size,
         const struct timespec *ACH_RESTRICT abstime,
         int options ) {
    ach_header_t *shm = chan->shm;
    ach_index_t *index_ar = ACH_SHM_INDEX(shm);

    /* Check guard bytes */
    {
        enum ach_status r = check_guards(shm);
        if( ACH_OK != r ) return r;
    }

    const bool o_wait = options & ACH_O_WAIT;
    const bool o_last = options & ACH_O_LAST;
    const bool o_copy = options & ACH_O_COPY;

    /* take read lock */
    {
        enum ach_status r = rdlock( chan, o_wait, abstime );
        if( ACH_OK != r ) return r;
    }
    assert( chan->seq_num <= shm->last_seq );

    enum ach_status retval = ACH_BUG;
    bool missed_frame = 0;

    /* get the data */
    if( (chan->seq_num == shm->last_seq && !o_copy) || 0 == shm->last_seq ) {
        /* no entries */
        assert(!o_wait);
        retval = ACH_STALE_FRAMES;
    } else {
        /* Compute the index to read */
        size_t read_index;
        if( o_last ) {
            /* normal case, get last */
            read_index = last_index_i(shm);
        } else if (!o_last &&
                   index_ar[chan->next_index].seq_num == chan->seq_num + 1) {
            /* normal case, get next */
            read_index = chan->next_index;
        } else {
            /* exception case, figure out which frame */
            if (chan->seq_num == shm->last_seq) {
                /* copy last */
                assert(o_copy);
                read_index = last_index_i(shm);
            } else {
                /* copy oldest */
                read_index = oldest_index_i(shm);
            }
        }

        if( index_ar[read_index].seq_num > chan->seq_num + 1 ) { missed_frame = 1; }

        /* read from the index */
        retval = ach_get_from_offset( chan, read_index, (char*)buf, size,
                                      frame_size );

        assert( index_ar[read_index].seq_num > 0 );
    }

    /* release read lock */
    ach_status_t r = unrdlock( shm );
    if( ACH_OK != r ) return r;

    return (ACH_OK == retval && missed_frame) ? ACH_MISSED_FRAME : retval;
}

enum ach_status
ach_get_loud( ach_channel_t *chan, void *buf, size_t size,
         size_t *frame_size,
         const struct timespec *ACH_RESTRICT abstime,
         int options ) {
    fprintf(stdout, "\n1 "); fflush(stdout);
    ach_header_t *shm = chan->shm;
    fprintf(stdout, "2 "); fflush(stdout);
    ach_index_t *index_ar = ACH_SHM_INDEX(shm);

    /* Check guard bytes */
    {
        fprintf(stdout, "3 "); fflush(stdout);
        enum ach_status r = check_guards(shm);
        fprintf(stdout, "4 "); fflush(stdout);
        if( ACH_OK != r ) return r;
    }

    fprintf(stdout, "5 "); fflush(stdout);
    const bool o_wait = options & ACH_O_WAIT;
    const bool o_last = options & ACH_O_LAST;
    const bool o_copy = options & ACH_O_COPY;

    /* take read lock */
    {
        fprintf(stdout, "6 "); fflush(stdout);
        enum ach_status r = rdlock_loud( chan, o_wait, abstime );
        fprintf(stdout, "7 "); fflush(stdout);
        if( ACH_OK != r ) return r;
    }
    fprintf(stdout, "8 "); fflush(stdout);
    assert( chan->seq_num <= shm->last_seq );

    fprintf(stdout, "9 "); fflush(stdout);
    enum ach_status retval = ACH_BUG;
    bool missed_frame = 0;

    /* get the data */
    if( (chan->seq_num == shm->last_seq && !o_copy) || 0 == shm->last_seq ) {
        /* no entries */
        fprintf(stdout, "9.01 "); fflush(stdout);
        assert(!o_wait);
        fprintf(stdout, "9.02 "); fflush(stdout);
        retval = ACH_STALE_FRAMES;
    } else {
        /* Compute the index to read */
        fprintf(stdout, "9.11 "); fflush(stdout);
        size_t read_index;
        if( o_last ) {
            /* normal case, get last */
            fprintf(stdout, "9.1101 "); fflush(stdout);
            read_index = last_index_i(shm);
        } else if (!o_last &&
                   index_ar[chan->next_index].seq_num == chan->seq_num + 1) {
            /* normal case, get next */
            fprintf(stdout, "9.1111 "); fflush(stdout);
            read_index = chan->next_index;
        } else {
            /* exception case, figure out which frame */
            fprintf(stdout, "9.1121 "); fflush(stdout);
            if (chan->seq_num == shm->last_seq) {
                /* copy last */
                fprintf(stdout, "9.112101 "); fflush(stdout);
                assert(o_copy);
                fprintf(stdout, "9.112102 "); fflush(stdout);
                read_index = last_index_i(shm);
            } else {
                /* copy oldest */
                fprintf(stdout, "9.112111 "); fflush(stdout);
                read_index = oldest_index_i(shm);
            }
        }
        fprintf(stdout, "9.12 "); fflush(stdout);
        if( index_ar[read_index].seq_num > chan->seq_num + 1 ) { missed_frame = 1; }

        /* read from the index */
        fprintf(stdout, "9.13 "); fflush(stdout);
        retval = ach_get_from_offset( chan, read_index, (char*)buf, size,
                                      frame_size );

        fprintf(stdout, "9.14 "); fflush(stdout);
        assert( index_ar[read_index].seq_num > 0 );
    }

    /* release read lock */
    fprintf(stdout, "10 "); fflush(stdout);
    ach_status_t r = unrdlock( shm );
    fprintf(stdout, "11 "); fflush(stdout);
    if( ACH_OK != r ) return r;

    fprintf(stdout, "12 "); fflush(stdout);
    return (ACH_OK == retval && missed_frame) ? ACH_MISSED_FRAME : retval;
}

enum ach_status
ach_flush( ach_channel_t *chan ) {
    ach_header_t *shm = chan->shm;
    enum ach_status r = rdlock(chan, 0,  NULL);
    if( ACH_OK != r ) return r;

    chan->seq_num = shm->last_seq;
    chan->next_index = shm->index_head;
    return unrdlock(shm);
}


static void free_index(ach_header_t *shm, size_t i ) {
    ach_index_t *index_ar = ACH_SHM_INDEX(shm);

    assert( index_ar[i].seq_num ); /* only free used indices */
    assert( index_ar[i].size );    /* must have some data */
    assert( shm->index_free < shm->index_cnt ); /* must be some used index */

    shm->data_free += index_ar[i].size;
    shm->index_free ++;
    memset( &index_ar[i], 0, sizeof( ach_index_t ) );
}

enum ach_status
ach_put( ach_channel_t *chan, const void *buf, size_t len ) {
    if( 0 == len || NULL == buf || NULL == chan->shm ) {
        return ACH_EINVAL;
    }

    ach_header_t *shm = chan->shm;

    /* Check guard bytes */
    {
        enum ach_status r = check_guards(shm);
        if( ACH_OK != r ) return r;
    }

    if( shm->data_size < len ) {
        return ACH_OVERFLOW;
    }

    ach_index_t *index_ar = ACH_SHM_INDEX(shm);
    uint8_t *data_ar = ACH_SHM_DATA(shm);

    if( len > shm->data_size ) return ACH_OVERFLOW;

    /* take write lock */
    wrlock( chan );

    /* find next index entry */
    ach_index_t *idx = index_ar + shm->index_head;

    /* clear entry used by index */
    if( 0 == shm->index_free ) { free_index(shm,shm->index_head); }
    else { assert(0== index_ar[shm->index_head].seq_num);}

    assert( shm->index_free > 0 );

    /* clear overlapping entries */
    size_t i;
    for(i = (shm->index_head + shm->index_free) % shm->index_cnt;
        shm->data_free < len;
        i = (i + 1) % shm->index_cnt) {
        assert( i != shm->index_head );
        free_index(shm,i);
    }

    assert( shm->data_free >= len );

    /* copy buffer */
    if( shm->data_size - shm->data_head >= len ) {
        /* simply copy */
        memcpy( data_ar + shm->data_head, buf, len );
    } else {
        /* wraparound copy */
        size_t end_cnt = shm->data_size - shm->data_head;
        memcpy( data_ar + shm->data_head, buf, end_cnt);
        memcpy( data_ar, (uint8_t*)buf + end_cnt, len - end_cnt );
    }

    /* modify counts */
    shm->last_seq++;
    idx->seq_num = shm->last_seq;
    idx->size = len;
    idx->offset = shm->data_head;

    shm->data_head = (shm->data_head + len) % shm->data_size;
    shm->data_free -= len;
    shm->index_head = (shm->index_head + 1) % shm->index_cnt;
    shm->index_free --;

    assert( shm->index_free <= shm->index_cnt );
    assert( shm->data_free <= shm->data_size );
    assert( shm->last_seq > 0 );

    /* release write lock */
    return unwrlock( shm );

}

enum ach_status
ach_put_loud( ach_channel_t *chan, const void *buf, size_t len ) {
    if( 0 == len || NULL == buf || NULL == chan->shm ) {
        fprintf(stdout, "\n1 "); fflush(stdout);
        return ACH_EINVAL;
    }

    fprintf(stdout, "2 "); fflush(stdout);
    ach_header_t *shm = chan->shm;

    /* Check guard bytes */
    {
        fprintf(stdout, "3 "); fflush(stdout);
        enum ach_status r = check_guards(shm);
        fprintf(stdout, "4 "); fflush(stdout);
        if( ACH_OK != r ) return r;
    }

    if( shm->data_size < len ) {
        fprintf(stdout, "5 "); fflush(stdout);
        return ACH_OVERFLOW;
    }

    fprintf(stdout, "6 "); fflush(stdout);
    ach_index_t *index_ar = ACH_SHM_INDEX(shm);
    fprintf(stdout, "7 "); fflush(stdout);
    uint8_t *data_ar = ACH_SHM_DATA(shm);

    if( len > shm->data_size ) {
        fprintf(stdout, "8 "); fflush(stdout);
        return ACH_OVERFLOW;
    }

    /* take write lock */
    fprintf(stdout, "9 "); fflush(stdout);
    wrlock( chan );

    /* find next index entry */
    fprintf(stdout, "10 "); fflush(stdout);
    ach_index_t *idx = index_ar + shm->index_head;

    /* clear entry used by index */
    if( 0 == shm->index_free ) {
      fprintf(stdout, "10.01 "); fflush(stdout);
      free_index(shm,shm->index_head);
    }
    else {
      fprintf(stdout, "10.11 "); fflush(stdout);
      assert(0== index_ar[shm->index_head].seq_num);
    }

    fprintf(stdout, "11 "); fflush(stdout);
    assert( shm->index_free > 0 );

    /* clear overlapping entries */
    size_t i;
    fprintf(stdout, "12 "); fflush(stdout); int print_iter = 1;
    for(i = (shm->index_head + shm->index_free) % shm->index_cnt;
        shm->data_free < len;
        i = (i + 1) % shm->index_cnt) {
        fprintf(stdout, "12.%03d ", print_iter); print_iter++; fflush(stdout);
        assert( i != shm->index_head );
        free_index(shm,i);
    }

    fprintf(stdout, "13 "); fflush(stdout);
    assert( shm->data_free >= len );

    /* copy buffer */
    if( shm->data_size - shm->data_head >= len ) {
        /* simply copy */
        fprintf(stdout, "13.01 "); fflush(stdout);
        memcpy( data_ar + shm->data_head, buf, len );
    } else {
        /* wraparound copy */
        fprintf(stdout, "13.11 "); fflush(stdout);
        size_t end_cnt = shm->data_size - shm->data_head;
        fprintf(stdout, "13.12 "); fflush(stdout);
        memcpy( data_ar + shm->data_head, buf, end_cnt);
        fprintf(stdout, "13.13 "); fflush(stdout);
        memcpy( data_ar, (uint8_t*)buf + end_cnt, len - end_cnt );
    }

    /* modify counts */
    fprintf(stdout, "14 "); fflush(stdout);
    shm->last_seq++;
    idx->seq_num = shm->last_seq;
    idx->size = len;
    idx->offset = shm->data_head;

    fprintf(stdout, "15 "); fflush(stdout);
    shm->data_head = (shm->data_head + len) % shm->data_size;
    shm->data_free -= len;
    shm->index_head = (shm->index_head + 1) % shm->index_cnt;
    shm->index_free --;

    fprintf(stdout, "16 "); fflush(stdout);
    assert( shm->index_free <= shm->index_cnt );
    fprintf(stdout, "17 "); fflush(stdout);
    assert( shm->data_free <= shm->data_size );
    fprintf(stdout, "18 "); fflush(stdout);
    assert( shm->last_seq > 0 );

    /* release write lock */
    fprintf(stdout, "19 "); fflush(stdout);
    enum ach_status ret = unwrlock_loud( shm );

    fprintf(stdout, "20 "); fflush(stdout);
    return ret;

}

enum ach_status
ach_close( ach_channel_t *chan ) {

    /* Check guard bytes */
    {
        enum ach_status r = check_guards(chan->shm);
        if( ACH_OK != r ) return r;
    }

    /* fprintf(stderr, "Closing\n"); */
    /* note the close in the channel */
    if( chan->attr.map_anon ) {
        /* FIXME: what to do here?? */
        ;
    } else {
        /* remove mapping */
        int r = munmap(chan->shm, chan->len);
        if( 0 != r ){
            DEBUGF("Failed to munmap channel\n");
            return ACH_FAILED_SYSCALL;
        }
        chan->shm = NULL;

        /* close file */
        int i = 0;
        do {
            IFDEBUG( i, DEBUGF("Retrying close()\n") )
            r = close(chan->fd);
        }while( -1 == r && EINTR == errno && i++ < ACH_INTR_RETRY );
        if( -1 == r ){
            DEBUGF("Failed to close() channel fd\n");
            return ACH_FAILED_SYSCALL;
        }
    }

    return ACH_OK;
}

void ach_dump( ach_header_t *shm ) {
    fprintf(stderr, "Magic: %x\n", shm->magic );
    fprintf(stderr, "len: %"PRIuPTR"\n", shm->len );
    fprintf(stderr, "data_size: %"PRIuPTR"\n", shm->data_size );
    fprintf(stderr, "data_head: %"PRIuPTR"\n", shm->data_head );
    fprintf(stderr, "data_free: %"PRIuPTR"\n", shm->data_free );
    fprintf(stderr, "index_head: %"PRIuPTR"\n", shm->index_head );
    fprintf(stderr, "index_free: %"PRIuPTR"\n", shm->index_free );
    fprintf(stderr, "last_seq: %"PRIu64"\n", shm->last_seq );
    fprintf(stderr, "head guard:  %"PRIx64"\n", * ACH_SHM_GUARD_HEADER(shm) );
    fprintf(stderr, "index guard: %"PRIx64"\n", * ACH_SHM_GUARD_INDEX(shm) );
    fprintf(stderr, "data guard:  %"PRIx64"\n", * ACH_SHM_GUARD_DATA(shm) );

    fprintf(stderr, "head seq:  %"PRIu64"\n",
            (ACH_SHM_INDEX(shm) +
             ((shm->index_head - 1 + shm->index_cnt) % shm->index_cnt)) -> seq_num );
    fprintf(stderr, "head size:  %"PRIuPTR"\n",
            (ACH_SHM_INDEX(shm) +
             ((shm->index_head - 1 + shm->index_cnt) % shm->index_cnt)) -> size );

}

void ach_attr_init( ach_attr_t *attr ) {
    memset( attr, 0, sizeof(ach_attr_t) );
}

enum ach_status
ach_chmod( ach_channel_t *chan, mode_t mode ) {
    return (0 == fchmod(chan->fd, mode)) ? ACH_OK : check_errno();;
}


enum ach_status
ach_unlink( const char *name ) {
    char shm_name[ACH_CHAN_NAME_MAX + 16];
    enum ach_status r = shmfile_for_channel_name( name, shm_name, sizeof(shm_name) );
    if( ACH_OK == r ) {
        /*r = shm_unlink(name); */
        int i = shm_unlink(shm_name);
        if( 0 == i ) {
            return  ACH_OK;
        } else {
            return check_errno();
        }
    } else {
        return r;
    }
}


void
ach_cancel_attr_init( ach_cancel_attr_t *attr ) {
    memset(attr, 0, sizeof(*attr));
}

static ach_cancel_attr_t default_cancel_attr = {.async_unsafe = 0};

enum ach_status
ach_cancel( ach_channel_t *chan, const ach_cancel_attr_t *attr ) {
    if( NULL == attr ) attr = &default_cancel_attr;

    if( attr->async_unsafe ) {
        /* Don't be async safe, i.e., called from another thread */
        enum ach_status r = chan_lock(chan);
        if( ACH_OK != r ) return r;
        chan->cancel = 1;
        if( pthread_mutex_unlock( &chan->shm->sync.mutex ) ) return ACH_FAILED_SYSCALL;
        if( pthread_cond_broadcast( &chan->shm->sync.cond ) )  {
            return ACH_FAILED_SYSCALL;
        }
        return ACH_OK;
    } else {
        /* Async safe, i.e., called from from a signal handler */
        chan->cancel = 1; /* Set cancel from the parent */
        pid_t pid = fork();
        if( 0 == pid ) { /* child */
            /* Now we can touch the synchronization variables in
             * shared memory without deadlock/races */

            /* Take and release the mutex.  This ensures that the
             * previously set cancel field will be seen by the caller
             * in ach_get() before it waits on the condition variable.
             * Otherwise, there is a race between checking the cancel
             * (during a spurious wakeup) and then re-waiting on the
             * condition variable vs. setting the cancel and
             * broadcasting on the condition variable (check-cancel,
             * set-cancel, cond-broadcast, cond-wait loses the race).
             */
            /* Lock mutex */
            if( ACH_OK != chan_lock(chan) ) {
                /* TODO: pipe to pass error to parent? except, can't
                 * wait in the parent or we risk deadlock */
                DEBUG_PERROR("ach_cancel chan_lock()");
                exit(EXIT_FAILURE);
            }
            /* At this point, chan->cancel is TRUE and any waiters
             * inside ach_get() must be waiting on the condition
             * variable */
            /* Unlock Mutex */
            if( pthread_mutex_unlock( &chan->shm->sync.mutex ) ) {
                DEBUG_PERROR("ach_cancel pthread_mutex_unlock()");
                exit(EXIT_FAILURE);
            }
            /* Condition Broadcast */
            if( pthread_cond_broadcast( &chan->shm->sync.cond ) )  {
                DEBUG_PERROR("ach_cancel pthread_cond_broadcast()");
                exit(EXIT_FAILURE);
            }
            exit(EXIT_SUCCESS);
        } else if (0 < pid ) { /* parent */
            /* Can't wait around since we could be holding the mutex
             * here */
            return ACH_OK;
        } else { /* fork error */
            DEBUG_PERROR("ach_cancel fork()");
            return ACH_FAILED_SYSCALL;
        }
    }
    return ACH_BUG;
}
