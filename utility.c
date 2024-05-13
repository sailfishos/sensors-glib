/******************************************************************************
 * Copyright (c) 2024 Jollyboys Ltd.
 *
 * All rights reserved.
 *
 * This file is part of Sailfish sensors-glib package.
 *
 * You may use this file under the terms of BSD license as follows:
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its
 *    contributors may be used to endorse or promote products derived from
 *    this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 *****************************************************************************/

#include "utility.h"

#include "sfwlogging.h"

#include <sys/un.h>

#include <fcntl.h>

/* ========================================================================= *
 * Prototypes
 * ========================================================================= */

/* ------------------------------------------------------------------------- *
 * CANCELLABLE
 * ------------------------------------------------------------------------- */

bool cancellable_finish(GCancellable **pcancellable);
bool cancellable_cancel(GCancellable **pcancellable);
void cancellable_start (GCancellable **pcancellable);

/* ------------------------------------------------------------------------- *
 * SOCKET
 * ------------------------------------------------------------------------- */

bool    socket_setup_unix_addr(struct sockaddr_un *sa, socklen_t *sa_len, const char *path);
bool    socket_set_blocking   (int fd, bool blocking);
guint   socket_add_notify     (int fd, bool close_on_unref, GIOCondition cnd, GIOFunc io_cb, gpointer aptr);
int     socket_open           (const char *path);
bool    socket_close          (int fd);
bool    socket_close_at       (int *pfd);
ssize_t socket_write          (int fd, const void *data, size_t size);
ssize_t socket_read           (int fd, void *buff, size_t size);

/* ------------------------------------------------------------------------- *
 * ERROR
 * ------------------------------------------------------------------------- */

const char *error_message(GError *err);

/* ========================================================================= *
 * CANCELLABLE
 * ========================================================================= */

bool
cancellable_finish(GCancellable **pcancellable)
{
    GCancellable *cancelled = NULL;
    if( pcancellable ) {
        cancelled = *pcancellable, *pcancellable = NULL;
        if( cancelled ) {
            sfwlog_debug("FINISH cancellable=%p", cancelled);
            g_object_unref(cancelled);
        }
    }
    return cancelled != NULL;
}

bool
cancellable_cancel(GCancellable **pcancellable)
{
    GCancellable *cancelled = NULL;
    if( pcancellable ) {
        cancelled = *pcancellable, *pcancellable = NULL;
        if( cancelled ) {
            g_cancellable_cancel(*pcancellable);
            sfwlog_debug("FINISH cancellable=%p", cancelled);
            g_object_unref(cancelled);
        }
    }
    return cancelled != NULL;
}

void
cancellable_start(GCancellable **pcancellable)
{
    if( pcancellable ) {
        cancellable_cancel(pcancellable);
        *pcancellable = g_cancellable_new();
        sfwlog_debug("CREATE cancellable=%p", *pcancellable);
    }
}

/* ========================================================================= *
 * SOCKET
 * ========================================================================= */

bool
socket_setup_unix_addr(struct sockaddr_un *sa, socklen_t *sa_len,
                       const char *path)
{
    if( !path ) {
        sfwlog_err("null unix socket path given");
        return false;
    }

    socklen_t len = strnlen(path, sizeof sa->sun_path) + 1;
    if( len > sizeof sa->sun_path ) {
        sfwlog_err("%s: unix socket path too long", path);
        return false;
    }

    memset(sa, 0, sizeof *sa);
    sa->sun_family = AF_UNIX;
    strcpy(sa->sun_path, path);
    /* Starts with a '@' -> turn into abstract address */
    if( sa->sun_path[0] == '@' )
        sa->sun_path[0] = 0;
    len += offsetof(struct sockaddr_un, sun_path);

    *sa_len = len;
    return true;
}

bool
socket_set_blocking(int fd, bool blocking)
{
    bool ack   = false;
    int  flags = 0;

    if( (flags = fcntl(fd, F_GETFL, 0)) == -1 ) {
        sfwlog_err("could not get socket flags");
        goto EXIT;
    }

    if( blocking )
        flags &= ~O_NONBLOCK;
    else
        flags |= O_NONBLOCK;

    if( fcntl(fd, F_SETFL, flags) == -1 ) {
        sfwlog_err("could not set socket flags");
        goto EXIT;
    }

    ack = true;

EXIT:
    return ack;
}

guint
socket_add_notify(int fd, bool close_on_unref, GIOCondition cnd,
                  GIOFunc io_cb, gpointer aptr)
{
    guint         wid = 0;
    GIOChannel   *chn = NULL;

    if( !(chn = g_io_channel_unix_new(fd)) )
        goto EXIT;

    g_io_channel_set_close_on_unref(chn, close_on_unref);

    cnd |= G_IO_ERR | G_IO_HUP | G_IO_NVAL;

    if( !(wid = g_io_add_watch(chn, cnd, io_cb, aptr)) )
        goto EXIT;

EXIT:
    if( chn )
        g_io_channel_unref(chn);

    return wid;

}

int
socket_open(const char *path)
{
    bool ack = false;
    int  fd  = -1;

    struct sockaddr_un sa = {};
    socklen_t sa_len;

    /* create socket */
    if( (fd = socket(AF_UNIX, SOCK_STREAM, 0)) == -1 ) {
        sfwlog_err("socket: %m");
        goto EXIT;
    }

    /* make it non-blocking -> connect() will return immediately */
    if( !socket_set_blocking(fd, false) )
        goto EXIT;

    /* connect to daemon */
    if( !socket_setup_unix_addr(&sa, &sa_len, path) )
        goto EXIT;

    if( connect(fd, (struct sockaddr *)&sa, sa_len) == -1 ) {
        sfwlog_err("connect to %s: %m", path);
        goto EXIT;
    }

    ack = true;

EXIT:
    /* all or nothing */
    if( !ack && fd != -1 )
        close(fd), fd = -1;

    return fd;
}

bool socket_close(int fd)
{
    if( fd != -1 )
        close(fd);
    return fd != -1;
}

bool socket_close_at(int *pfd)
{
    bool closed = socket_close(*pfd);
    return *pfd = -1, closed;
}

ssize_t
socket_write(int fd, const void *data, size_t size)
{
    ssize_t done = send(fd, data, size, MSG_DONTWAIT | MSG_NOSIGNAL);

    if( done == -1 )
        sfwlog_err("socket write: %m");
    else if( (size_t)done < size )
        sfwlog_err("socket write: partial success");

    return done;
}

ssize_t
socket_read(int fd, void *buff, size_t size)
{
    ssize_t done = recv(fd, buff, size, MSG_DONTWAIT);

    if( done == -1 )
        sfwlog_err("socket reead: %m");
    else if( done == 0 )
        sfwlog_err("socket read: EOF");

    return done;
}

/* ========================================================================= *
 * ERROR
 * ========================================================================= */

const char *error_message(GError *err)
{
    return !err ? "(null error)" : err->message ?: "(null message)";
}
