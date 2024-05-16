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

#ifndef UTILITY_H_
# define UTILITY_H_

# include <sys/socket.h>

# include <stdbool.h>

# include <gio/gio.h>

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
 * GVariant helpers
 * ========================================================================= */

static inline void
gutil_variant_unref(GVariant *variant)
{
    if( variant )
        g_variant_unref(variant);
}

static inline void
gutil_variant_unref_at(GVariant **pvariant)
{
    gutil_variant_unref(*pvariant), *pvariant = NULL;
}

static inline void
gutil_variant_unref_cb(gpointer variant)
{
    gutil_variant_unref(variant);
}

static inline bool
gutil_variant_equal(const GVariant *v1, const GVariant *v2)
{
    return (v1 == v2) || (v1 && v2 && g_variant_equal(v1, v2));
}

/* ========================================================================= *
 * GObject helpers
 * ========================================================================= */

static inline gpointer
gutil_object_ref(gpointer object)
{
    return object ? g_object_ref(object) : NULL;
}

static inline void
gutil_object_unref(gpointer object)
{
    if( object )
        g_object_unref(object);
}

static inline void
gutil_object_unref_at(gpointer pobject)
{
    gutil_object_unref(*(GObject **)pobject), *(GObject **)pobject = NULL;
}

static inline void
gutil_object_unref_cb(gpointer object)
{
    gutil_object_unref(object);
}

/* ========================================================================= *
 * Generic source id helpers
 * ========================================================================= */

static inline bool
gutil_source_remove(guint source)
{
    bool removed = false;
    if( source )
        g_source_remove(source), removed = true;
    return removed;
}

static inline bool
gutil_source_remove_at(guint *psource)
{
    bool removed = gutil_source_remove(*psource);
    return *psource = 0, removed;
}

static inline void
gutil_source_remove_cb(gpointer source)
{
    gutil_source_remove(GPOINTER_TO_INT(source));
}

/* ========================================================================= *
 * Timer source id helpers
 * ========================================================================= */

static inline bool
gutil_timeout_stop(guint *psource)
{
    bool stopped = false;
    if( *psource )
        g_source_remove(*psource), *psource = 0, stopped = true;
    return stopped;
}

static inline bool
gutil_timeout_start(guint *psource, guint interval, GSourceFunc func,
                    gpointer aptr)
{
    bool started = false;
    if( !*psource )
        *psource = g_timeout_add(interval, func, aptr), started = true;
    return started;
}

static inline bool
gutil_timeout_restart(guint *psource, guint interval, GSourceFunc func,
                      gpointer aptr)
{
    bool restarted = gutil_timeout_stop(psource);
    *psource = g_timeout_add(interval, func, aptr);
    return restarted;
}

#endif /* UTILITY_H_ */
