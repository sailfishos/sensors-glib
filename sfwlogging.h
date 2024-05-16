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

#ifndef SFWLOGGING_H_
# define SFWLOGGING_H_

# include "sfwtypes.h"

# include <syslog.h>

G_BEGIN_DECLS

# pragma GCC visibility push(default)

/* ========================================================================= *
 * Constants
 * ========================================================================= */

enum {
    SFWLOG_EMERG   = LOG_EMERG,
    SFWLOG_ALERT   = LOG_ALERT,
    SFWLOG_CRIT    = LOG_CRIT,
    SFWLOG_ERR     = LOG_ERR,
    SFWLOG_WARNING = LOG_WARNING,
    SFWLOG_NOTICE  = LOG_NOTICE,
    SFWLOG_INFO    = LOG_INFO,
    SFWLOG_DEBUG   = LOG_DEBUG,
    SFWLOG_TRACE   = LOG_DEBUG + 1,

    SFWLOG_MINLEV  = SFWLOG_EMERG,
    SFWLOG_MAXLEV  = SFWLOG_TRACE,
    SFWLOG_DEFLEV  = SFWLOG_WARNING,
};

enum {
    SFWLOG_TO_UNSET,
    SFWLOG_TO_STDERR,
    SFWLOG_TO_SYSLOG,
};

/* ========================================================================= *
 * Types
 * ========================================================================= */

typedef struct SfwLoggingState {
    const char *file;
    const char *func;
    int         line;
    int         level;
    int         generation;
    bool        enabled;
} SfwLoggingState;

/* ========================================================================= *
 * Prototypes
 * ========================================================================= */

/* ------------------------------------------------------------------------- *
 * LOGGING
 * ------------------------------------------------------------------------- */

int         sfwlog_get_verbosity  (void);
void        sfwlog_set_verbosity  (int level);
void        sfwlog_set_target     (int target);
void        sfwlog_add_pattern    (const char *pattern);
void        sfwlog_remove_pattern (const char *pattern);
void        sfwlog_clear_patterns (void);
int         sfwlog_level_from_name(const char *name);
const char *sfwlog_level_name     (int level);
bool        sfwlog_p_             (const char *file, const char *func, int level);
void        sfwlog_emit_          (const char *file, int line, const char *func, int level, const char *fmt, ...) __attribute__((format(printf, 5, 6)));

/* ------------------------------------------------------------------------- *
 * LOGGING_STATE
 * ------------------------------------------------------------------------- */

bool sfwlogging_state_evaluate(SfwLoggingState *self);

/* ========================================================================= *
 * Macros
 * ========================================================================= */

# define sfwlog_p(LEV) ({\
    static SfwLoggingState cached_state = {\
        .file  = __FILE__,\
        .func  =  __func__,\
        .line  = __LINE__,\
        .level = LEV,\
    };\
    sfwlogging_state_evaluate(&cached_state);\
})

# define sfwlog_emit(LEV, FMT, ARGS...)\
     do {\
         if( sfwlog_p(LEV) ) \
             sfwlog_emit_(__FILE__, __LINE__, __func__, LEV, FMT, ##ARGS);\
     } while( 0 )

# define sfwlog_crit(   FMT,ARGS...) sfwlog_emit(SFWLOG_CRIT,    FMT, ##ARGS)
# define sfwlog_err(    FMT,ARGS...) sfwlog_emit(SFWLOG_ERR,     FMT, ##ARGS)
# define sfwlog_warning(FMT,ARGS...) sfwlog_emit(SFWLOG_WARNING, FMT, ##ARGS)
# define sfwlog_notice( FMT,ARGS...) sfwlog_emit(SFWLOG_NOTICE,  FMT, ##ARGS)
# define sfwlog_info(   FMT,ARGS...) sfwlog_emit(SFWLOG_INFO,    FMT, ##ARGS)
# define sfwlog_debug(  FMT,ARGS...) sfwlog_emit(SFWLOG_DEBUG,   FMT, ##ARGS)
# define sfwlog_trace(  FMT,ARGS...) sfwlog_emit(SFWLOG_TRACE,   FMT, ##ARGS)

# define sfwlog_here()  sfwlog_emit(SFWLOG_CRIT, "...")

# pragma GCC visibility pop

G_END_DECLS

#endif /* SFWLOGGING_H_ */
