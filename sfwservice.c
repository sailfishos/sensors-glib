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

#include "sfwservice.h"

#include "sfwlogging.h"
#include "sfwdbus.h"
#include "utility.h"

/* ========================================================================= *
 * Types
 * ========================================================================= */

typedef struct SfwServicePrivate SfwServicePrivate;

typedef enum SfwServiceSignal
{
    SFWSERVICE_SIGNAL_VALID_CHANGED,
    SFWSERVICE_SIGNAL_COUNT,
} SfwServiceSignal;

typedef enum SfwServiceState {
    SFWSERVICESTATE_INITIAL,
    SFWSERVICESTATE_DISABLED,
    SFWSERVICESTATE_ENUMERATING,
    SFWSERVICESTATE_READY,
    SFWSERVICESTATE_FAILED,
    SFWSERVICESTATE_FINAL,
    SFWSERVICESTATE_COUNT
} SfwServiceState;

struct SfwServicePrivate
{
    bool                srv_valid;
    SfwServiceState        srv_state;
    guint               srv_eval_state_id;
    guint               srv_retry_delay_id;

    GDBusConnection    *srv_connection;
    GCancellable       *srv_bus_get_cancellable;

    gchar              *srv_name_owner;
    guint               srv_name_watcher_id;

    GHashTable         *srv_available_sensors;
    GCancellable       *srv_enumerate_cancellable;
};

struct SfwService
{
    GObject object;
};

typedef GObjectClass SfwServiceClass;

/* ========================================================================= *
 * Macros
 * ========================================================================= */

# define sfwservice_log_emit(LEV, FMT, ARGS...)\
     sfwlog_emit(LEV, "sfservice: " FMT, ##ARGS)

# define sfwservice_log_crit(   FMT, ARGS...) sfwservice_log_emit(SFWLOG_CRIT,    FMT, ##ARGS)
# define sfwservice_log_err(    FMT, ARGS...) sfwservice_log_emit(SFWLOG_ERR,     FMT, ##ARGS)
# define sfwservice_log_warning(FMT, ARGS...) sfwservice_log_emit(SFWLOG_WARNING, FMT, ##ARGS)
# define sfwservice_log_notice( FMT, ARGS...) sfwservice_log_emit(SFWLOG_NOTICE,  FMT, ##ARGS)
# define sfwservice_log_info(   FMT, ARGS...) sfwservice_log_emit(SFWLOG_INFO,    FMT, ##ARGS)
# define sfwservice_log_debug(  FMT, ARGS...) sfwservice_log_emit(SFWLOG_DEBUG,   FMT, ##ARGS)
# define sfwservice_log_trace(  FMT, ARGS...) sfwservice_log_emit(SFWLOG_TRACE,   FMT, ##ARGS)

/* ========================================================================= *
 * Prototypes
 * ========================================================================= */

/* ------------------------------------------------------------------------- *
 * SFWSERVICE_CLASS
 * ------------------------------------------------------------------------- */

static void sfwservice_class_intern_init(gpointer klass);
static void sfwservice_class_init       (SfwServiceClass *klass);

/* ------------------------------------------------------------------------- *
 * SFWSERVICE_TYPE
 * ------------------------------------------------------------------------- */

GType        sfwservice_get_type     (void);
static GType sfwservice_get_type_once(void);

/* ------------------------------------------------------------------------- *
 * SFWSERVICESTATE
 * ------------------------------------------------------------------------- */

static const char *sfwservicestate_repr(SfwServiceState state);

/* ------------------------------------------------------------------------- *
 * SFWSERVICE_LIFECYCLE
 * ------------------------------------------------------------------------- */

static void        sfwservice_init    (SfwService *self);
static void        sfwservice_finalize(GObject *object);
static SfwService *sfwservice_new     (void);
SfwService        *sfwservice_instance(void);
SfwService        *sfwservice_ref     (SfwService *self);
void               sfwservice_unref   (SfwService *self);
void               sfwservice_unref_at(SfwService **pself);

/* ------------------------------------------------------------------------- *
 * SFWSERVICE_VALID
 * ------------------------------------------------------------------------- */

bool        sfwservice_is_valid (const SfwService *self);
static void sfwservice_set_valid(SfwService *self, bool valid);

/* ------------------------------------------------------------------------- *
 * SFWSERVICE_SIGNALS
 * ------------------------------------------------------------------------- */

static gulong sfwservice_add_handler              (SfwService *self, SfwServiceSignal signo, SfwServiceHandler handler, gpointer aptr);
gulong        sfwservice_add_valid_changed_handler(SfwService *self, SfwServiceHandler handler, gpointer aptr);
void          sfwservice_remove_handler           (SfwService *self, gulong id);
void          sfwservice_remove_handler_at        (SfwService *self, gulong *pid);
static void   sfwservice_emit_signal              (SfwService *self, SfwServiceSignal signo);

/* ------------------------------------------------------------------------- *
 * SFWSERVICE_ACCESSORS
 * ------------------------------------------------------------------------- */

static SfwServicePrivate *sfwservice_priv(const SfwService *self);

/* ------------------------------------------------------------------------- *
 * SFWSERVICE_CONNECTION
 * ------------------------------------------------------------------------- */

GDBusConnection *sfwservice_get_connection(const SfwService *self);
static void      sfwservice_set_connection(SfwService *self, GDBusConnection *con);

/* ------------------------------------------------------------------------- *
 * SFWSERVICE_NAME_OWNER
 * ------------------------------------------------------------------------- */

static void sfwservice_set_name_owner        (SfwService *self, const gchar *name_owner);
static void sfwservice_name_owner_vanished_cb(GDBusConnection *connection, const gchar *name, gpointer aptr);
static void sfwservice_name_owner_appeared_cb(GDBusConnection *connection, const gchar *name, const gchar *name_owner, gpointer aptr);
static void sfwservice_unwatch_name_owner    (SfwService *self);
static void sfwservice_watch_name_owner      (SfwService *self);

/* ------------------------------------------------------------------------- *
 * SFWSERVICE_STM_STATE
 * ------------------------------------------------------------------------- */

static const char      *sfwservice_stm_state_name       (const SfwService *self);
static SfwServiceState  sfwservice_stm_get_state        (const SfwService *self);
static void             sfwservice_stm_set_state        (SfwService *self, SfwServiceState state);
static void             sfwservice_stm_enter_state      (SfwService *self);
static void             sfwservice_stm_leave_state      (SfwService *self);
static gboolean         sfwservice_stm_eval_state_cb    (gpointer aptr);
static void             sfwservice_stm_cancel_eval_state(SfwService *self);
static void             sfwservice_stm_eval_state_later (SfwService *self);

/* ------------------------------------------------------------------------- *
 * SFWSERVICE_STM_RETRY
 * ------------------------------------------------------------------------- */

static gboolean sfwservice_stm_retry_delay_cb     (gpointer aptr);
static void     sfwservice_stm_start_retry_delay  (SfwService *self);
static void     sfwservice_stm_cancel_retry_delay (SfwService *self);
static bool     sfwservice_stm_pending_retry_delay(const SfwService *self);

/* ------------------------------------------------------------------------- *
 * SFWSERVICE_STM_CONNECTION
 * ------------------------------------------------------------------------- */

static void sfwservice_stm_connect_cb(GObject *object, GAsyncResult *res, gpointer aptr);
static void sfwservice_stm_connect   (SfwService *self);
static void sfwservice_stm_disconnect(SfwService *self);

/* ------------------------------------------------------------------------- *
 * SFWSERVICE_STM_ENUMERATE
 * ------------------------------------------------------------------------- */

static void sfwservice_stm_enumerate_cb     (GObject *object, GAsyncResult *res, gpointer aptr);
static void sfwservice_stm_start_enumerate  (SfwService *self);
static void sfwservice_stm_cancel_enumerate (SfwService *self);
static bool sfwservice_stm_pending_enumerate(SfwService *self);

/* ========================================================================= *
 * SFWSERVICE_CLASS
 * ========================================================================= */

G_DEFINE_TYPE_WITH_PRIVATE(SfwService, sfwservice, G_TYPE_OBJECT)
#define SFWSERVICE_TYPE (sfwservice_get_type())
#define SFWSERVICE(obj) (G_TYPE_CHECK_INSTANCE_CAST(obj, SFWSERVICE_TYPE, SfwService))

static const char * const sfwservice_signal_name[SFWSERVICE_SIGNAL_COUNT] =
{
    [SFWSERVICE_SIGNAL_VALID_CHANGED] = "sfwservice-valid-changed",
};

static guint sfwservice_signal_id[SFWSERVICE_SIGNAL_COUNT] = { };

static void
sfwservice_class_init(SfwServiceClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS(klass);

    object_class->finalize = sfwservice_finalize;

    for( guint signo = 0; signo < SFWSERVICE_SIGNAL_COUNT; ++signo )
        sfwservice_signal_id[signo] = g_signal_new(sfwservice_signal_name[signo],
                                                   G_OBJECT_CLASS_TYPE(klass),
                                                   G_SIGNAL_RUN_FIRST,
                                                   0, NULL, NULL, NULL,
                                                   G_TYPE_NONE, 0);
}

/* ========================================================================= *
 * SFWSERVICESTATE
 * ========================================================================= */

static const char *
sfwservicestate_repr(SfwServiceState state)
{
    const char *repr = "SFWSERVICESTATE_INVALID";
    switch( state ) {
    case SFWSERVICESTATE_INITIAL:     repr = "SFWSERVICESTATE_INITIAL";     break;
    case SFWSERVICESTATE_DISABLED:    repr = "SFWSERVICESTATE_DISABLED";    break;
    case SFWSERVICESTATE_ENUMERATING: repr = "SFWSERVICESTATE_ENUMERATING"; break;
    case SFWSERVICESTATE_READY:       repr = "SFWSERVICESTATE_READY";       break;
    case SFWSERVICESTATE_FAILED:      repr = "SFWSERVICESTATE_FAILED";      break;
    case SFWSERVICESTATE_FINAL:       repr = "SFWSERVICESTATE_FINAL";       break;
    default: break;
    }
    return repr;
}

/* ========================================================================= *
 * SFWSERVICE
 * ========================================================================= */

/* ------------------------------------------------------------------------- *
 * SFWSERVICE_LIFECYCLE
 * ------------------------------------------------------------------------- */

static void
sfwservice_init(SfwService *self)
{
    SfwServicePrivate *priv = sfwservice_priv(self);

    priv->srv_valid                 = false;
    priv->srv_state                 = SFWSERVICESTATE_INITIAL;
    priv->srv_eval_state_id         = 0;
    priv->srv_retry_delay_id        = 0;
    priv->srv_connection            = NULL;
    priv->srv_bus_get_cancellable   = NULL;
    priv->srv_name_owner            = NULL;
    priv->srv_name_watcher_id       = 0;
    priv->srv_available_sensors     =
        g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);
    priv->srv_enumerate_cancellable = NULL;
}

static void
sfwservice_finalize(GObject *object)
{
    SfwService        *self = SFWSERVICE(object);
    SfwServicePrivate *priv = sfwservice_priv(self);
    sfwservice_log_info("DELETED");

    sfwservice_stm_set_state(self, SFWSERVICESTATE_FINAL);
    g_hash_table_unref(priv->srv_available_sensors),
        priv->srv_available_sensors = NULL;

    G_OBJECT_CLASS(sfwservice_parent_class)->finalize(object);
}

static SfwService *
sfwservice_new(void)
{
    SfwService *self = g_object_new(SFWSERVICE_TYPE, NULL);

    sfwservice_stm_set_state(self, SFWSERVICESTATE_DISABLED);

    sfwservice_log_info("CREATED");

    return self;
}

SfwService *
sfwservice_instance(void)
{
    static SfwService *sfwservice_instance = NULL;

    SfwService *self = sfwservice_ref(sfwservice_instance);
    if( !self ) {
        self = sfwservice_instance = sfwservice_new();
        g_object_add_weak_pointer(G_OBJECT(sfwservice_instance),
                                  (gpointer*)(&sfwservice_instance));
    }
    sfwservice_log_debug("sfwservice_instance=%p", self);
    return self;
}

SfwService *
sfwservice_ref(SfwService *self)
{
    if( self ) {
        sfwservice_log_debug("self=%p", self);
        g_object_ref(SFWSERVICE(self));
    }
    return self;
}

void
sfwservice_unref(SfwService *self)
{
    if( self ) {
        sfwservice_log_debug("self=%p", self);
        g_object_unref(SFWSERVICE(self));
    }
}

void
sfwservice_unref_at(SfwService **pself)
{
    sfwservice_unref(*pself), *pself = NULL;
}

/* ------------------------------------------------------------------------- *
 * SFWSERVICE_VALID
 * ------------------------------------------------------------------------- */

bool
sfwservice_is_valid(const SfwService *self)
{
    SfwServicePrivate *priv = sfwservice_priv(self);
    return priv ? priv->srv_valid : false;
}

static void
sfwservice_set_valid(SfwService *self, bool valid)
{
    SfwServicePrivate *priv = sfwservice_priv(self);
    if( priv && priv->srv_valid != valid ) {
        sfwservice_log_info("valid: %s -> %s",
                            priv->srv_valid ? "true" : "false",
                            valid           ? "true" : "false");
        priv->srv_valid = valid;
        sfwservice_emit_signal(self, SFWSERVICE_SIGNAL_VALID_CHANGED);
    }
}

/* ------------------------------------------------------------------------- *
 * SFWSERVICE_SIGNALS
 * ------------------------------------------------------------------------- */

static gulong
sfwservice_add_handler(SfwService *self, SfwServiceSignal signo,
                       SfwServiceHandler handler, gpointer aptr)
{
    gulong id = 0;
    if( self && handler )
        id = g_signal_connect(self, sfwservice_signal_name[signo],
                              G_CALLBACK(handler), aptr);
    sfwservice_log_debug("self=%p sig=%s id=%lu", self,
                         sfwservice_signal_name[signo], id);
    return id;
}

gulong
sfwservice_add_valid_changed_handler(SfwService *self, SfwServiceHandler handler,
                                     gpointer aptr)
{
    return sfwservice_add_handler(self, SFWSERVICE_SIGNAL_VALID_CHANGED,
                                  handler, aptr);
}

void
sfwservice_remove_handler(SfwService *self, gulong id)
{
    if( self && id ) {
        sfwservice_log_debug("self=%p id=%lu", self, id);
        g_signal_handler_disconnect(self, id);
    }
}

void
sfwservice_remove_handler_at(SfwService *self, gulong *pid)
{
    sfwservice_remove_handler(self, *pid), *pid = 0;
}

static void
sfwservice_emit_signal(SfwService *self, SfwServiceSignal signo)
{
    sfwservice_log_info("sig=%s id=%u",
                        sfwservice_signal_name[signo],
                        sfwservice_signal_id[signo]);
    g_signal_emit(self, sfwservice_signal_id[signo], 0);
}

/* ------------------------------------------------------------------------- *
 * SFWSERVICE_ACCESSORS
 * ------------------------------------------------------------------------- */

static SfwServicePrivate *
sfwservice_priv(const SfwService *self)
{
    SfwServicePrivate *priv = NULL;
    if( self )
        priv = sfwservice_get_instance_private((SfwService *)self);
    return priv;
}

/* ------------------------------------------------------------------------- *
 * SFWSERVICE_CONNECTION
 * ------------------------------------------------------------------------- */

GDBusConnection *
sfwservice_get_connection(const SfwService *self)
{
    SfwServicePrivate *priv = sfwservice_priv(self);
    return priv ? priv->srv_connection : NULL;
}

static void
sfwservice_set_connection(SfwService *self, GDBusConnection *con)
{
    SfwServicePrivate *priv = sfwservice_priv(self);
    if( priv->srv_connection != con ) {
        /* Actions on disconnect */
        if( priv->srv_connection ) {
            sfwservice_unwatch_name_owner(self);
            g_object_unref(priv->srv_connection),
                priv->srv_connection = NULL;
        }

        /* Actions on connect */
        if( con ) {
            priv->srv_connection = g_object_ref(con);
            sfwservice_watch_name_owner(self);
        }
    }
}

/* ------------------------------------------------------------------------- *
 * SFWSERVICE_NAME_OWNER
 * ------------------------------------------------------------------------- */

static void
sfwservice_set_name_owner(SfwService *self, const gchar *name_owner)
{
    SfwServicePrivate *priv = sfwservice_priv(self);
    if( name_owner && !*name_owner )
        name_owner = NULL;

    if( g_strcmp0(priv->srv_name_owner, name_owner) ) {
        sfwservice_log_info("name owner: %s -> %s",
                            priv->srv_name_owner ?: "null",
                            name_owner           ?: "null");
        g_free(priv->srv_name_owner),
            priv->srv_name_owner = g_strdup(name_owner);

        if( priv->srv_name_owner )
            sfwservice_stm_set_state(self, SFWSERVICESTATE_ENUMERATING);
        else
            sfwservice_stm_set_state(self, SFWSERVICESTATE_DISABLED);
    }
}

static void
sfwservice_name_owner_vanished_cb(GDBusConnection *connection,
                                  const gchar *name,
                                  gpointer aptr)
{
    (void)connection;
    (void)name;

    SfwService *self = aptr;
    sfwservice_set_name_owner(self, NULL);
}

static void
sfwservice_name_owner_appeared_cb(GDBusConnection *connection,
                                  const gchar *name,
                                  const gchar *name_owner,
                                  gpointer aptr)
{
    (void)connection;
    (void)name;

    SfwService *self = aptr;
    sfwservice_set_name_owner(self, NULL);
    sfwservice_set_name_owner(self, name_owner);
}

static void
sfwservice_unwatch_name_owner(SfwService *self)
{
    SfwServicePrivate *priv = sfwservice_priv(self);
    if( priv->srv_name_watcher_id ) {
        sfwservice_log_info("delete watcher %d", priv->srv_name_watcher_id);
        g_bus_unwatch_name(priv->srv_name_watcher_id),
            priv->srv_name_watcher_id = 0;
    }

    sfwservice_set_name_owner(self, NULL);
}

static void
sfwservice_watch_name_owner(SfwService *self)
{
    SfwServicePrivate *priv = sfwservice_priv(self);
    sfwservice_unwatch_name_owner(self);
    priv->srv_name_watcher_id =
        g_bus_watch_name_on_connection(sfwservice_get_connection(self),
                                       SFWDBUS_SERVICE,
                                       G_BUS_NAME_WATCHER_FLAGS_NONE,
                                       sfwservice_name_owner_appeared_cb,
                                       sfwservice_name_owner_vanished_cb,
                                       self,
                                       NULL);
    sfwservice_log_info("create watcher %d", priv->srv_name_watcher_id);
}

/* ------------------------------------------------------------------------- *
 * SFWSERVICE_STM_STATE
 * ------------------------------------------------------------------------- */

static const char *
sfwservice_stm_state_name(const SfwService *self)
{
    return sfwservicestate_repr(sfwservice_stm_get_state(self));
}

static SfwServiceState
sfwservice_stm_get_state(const SfwService *self)
{
    SfwServicePrivate *priv = sfwservice_priv(self);
    return priv ? priv->srv_state : SFWSERVICESTATE_DISABLED;
}

static void
sfwservice_stm_set_state(SfwService *self, SfwServiceState state)
{
    SfwServicePrivate *priv = sfwservice_priv(self);
    if( priv->srv_state == SFWSERVICESTATE_FINAL ) {
        /* No way out */
    }
    else if( priv->srv_state != state ) {
        sfwservice_log_info("state: %s -> %s",
                            sfwservicestate_repr(priv->srv_state),
                            sfwservicestate_repr(state));

        sfwservice_stm_leave_state(self);
        priv->srv_state = state;
        sfwservice_stm_enter_state(self);

        sfwservice_stm_eval_state_later(self);
    }
}

static void
sfwservice_stm_enter_state(SfwService *self)
{
    switch( sfwservice_stm_get_state(self) ) {
    case SFWSERVICESTATE_INITIAL:
        break;
    case SFWSERVICESTATE_DISABLED:
        break;
    case SFWSERVICESTATE_ENUMERATING:
        sfwservice_stm_start_enumerate(self);
        break;
    case SFWSERVICESTATE_READY:
        sfwservice_set_valid(self, true);
        break;
    case SFWSERVICESTATE_FAILED:
        sfwservice_stm_start_retry_delay(self);
        break;
    case SFWSERVICESTATE_FINAL:
        sfwservice_stm_cancel_enumerate(self);
        sfwservice_stm_disconnect(self);
        break;
    default:
        abort();
    }
}

static void
sfwservice_stm_leave_state(SfwService *self)
{
    SfwServicePrivate *priv = sfwservice_priv(self);
    switch( sfwservice_stm_get_state(self) ) {
    case SFWSERVICESTATE_INITIAL:
        sfwservice_stm_connect(self);
        break;
    case SFWSERVICESTATE_DISABLED:
        break;
    case SFWSERVICESTATE_ENUMERATING:
        sfwservice_stm_cancel_enumerate(self);
        break;
    case SFWSERVICESTATE_READY:
        sfwservice_set_valid(self, false);
        break;
    case SFWSERVICESTATE_FAILED:
        sfwservice_stm_cancel_retry_delay(self);
        cancellable_cancel(&priv->srv_enumerate_cancellable);
        break;
    case SFWSERVICESTATE_FINAL:
        break;
    default:
        abort();
    }
}

static gboolean
sfwservice_stm_eval_state_cb(gpointer aptr)
{
    SfwService        *self = aptr;
    SfwServicePrivate *priv = sfwservice_priv(self);

    priv->srv_eval_state_id = 0;

    sfwservice_log_debug("eval state: %s", sfwservice_stm_state_name(self));

    switch( sfwservice_stm_get_state(self) ) {
    case SFWSERVICESTATE_INITIAL:
        break;
    case SFWSERVICESTATE_DISABLED:
        break;
    case SFWSERVICESTATE_ENUMERATING:
        if( !sfwservice_stm_pending_enumerate(self) )
            sfwservice_stm_set_state(self, SFWSERVICESTATE_READY);
        break;
    case SFWSERVICESTATE_READY:
        break;
    case SFWSERVICESTATE_FAILED:
        if( !sfwservice_stm_pending_retry_delay(self) )
            sfwservice_stm_set_state(self, SFWSERVICESTATE_ENUMERATING);
        break;
    case SFWSERVICESTATE_FINAL:
        break;
    default:
        abort();
    }

    return G_SOURCE_REMOVE;
}

static void
sfwservice_stm_cancel_eval_state(SfwService *self)
{
    SfwServicePrivate *priv = sfwservice_priv(self);
    if( priv->srv_eval_state_id ) {
        sfwservice_log_debug("cancel state eval");
        g_source_remove(priv->srv_eval_state_id),
            priv->srv_eval_state_id = 0;
    }
}

static void
sfwservice_stm_eval_state_later(SfwService *self)
{
    SfwServicePrivate *priv = sfwservice_priv(self);
    if( sfwservice_stm_get_state(self) == SFWSERVICESTATE_FINAL ) {
        sfwservice_stm_cancel_eval_state(self);
    }
    else if( !priv->srv_eval_state_id ) {
        priv->srv_eval_state_id = g_idle_add(sfwservice_stm_eval_state_cb, self);
        sfwservice_log_debug("schedule state eval");
    }
}

/* ------------------------------------------------------------------------- *
 * SFWSERVICE_STM_RETRY
 * ------------------------------------------------------------------------- */

static gboolean
sfwservice_stm_retry_delay_cb(gpointer aptr)
{
    SfwService        *self = aptr;
    SfwServicePrivate *priv = sfwservice_priv(self);
    sfwservice_log_debug("trigger retry");
    priv->srv_retry_delay_id = 0;
    sfwservice_stm_eval_state_later(self);
    return G_SOURCE_REMOVE;
}

static void
sfwservice_stm_start_retry_delay(SfwService *self)
{
    SfwServicePrivate *priv = sfwservice_priv(self);
    if( gutil_timeout_start(&priv->srv_retry_delay_id, 5000,
                            sfwservice_stm_retry_delay_cb, self) )
        sfwservice_log_debug("schedule retry");
}

static void
sfwservice_stm_cancel_retry_delay(SfwService *self)
{
    SfwServicePrivate *priv = sfwservice_priv(self);
    if( gutil_timeout_stop(&priv->srv_retry_delay_id) )
        sfwservice_log_debug("cancel retry");
}

static bool
sfwservice_stm_pending_retry_delay(const SfwService *self)
{
    SfwServicePrivate *priv = sfwservice_priv(self);
    return priv ? priv->srv_retry_delay_id : false;
}

/* ------------------------------------------------------------------------- *
 * SFWSERVICE_STM_CONNECTION
 * ------------------------------------------------------------------------- */

static void
sfwservice_stm_connect_cb(GObject *object, GAsyncResult *res, gpointer aptr)
{
    (void)object;

    SfwService        *self = aptr;
    SfwServicePrivate *priv = sfwservice_priv(self);
    GError            *err  = NULL;
    GDBusConnection   *con  = g_bus_get_finish(res, &err);

    if( !con )
        sfwservice_log_warning("systembus connect failed: %s",
                               error_message(err));

    if( cancellable_finish(&priv->srv_bus_get_cancellable) )
        sfwservice_set_connection(self, con);

    g_clear_error(&err);
    if( con )
        g_object_unref(con);
    sfwservice_unref(self);
}

static void
sfwservice_stm_connect(SfwService *self)
{
    SfwServicePrivate *priv = sfwservice_priv(self);
    sfwservice_stm_disconnect(self);
    cancellable_start(&priv->srv_bus_get_cancellable);
    g_bus_get(G_BUS_TYPE_SYSTEM,
              priv->srv_bus_get_cancellable,
              sfwservice_stm_connect_cb,
              sfwservice_ref(self));
}
static void
sfwservice_stm_disconnect(SfwService *self)
{
    SfwServicePrivate *priv = sfwservice_priv(self);
    cancellable_cancel(&priv->srv_bus_get_cancellable);
    sfwservice_set_connection(self, NULL);
}

/* ------------------------------------------------------------------------- *
 * SFWSERVICE_STM_ENUMERATE
 * ------------------------------------------------------------------------- */

static void
sfwservice_stm_enumerate_cb(GObject *object, GAsyncResult *res, gpointer aptr)
{
    SfwService        *self = aptr;
    SfwServicePrivate *priv = sfwservice_priv(self);
    GDBusConnection   *con  = G_DBUS_CONNECTION(object);
    GError            *err  = NULL;
    GVariant          *rsp  = g_dbus_connection_call_finish(con, res, &err);

    if( !rsp )
        sfwservice_log_err("err: %s", error_message(err));

    if( cancellable_finish(&priv->srv_enumerate_cancellable) ) {
        g_hash_table_remove_all(priv->srv_available_sensors);
        if( rsp ) {
            gchar **sensors = NULL;
            g_variant_get(rsp, "(^as)", &sensors);
            if( sensors ) {
                for( gsize i = 0; sensors[i]; ++i ) {
                    const gchar *sensor = sensors[i];
                    sfwservice_log_info("sensor[%zd] = \"%s\"", i, sensor);
                    g_hash_table_add(priv->srv_available_sensors, g_strdup(sensor));
                }
                g_strfreev(sensors);
            }
        }
        sfwservice_stm_eval_state_later(self);
    }

    gutil_variant_unref(rsp);
    g_clear_error(&err);
    sfwservice_unref(self);
}

static void
sfwservice_stm_start_enumerate(SfwService *self)
{
    SfwServicePrivate *priv       = sfwservice_priv(self);
    GDBusConnection   *connection = sfwservice_get_connection(self);
    cancellable_start(&priv->srv_enumerate_cancellable);
    g_dbus_connection_call(connection,
                           SFWDBUS_SERVICE,
                           SFWDBUS_MANAGER_OBJECT,
                           SFWDBUS_MANAGER_INTEFCACE,
                           SFWDBUS_MANAGER_METHOD_AVAILABLE_PLUGINS,
                           NULL,
                           NULL,
                           G_DBUS_CALL_FLAGS_NO_AUTO_START,
                           -1,
                           priv->srv_enumerate_cancellable,
                           sfwservice_stm_enumerate_cb,
                           sfwservice_ref(self));
}

static void
sfwservice_stm_cancel_enumerate(SfwService *self)
{
    SfwServicePrivate *priv = sfwservice_priv(self);
    cancellable_cancel(&priv->srv_enumerate_cancellable);
}

static bool
sfwservice_stm_pending_enumerate(SfwService *self)
{
    SfwServicePrivate *priv = sfwservice_priv(self);
    return priv ? priv->srv_enumerate_cancellable : false;
}
