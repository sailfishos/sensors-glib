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

#include "sfwlogging.h"

#include <stdio.h>
#include <inttypes.h>
#include <fnmatch.h>

/* ========================================================================= *
 * Types
 * ========================================================================= */

enum {
    EMIT_UNKNOWN = 0, /* Zero = matches null from hash table miss */
    EMIT_ENABLED,
    EMIT_DISABLED,
};

/* ========================================================================= *
 * Prototypes
 * ========================================================================= */

/* ------------------------------------------------------------------------- *
 * LOGGING
 * ------------------------------------------------------------------------- */

static uint64_t    sfwlog_tick            (void);
static char       *sfwlog_timestamp       (char *buff, size_t size);
static size_t      sfwlog_common_chars    (const char *s1, const char *s2);
static int         sfwlog_normalize_level (int level);
static int         sfwlog_syslog_level    (int level);
static const char *sfwlog_level_tag       (int level);
static bool        sfwlog_white_p         (int ch);
static bool        sfwlog_black_p         (int ch);
static char       *sfwlog_strip           (char *str);
static gchar      *sfwlog_lookup_pattern  (const char *pattern);
static gint        sfwlog_lookup_from_list(const gchar *key);
static gint        sfwlog_lookup_from_hash(const gchar *key);
static void        sfwlog_store_to_hash   (const gchar *key, gint emit);
static void        sfwlog_generation_bump (void);
int                sfwlog_get_verbosity   (void);
void               sfwlog_set_verbosity   (int level);
void               sfwlog_set_target      (int target);
void               sfwlog_add_pattern     (const char *pattern);
void               sfwlog_remove_pattern  (const char *pattern);
void               sfwlog_clear_patterns  (void);
int                sfwlog_level_from_name (const char *name);
const char        *sfwlog_level_name      (int level);
bool               sfwlog_p_              (const char *file, const char *func, int level);
void               sfwlog_emit_           (const char *file, int line, const char *func, int level, const char *fmt, ...) __attribute__((format(printf, 5, 6)));

/* ------------------------------------------------------------------------- *
 * LOGGING_STATE
 * ------------------------------------------------------------------------- */

bool sfwlogging_state_evaluate(SfwLoggingState *self);

/* ========================================================================= *
 * Data
 * ========================================================================= */

static int         sfwlog_target       = SFWLOG_TO_STDERR;
static int         sfwlog_level        = SFWLOG_DEFLEV;
static GHashTable *sfwlog_pattern_hash = NULL;
static GSList     *sfwlog_pattern_list = NULL;
static int         sfwlog_generation   = 1;

static const struct {
    const char *name;
    int         level;
} log_level_lut[] = {
    { "emerg",     SFWLOG_EMERG   },
    { "alert",     SFWLOG_ALERT   },
    { "crit",      SFWLOG_CRIT    },
    { "err",       SFWLOG_ERR     },
    { "warning",   SFWLOG_WARNING },
    { "notice",    SFWLOG_NOTICE  },
    { "info",      SFWLOG_INFO    },
    { "debug",     SFWLOG_DEBUG   },
    { "trace",     SFWLOG_TRACE   },
    { NULL,                       }
};

/* ========================================================================= *
 * LOGGING
 * ========================================================================= */

static uint64_t
sfwlog_tick(void)
{
    static int64_t t0 = 0;
    int64_t        t1 = 0;

    struct timespec ts;
    if( clock_gettime(CLOCK_BOOTTIME, &ts) == 0 ) {
        t1  = ts.tv_sec;
        t1 *= 1000;
        t1 += ts.tv_nsec / 1000000;
    }

    if( t0 == 0 )
        t0 = t1;

    return (uint64_t)(t1 - t0);
}

static char *
sfwlog_timestamp(char *buff, size_t size)
{
    uint64_t t  = sfwlog_tick();
    unsigned ms = (unsigned)(t % 1000); t /= 1000;
    snprintf(buff, size, "%04" PRIu64 ".%03u", t, ms);
    return buff;
}

static size_t sfwlog_common_chars(const char *s1, const char *s2)
{
    size_t n = 0;
    if( s1 && s2 ) {
        while( s1[n] && s1[n] == s2[n] )
            ++n;
    }
    return n;
}

static int
sfwlog_normalize_level(int level)
{
    if( level < SFWLOG_MINLEV )
        level = SFWLOG_MINLEV;
    else if( level > SFWLOG_MAXLEV )
        level = SFWLOG_MAXLEV;
    return level;
}

static int
sfwlog_syslog_level(int level)
{
    if( level < SFWLOG_EMERG )
        level = SFWLOG_EMERG;
    else if( level > SFWLOG_DEBUG )
        level = SFWLOG_DEBUG;
    return level;
}

static const char *
sfwlog_level_tag(int level)
{
    const char * const lut[] = {
        [SFWLOG_EMERG]   = "X: ",
        [SFWLOG_ALERT]   = "A: ",
        [SFWLOG_CRIT]    = "C: ",
        [SFWLOG_ERR]     = "E: ",
        [SFWLOG_WARNING] = "W: ",
        [SFWLOG_NOTICE]  = "N: ",
        [SFWLOG_INFO]    = "I: ",
        [SFWLOG_DEBUG]   = "D: ",
        [SFWLOG_TRACE]   = "T: ",
    };
    return lut[sfwlog_normalize_level(level)];
}

static bool
sfwlog_white_p(int ch)
{
    return (0 < ch) && (ch <= 32);
}

static bool
sfwlog_black_p(int ch)
{
    return (ch < 0) || (32 < ch);
}

static char *
sfwlog_strip(char *str)
{
    if( str ) {
        char *src = str;
        char *dst = str;
        while( sfwlog_white_p(*src) )
            ++src;
        for( ;; ) {
            while( sfwlog_black_p(*src) )
                *dst++ = *src++;
            while( sfwlog_white_p(*src) )
                ++src;
            if( !*src )
                break;
            *dst++ = ' ';
        }
        *dst = 0;
    }
    return str;
}

static gchar *
sfwlog_lookup_pattern(const char *pattern)
{
    gchar *cached = NULL;
    if( pattern ) {
        for( GSList *item = sfwlog_pattern_list; item; item = item->next ) {
            if( !g_strcmp0(item->data, pattern) ) {
                cached = item->data;
                break;
            }
        }
    }
    return cached;
}

static gint
sfwlog_lookup_from_list(const gchar *key)
{
    gint emit = EMIT_DISABLED;
    for( GSList *item = sfwlog_pattern_list; item; item = item->next ) {
        if( fnmatch(item->data, key, 0) == 0 ) {
            emit = EMIT_ENABLED;
            break;
        }
    }
    return emit;
}

static gint
sfwlog_lookup_from_hash(const gchar *key)
{
    gint emit = EMIT_UNKNOWN;
    if( sfwlog_pattern_hash )
        emit = GPOINTER_TO_INT(g_hash_table_lookup(sfwlog_pattern_hash, key));
    return emit;
}

static void
sfwlog_store_to_hash(const gchar *key, gint emit)
{
    if( !sfwlog_pattern_hash )
        sfwlog_pattern_hash = g_hash_table_new_full(g_str_hash,
                                                    g_str_equal,
                                                    g_free,
                                                    NULL);
    g_hash_table_replace(sfwlog_pattern_hash,
                         g_strdup(key),
                         GINT_TO_POINTER(emit));
}

static void sfwlog_generation_bump(void)
{
    /* Invalidate all cached SfwLoggingState instances */
    ++sfwlog_generation;

    /* Flush pattern match cache */
    if( sfwlog_pattern_hash ) {
        g_hash_table_unref(sfwlog_pattern_hash),
            sfwlog_pattern_hash = NULL;
    }
}

int
sfwlog_get_verbosity(void)
{
    return sfwlog_level;
}

void
sfwlog_set_verbosity(int level)
{
    level = sfwlog_normalize_level(level);
    if( sfwlog_level != level ) {
        sfwlog_level = level;
        sfwlog_generation_bump();
    }
}

void
sfwlog_set_target(int target)
{
    if( target != SFWLOG_TO_SYSLOG )
        target = SFWLOG_TO_STDERR;

    switch( target ) {
    case SFWLOG_TO_STDERR:
    case SFWLOG_TO_SYSLOG:
        sfwlog_target = target;
        break;
    default:
        break;
    }
}

void
sfwlog_add_pattern(const char *pattern)
{
    if( pattern && !sfwlog_lookup_pattern(pattern) ) {
        sfwlog_pattern_list = g_slist_prepend(sfwlog_pattern_list,
                                              g_strdup(pattern));
        sfwlog_generation_bump();
    }
}

void
sfwlog_remove_pattern(const char *pattern)
{
    gchar *cached = sfwlog_lookup_pattern(pattern);
    if( cached ) {
        sfwlog_pattern_list = g_slist_remove(sfwlog_pattern_list, cached);
        g_free(cached);
        sfwlog_generation_bump();
    }
}

void
sfwlog_clear_patterns(void)
{
    if( sfwlog_pattern_list ) {
        g_slist_free_full(g_steal_pointer(&sfwlog_pattern_list), g_free);
        sfwlog_generation_bump();
    }
}

int
sfwlog_level_from_name(const char *name)
{
    int level = SFWLOG_DEFLEV;
    if( name ) {
        size_t best = 0;
        for( size_t i = 0; log_level_lut[i].name; ++i ) {
            size_t score = sfwlog_common_chars(log_level_lut[i].name, name);
            if( best < score ) {
                best  = score;
                level = log_level_lut[i].level;
            }
        }
    }
    return level;
}

const char *
sfwlog_level_name(int level)
{
    const char *name = "unknown";
    for( size_t i = 0; log_level_lut[i].name; ++i ) {
        if( log_level_lut[i].level == level ) {
            name = log_level_lut[i].name;
            break;
        }
    }
    return name;
}

bool
sfwlog_p_(const char *file, const char *func, int level)
{
    /* Logging must not change errno */
    int  saved = errno;
    gint state = EMIT_DISABLED;
    if( level <= sfwlog_level ) {
        state = EMIT_ENABLED;
    }
    else if( !sfwlog_pattern_list ) {
        state = EMIT_DISABLED;
    }
    else {
        gchar *key = g_strdup_printf("%s:%s", file, func);
        if( (state = sfwlog_lookup_from_hash(key)) == EMIT_UNKNOWN ) {
            state = sfwlog_lookup_from_list(key);
            sfwlog_store_to_hash(key, state);
        }
        g_free(key);
    }
    errno = saved;
    return state == EMIT_ENABLED;
}

void
sfwlog_emit_(const char *file, int line, const char *func, int level,
             const char *fmt, ...)
{
    /* Logging must not change errno */
    int saved = errno;
    char *msg = NULL;
    va_list va;
    va_start(va, fmt);
    if( vasprintf(&msg, fmt, va) == -1 )
        msg = NULL;
    else
        sfwlog_strip(msg);
    va_end(va);

    char  timestamp[32];
    char  context[128];

    switch( sfwlog_target ) {
    default:
    case SFWLOG_TO_STDERR:
        sfwlog_timestamp(timestamp, sizeof timestamp);
        snprintf(context, sizeof context, "%s:%d:", file, line);
        fprintf(stderr, "%-21s %s %s: %s%s\n",
                context,
                timestamp,
                func,
                sfwlog_level_tag(level),
                msg ?: fmt);
        fflush(stderr);
        break;
    case SFWLOG_TO_SYSLOG:
        syslog(sfwlog_syslog_level(level), "%s", msg ?: fmt);
        break;
    }

    free(msg);
    errno = saved;
}

/* ========================================================================= *
 * LOGGING_STATE
 * ========================================================================= */

bool
sfwlogging_state_evaluate(SfwLoggingState *self)
{
    /* Logging must not change errno */
    if( self->generation != sfwlog_generation ) {
        self->generation = sfwlog_generation;
        self->enabled    = sfwlog_p_(self->file, self->func, self->level);
    }
    return self->enabled;
}
