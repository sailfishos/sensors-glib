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

#include "sfwplugin.h"

#include "sfwservice.h"
#include "sfwlogging.h"
#include "sfwdbus.h"
#include "utility.h"

/* ========================================================================= *
 * Types
 * ========================================================================= */

typedef enum SfwPluginState
{
    SFWPLUGINSTATE_INITIAL,
    SFWPLUGINSTATE_DISABLED,
    SFWPLUGINSTATE_LOADING,
    SFWPLUGINSTATE_READY,
    SFWPLUGINSTATE_FAILED,
    SFWPLUGINSTATE_FINAL,
    SFWPLUGINSTATE_COUNT
} SfwPluginState;

typedef struct SfwPluginPrivate
{
    SfwService     *plg_service;
    gulong          plg_service_changed_id;
    SfwSensorId     plg_id;
    bool            plg_valid;
    SfwPluginState  plg_state;
    guint           plg_eval_state_id;
    GCancellable   *plg_load_cancellable;
    bool            plg_load_succeeded;
    guint           plg_retry_delay_id;
} SfwPluginPrivate;

struct SfwPlugin
{
    GObject object;
};

typedef enum SfwPluginSignal
{
    SFWPLUGIN_SIGNAL_VALID_CHANGED,
    SFWPLUGIN_SIGNAL_COUNT,
} SfwPluginSignal;

typedef GObjectClass SfwPluginClass;

/* ========================================================================= *
 * Macros
 * ========================================================================= */

# define sfwplugin_log_emit(LEV, FMT, ARGS...)\
     sfwlog_emit(LEV, "sfplugin(%s): " FMT, sfwplugin_name(self), ##ARGS)

# define sfwplugin_log_crit(   FMT, ARGS...) sfwplugin_log_emit(SFWLOG_CRIT,    FMT, ##ARGS)
# define sfwplugin_log_err(    FMT, ARGS...) sfwplugin_log_emit(SFWLOG_ERR,     FMT, ##ARGS)
# define sfwplugin_log_warning(FMT, ARGS...) sfwplugin_log_emit(SFWLOG_WARNING, FMT, ##ARGS)
# define sfwplugin_log_notice( FMT, ARGS...) sfwplugin_log_emit(SFWLOG_NOTICE,  FMT, ##ARGS)
# define sfwplugin_log_info(   FMT, ARGS...) sfwplugin_log_emit(SFWLOG_INFO,    FMT, ##ARGS)
# define sfwplugin_log_debug(  FMT, ARGS...) sfwplugin_log_emit(SFWLOG_DEBUG,   FMT, ##ARGS)
# define sfwplugin_log_trace(  FMT, ARGS...) sfwplugin_log_emit(SFWLOG_TRACE,   FMT, ##ARGS)

/* ========================================================================= *
 * Prototypes
 * ========================================================================= */

/* ------------------------------------------------------------------------- *
 * SFWPLUGIN_CLASS
 * ------------------------------------------------------------------------- */

static void sfwplugin_class_intern_init(gpointer klass);
static void sfwplugin_class_init       (SfwPluginClass *klass);

/* ------------------------------------------------------------------------- *
 * SFWPLUGIN_TYPE
 * ------------------------------------------------------------------------- */

GType        sfwplugin_get_type     (void);
static GType sfwplugin_get_type_once(void);

/* ------------------------------------------------------------------------- *
 * SFWPLUGINSTATE
 * ------------------------------------------------------------------------- */

static const char *sfwpluginstate_repr(SfwPluginState state);

/* ------------------------------------------------------------------------- *
 * SFWPLUGIN_LIFECYCLE
 * ------------------------------------------------------------------------- */

static void       sfwplugin_init    (SfwPlugin *self);
static void       sfwplugin_finalize(GObject *object);
static SfwPlugin *sfwplugin_new     (SfwSensorId id);
SfwPlugin        *sfwplugin_instance(SfwSensorId id);
SfwPlugin        *sfwplugin_ref     (SfwPlugin *self);
void              sfwplugin_unref   (SfwPlugin *self);
void              sfwplugin_unref_at(SfwPlugin **pself);

/* ------------------------------------------------------------------------- *
 * SFWPLUGIN_VALID
 * ------------------------------------------------------------------------- */

bool        sfwplugin_is_valid (const SfwPlugin *self);
static void sfwplugin_set_valid(SfwPlugin *self, bool valid);

/* ------------------------------------------------------------------------- *
 * SFWPLUGIN_SIGNALS
 * ------------------------------------------------------------------------- */

static gulong sfwplugin_add_handler              (SfwPlugin *self, SfwPluginSignal signo, SfwPluginHandler handler, gpointer aptr);
gulong        sfwplugin_add_valid_changed_handler(SfwPlugin *self, SfwPluginHandler handler, gpointer aptr);
void          sfwplugin_remove_handler           (SfwPlugin *self, gulong id);
void          sfwplugin_remove_handler_at        (SfwPlugin *self, gulong *pid);
static void   sfwplugin_emit_signal              (SfwPlugin *self, SfwPluginSignal signo);

/* ------------------------------------------------------------------------- *
 * SFWPLUGIN_ACCESSORS
 * ------------------------------------------------------------------------- */

static SfwPluginPrivate *sfwplugin_priv      (const SfwPlugin *self);
SfwSensorId              sfwplugin_id        (const SfwPlugin *self);
SfwService              *sfwplugin_service   (const SfwPlugin *self);
const char              *sfwplugin_name      (const SfwPlugin *self);
const char              *sfwplugin_object    (const SfwPlugin *self);
const char              *sfwplugin_interface (const SfwPlugin *self);
static GDBusConnection  *sfwplugin_connection(const SfwPlugin *self);

/* ------------------------------------------------------------------------- *
 * SFWPLUGIN_SERVICE
 * ------------------------------------------------------------------------- */

static void sfwplugin_service_changed_cb (SfwService *sfwservice, gpointer aptr);
static void sfwplugin_detach_from_service(SfwPlugin *self);
static void sfwplugin_attach_to_service  (SfwPlugin *self);

/* ------------------------------------------------------------------------- *
 * SFWPLUGIN_STM_STATE
 * ------------------------------------------------------------------------- */

static const char     *sfwplugin_stm_state_name       (const SfwPlugin *self);
static SfwPluginState  sfwplugin_stm_get_state        (const SfwPlugin *self);
static void            sfwplugin_stm_set_state        (SfwPlugin *self, SfwPluginState state);
static void            sfwplugin_stm_enter_state      (SfwPlugin *self);
static void            sfwplugin_stm_leave_state      (SfwPlugin *self);
static gboolean        sfwplugin_stm_eval_state_cb    (gpointer aptr);
static void            sfwplugin_stm_cancel_eval_state(SfwPlugin *self);
static void            sfwplugin_stm_eval_state_later (SfwPlugin *self);
static void            sfwplugin_stm_reset_state      (SfwPlugin *self);

/* ------------------------------------------------------------------------- *
 * SFWPLUGIN_STM_RETRY
 * ------------------------------------------------------------------------- */

static gboolean sfwplugin_stm_retry_delay_cb     (gpointer aptr);
static void     sfwplugin_stm_start_retry_delay  (SfwPlugin *self);
static void     sfwplugin_stm_cancel_retry_delay (SfwPlugin *self);
static bool     sfwplugin_stm_pending_retry_delay(const SfwPlugin *self);

/* ------------------------------------------------------------------------- *
 * SFWPLUGIN_STM_LOAD
 * ------------------------------------------------------------------------- */

static void sfwplugin_stm_load_cb     (GObject *object, GAsyncResult *res, gpointer aptr);
static void sfwplugin_stm_start_load  (SfwPlugin *self);
static void sfwplugin_stm_cancel_load (SfwPlugin *self);
static bool sfwplugin_stm_pending_load(SfwPlugin *self);

/* ========================================================================= *
 * SFWPLUGIN_CLASS
 * ========================================================================= */

G_DEFINE_TYPE_WITH_PRIVATE(SfwPlugin, sfwplugin, G_TYPE_OBJECT)
#define SFWPLUGIN_TYPE (sfwplugin_get_type())
#define SFWPLUGIN(obj) (G_TYPE_CHECK_INSTANCE_CAST(obj, SFWPLUGIN_TYPE, SfwPlugin))

static const char * const sfwplugin_signal_name[SFWPLUGIN_SIGNAL_COUNT] =
{
    [SFWPLUGIN_SIGNAL_VALID_CHANGED] = "sfwplugin-valid-changed",
};

static guint sfwplugin_signal_id[SFWPLUGIN_SIGNAL_COUNT] = { };

static void
sfwplugin_class_init(SfwPluginClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS(klass);

    object_class->finalize = sfwplugin_finalize;

    for( guint signo = 0; signo < SFWPLUGIN_SIGNAL_COUNT; ++signo )
        sfwplugin_signal_id[signo] = g_signal_new(sfwplugin_signal_name[signo],
                                                  G_OBJECT_CLASS_TYPE(klass),
                                                  G_SIGNAL_RUN_FIRST,
                                                  0, NULL, NULL, NULL,
                                                  G_TYPE_NONE, 0);
}

/* ========================================================================= *
 * SFWPLUGINSTATE
 * ========================================================================= */

static const char *
sfwpluginstate_repr(SfwPluginState state)
{
    const char *repr = "SFWPLUGINSTATE_INVALID";
    switch( state ) {
    case SFWPLUGINSTATE_INITIAL:  repr = "SFWPLUGINSTATE_INITIAL";  break;
    case SFWPLUGINSTATE_DISABLED: repr = "SFWPLUGINSTATE_DISABLED"; break;
    case SFWPLUGINSTATE_LOADING:  repr = "SFWPLUGINSTATE_LOADING";  break;
    case SFWPLUGINSTATE_READY:    repr = "SFWPLUGINSTATE_READY";    break;
    case SFWPLUGINSTATE_FAILED:   repr = "SFWPLUGINSTATE_FAILED";   break;
    case SFWPLUGINSTATE_FINAL:    repr = "SFWPLUGINSTATE_FINAL";    break;
    default: break;
    }
    return repr;
}

/* ========================================================================= *
 * SFWPLUGIN
 * ========================================================================= */

/* ------------------------------------------------------------------------- *
 * SFWPLUGIN_LIFECYCLE
 * ------------------------------------------------------------------------- */

static void
sfwplugin_init(SfwPlugin *self)
{
    SfwPluginPrivate *priv = sfwplugin_priv(self);

    priv->plg_service            = NULL;
    priv->plg_service_changed_id = 0;
    priv->plg_id                 = SFW_SENSOR_ID_INVALID;
    priv->plg_valid              = false;
    priv->plg_state              = SFWPLUGINSTATE_INITIAL;
    priv->plg_eval_state_id      = 0;
    priv->plg_load_cancellable   = NULL;
    priv->plg_load_succeeded     = false;
    priv->plg_retry_delay_id     = 0;
}

static void
sfwplugin_finalize(GObject *object)
{
    SfwPlugin *self = SFWPLUGIN(object);
    sfwplugin_log_info("DELETED");

    sfwplugin_stm_set_state(self, SFWPLUGINSTATE_FINAL);
    sfwplugin_detach_from_service(self);

    G_OBJECT_CLASS(sfwplugin_parent_class)->finalize(object);
}

static SfwPlugin *
sfwplugin_new(SfwSensorId id)
{
    SfwPlugin        *self = g_object_new(SFWPLUGIN_TYPE, NULL);
    SfwPluginPrivate *priv = sfwplugin_priv(self);

    priv->plg_id      = id;

    sfwplugin_attach_to_service(self);

    sfwplugin_log_info("CREATED");

    return self;
}

SfwPlugin *
sfwplugin_instance(SfwSensorId id)
{
    static SfwPlugin *instance[SFW_SENSOR_ID_COUNT] = { };

    SfwPlugin *plugin = NULL;
    if( sfwsensorid_is_valid(id) ) {
        if( !(plugin = sfwplugin_ref(instance[id])) ) {
            plugin = instance[id] = sfwplugin_new(id);
            g_object_add_weak_pointer(G_OBJECT(plugin),
                                      (gpointer*)(&instance[id]));
        }
    }
    return plugin;
}

SfwPlugin *
sfwplugin_ref(SfwPlugin *self)
{
    if( self ) {
        sfwplugin_log_debug("self=%p", self);
        g_object_ref(SFWPLUGIN(self));
    }
    return self;
}

void
sfwplugin_unref(SfwPlugin *self)
{
    if( self ) {
        sfwplugin_log_debug("self=%p", self);
        g_object_unref(SFWPLUGIN(self));
    }
}

void
sfwplugin_unref_at(SfwPlugin **pself)
{
    sfwplugin_unref(*pself), *pself = NULL;
}

/* ------------------------------------------------------------------------- *
 * SFWPLUGIN_VALID
 * ------------------------------------------------------------------------- */

bool
sfwplugin_is_valid(const SfwPlugin *self)
{
    SfwPluginPrivate *priv = sfwplugin_priv(self);
    return priv ? priv->plg_valid : false;
}

static void
sfwplugin_set_valid(SfwPlugin *self, bool valid)
{
    SfwPluginPrivate *priv = sfwplugin_priv(self);
    if( priv && priv->plg_valid != valid ) {
        sfwplugin_log_info("valid: %s -> %s",
                           priv->plg_valid ? "true" : "false",
                           valid           ? "true" : "false");
        priv->plg_valid = valid;
        sfwplugin_emit_signal(self, SFWPLUGIN_SIGNAL_VALID_CHANGED);
    }
}

/* ------------------------------------------------------------------------- *
 * SFWPLUGIN_SIGNALS
 * ------------------------------------------------------------------------- */

static gulong
sfwplugin_add_handler(SfwPlugin *self, SfwPluginSignal signo,
                      SfwPluginHandler handler, gpointer aptr)
{
    gulong id = 0;
    if( self && handler )
        id = g_signal_connect(self, sfwplugin_signal_name[signo],
                              G_CALLBACK(handler), aptr);
    sfwplugin_log_debug("sig=%s id=%lu",  sfwplugin_signal_name[signo], id);
    return id;
}

gulong
sfwplugin_add_valid_changed_handler(SfwPlugin *self, SfwPluginHandler handler,
                                    gpointer aptr)
{
    return sfwplugin_add_handler(self, SFWPLUGIN_SIGNAL_VALID_CHANGED,
                                 handler, aptr);
}

void
sfwplugin_remove_handler(SfwPlugin *self, gulong id)
{
    if( self && id ) {
        sfwplugin_log_debug("id=%lu", id);
        g_signal_handler_disconnect(self, id);
    }
}

void
sfwplugin_remove_handler_at(SfwPlugin *self, gulong *pid)
{
    sfwplugin_remove_handler(self, *pid), *pid = 0;
}

static void
sfwplugin_emit_signal(SfwPlugin *self, SfwPluginSignal signo)
{
    sfwplugin_log_info("sig=%s id=%u",
                       sfwplugin_signal_name[signo],
                       sfwplugin_signal_id[signo]);
    g_signal_emit(self, sfwplugin_signal_id[signo], 0);
}

/* ------------------------------------------------------------------------- *
 * SFWPLUGIN_ACCESSORS
 * ------------------------------------------------------------------------- */

static SfwPluginPrivate *
sfwplugin_priv(const SfwPlugin *self)
{
    SfwPluginPrivate *priv = NULL;
    if( self )
        priv = sfwplugin_get_instance_private((SfwPlugin *)self);
    return priv;
}

SfwSensorId
sfwplugin_id(const SfwPlugin *self)
{
    SfwPluginPrivate *priv = sfwplugin_priv(self);
    return priv ? priv->plg_id : SFW_SENSOR_ID_INVALID;
}

SfwService *
sfwplugin_service(const SfwPlugin *self)
{
    SfwPluginPrivate *priv = sfwplugin_priv(self);
    return priv ? priv->plg_service : NULL;
}

const char *
sfwplugin_name(const SfwPlugin *self)
{
    return sfwsensorid_name(sfwplugin_id(self));
}

const char *
sfwplugin_object(const SfwPlugin *self)
{
    return sfwsensorid_object(sfwplugin_id(self));
}

const char *
sfwplugin_interface(const SfwPlugin *self)
{
    return sfwsensorid_interface(sfwplugin_id(self));
}

static GDBusConnection *
sfwplugin_connection(const SfwPlugin *self)
{
    return sfwservice_get_connection(sfwplugin_service(self));
}

/* ------------------------------------------------------------------------- *
 * SFWPLUGIN_SERVICE
 * ------------------------------------------------------------------------- */

static void
sfwplugin_service_changed_cb(SfwService *sfwservice, gpointer aptr)
{
    (void)sfwservice;
    SfwPlugin *self = aptr;
    sfwplugin_stm_reset_state(self);
}

static void
sfwplugin_detach_from_service(SfwPlugin *self)
{
    SfwPluginPrivate *priv = sfwplugin_priv(self);
    sfwservice_remove_handler_at(priv->plg_service,
                                 &priv->plg_service_changed_id);
    sfwservice_unref_at(&priv->plg_service);
}

static void
sfwplugin_attach_to_service(SfwPlugin *self)
{
    SfwPluginPrivate *priv = sfwplugin_priv(self);
    sfwplugin_detach_from_service(self);
    priv->plg_service            = sfwservice_instance();
    priv->plg_service_changed_id =
        sfwservice_add_valid_changed_handler(priv->plg_service,
                                             sfwplugin_service_changed_cb,
                                             self);
    sfwplugin_stm_reset_state(self);
}

/* ------------------------------------------------------------------------- *
 * SFWPLUGIN_STM_STATE
 * ------------------------------------------------------------------------- */

static const char *
sfwplugin_stm_state_name(const SfwPlugin *self)
{
    return sfwpluginstate_repr(sfwplugin_stm_get_state(self));
}

static SfwPluginState
sfwplugin_stm_get_state(const SfwPlugin *self)
{
    SfwPluginPrivate *priv = sfwplugin_priv(self);
    return priv ? priv->plg_state : SFWPLUGINSTATE_DISABLED;
}

static void
sfwplugin_stm_set_state(SfwPlugin *self, SfwPluginState state)
{
    SfwPluginPrivate *priv = sfwplugin_priv(self);
    if( priv->plg_state == SFWPLUGINSTATE_FINAL ) {
        /* No way out */
    }
    else if( priv->plg_state != state ) {
        sfwplugin_log_info("state: %s -> %s",
                           sfwpluginstate_repr(priv->plg_state),
                           sfwpluginstate_repr(state));
        sfwplugin_stm_leave_state(self);
        priv->plg_state = state;
        sfwplugin_stm_enter_state(self);
        sfwplugin_stm_eval_state_later(self);
    }
}

static void
sfwplugin_stm_enter_state(SfwPlugin *self)
{
    switch( sfwplugin_stm_get_state(self) ) {
    case SFWPLUGINSTATE_INITIAL:
        break;
    case SFWPLUGINSTATE_DISABLED:
        break;
    case SFWPLUGINSTATE_LOADING:
        sfwplugin_stm_start_load(self);
        break;
    case SFWPLUGINSTATE_READY:
        sfwplugin_set_valid(self, true);
        break;
    case SFWPLUGINSTATE_FAILED:
        sfwplugin_stm_start_retry_delay(self);
        break;
    case SFWPLUGINSTATE_FINAL:
        break;
    default:
        abort();
    }
}

static void
sfwplugin_stm_leave_state(SfwPlugin *self)
{
    switch( sfwplugin_stm_get_state(self) ) {
    case SFWPLUGINSTATE_INITIAL:
        break;
    case SFWPLUGINSTATE_DISABLED:
        break;
    case SFWPLUGINSTATE_LOADING:
        sfwplugin_stm_cancel_load(self);
        break;
    case SFWPLUGINSTATE_READY:
        sfwplugin_set_valid(self, false);
        break;
    case SFWPLUGINSTATE_FAILED:
        sfwplugin_stm_cancel_retry_delay(self);
        break;
    case SFWPLUGINSTATE_FINAL:
        break;
    default:
        abort();
    }
}

static gboolean
sfwplugin_stm_eval_state_cb(gpointer aptr)
{
    SfwPlugin        *self = aptr;
    SfwPluginPrivate *priv = sfwplugin_priv(self);

    priv->plg_eval_state_id = 0;

    sfwplugin_log_debug("eval state: %s", sfwplugin_stm_state_name(self));

    switch( sfwplugin_stm_get_state(self) ) {
    case SFWPLUGINSTATE_INITIAL:
        break;
    case SFWPLUGINSTATE_DISABLED:
        if( sfwservice_is_valid(sfwplugin_service(self)) ) {
            sfwplugin_stm_set_state(self, SFWPLUGINSTATE_LOADING);
        }
        break;
    case SFWPLUGINSTATE_LOADING:
        if( sfwplugin_stm_pending_load(self) )
            break;
        if( priv->plg_load_succeeded )
            sfwplugin_stm_set_state(self, SFWPLUGINSTATE_READY );
        else
            sfwplugin_stm_set_state(self, SFWPLUGINSTATE_FAILED);
        break;
    case SFWPLUGINSTATE_READY:
        break;
    case SFWPLUGINSTATE_FAILED:
        if( !sfwplugin_stm_pending_retry_delay(self) )
            sfwplugin_stm_set_state(self, SFWPLUGINSTATE_LOADING);
        break;
    case SFWPLUGINSTATE_FINAL:
        break;
    default:
        abort();
    }

    return G_SOURCE_REMOVE;
}

static void
sfwplugin_stm_cancel_eval_state(SfwPlugin *self)
{
    SfwPluginPrivate *priv = sfwplugin_priv(self);
    if( priv->plg_eval_state_id ) {
        sfwplugin_log_debug("cancel state eval");
        g_source_remove(priv->plg_eval_state_id),
            priv->plg_eval_state_id = 0;
    }
}

static void
sfwplugin_stm_eval_state_later(SfwPlugin *self)
{
    SfwPluginPrivate *priv = sfwplugin_priv(self);
    if( sfwplugin_stm_get_state(self) == SFWPLUGINSTATE_FINAL ) {
        sfwplugin_stm_cancel_eval_state(self);
    }
    else if( !priv->plg_eval_state_id ) {
        priv->plg_eval_state_id = g_idle_add(sfwplugin_stm_eval_state_cb, self);
        sfwplugin_log_debug("schedule state eval");
    }
}

static void
sfwplugin_stm_reset_state(SfwPlugin *self)
{
    if( self ) {
        sfwplugin_stm_set_state(self, SFWPLUGINSTATE_DISABLED);
        sfwplugin_stm_eval_state_later(self);
    }
}

/* ------------------------------------------------------------------------- *
 * SFWPLUGIN_STM_RETRY
 * ------------------------------------------------------------------------- */

static gboolean
sfwplugin_stm_retry_delay_cb(gpointer aptr)
{
    SfwPlugin        *self = aptr;
    SfwPluginPrivate *priv = sfwplugin_priv(self);
    sfwplugin_log_debug("trigger retry");
    priv->plg_retry_delay_id = 0;
    sfwplugin_stm_eval_state_later(self);
    return G_SOURCE_REMOVE;
}

static void
sfwplugin_stm_start_retry_delay(SfwPlugin *self)
{
    SfwPluginPrivate *priv = sfwplugin_priv(self);
    if( gutil_timeout_start(&priv->plg_retry_delay_id, 5000,
                            sfwplugin_stm_retry_delay_cb, self) )
        sfwplugin_log_debug("schedule retry");
}

static void
sfwplugin_stm_cancel_retry_delay(SfwPlugin *self)
{
    SfwPluginPrivate *priv = sfwplugin_priv(self);
    if( gutil_timeout_stop(&priv->plg_retry_delay_id) )
        sfwplugin_log_debug("cancel retry");
}

static bool
sfwplugin_stm_pending_retry_delay(const SfwPlugin *self)
{
    SfwPluginPrivate *priv = sfwplugin_priv(self);
    return priv ? priv->plg_retry_delay_id : false;
}

/* ------------------------------------------------------------------------- *
 * SFWPLUGIN_STM_LOAD
 * ------------------------------------------------------------------------- */

static void
sfwplugin_stm_load_cb(GObject *object, GAsyncResult *res, gpointer aptr)
{
    SfwPlugin        *self = aptr;
    SfwPluginPrivate *priv = sfwplugin_priv(self);

    sfwplugin_log_debug("loaded");
    gboolean         ack = false;
    GDBusConnection *con = G_DBUS_CONNECTION(object);
    GError          *err = NULL;
    GVariant        *rsp = g_dbus_connection_call_finish(con, res, &err);

    if( !rsp )
        sfwplugin_log_err("err: %s", error_message(err));
    else
        g_variant_get(rsp, "(b)", &ack);

    if( cancellable_finish(&priv->plg_load_cancellable) ) {
        if( ack )
            priv->plg_load_succeeded = true;
        sfwplugin_stm_eval_state_later(self);
    }

    g_clear_error(&err);
    gutil_variant_unref(rsp);
    sfwplugin_unref(self);
}

static void
sfwplugin_stm_start_load(SfwPlugin *self)
{
    sfwplugin_log_debug("loading");
    SfwPluginPrivate *priv = sfwplugin_priv(self);
    const char       *name = sfwplugin_name(self);

    priv->plg_load_succeeded = false;
    cancellable_start(&priv->plg_load_cancellable);
    g_dbus_connection_call(sfwplugin_connection(self),
                           SFWDBUS_SERVICE,
                           SFWDBUS_MANAGER_OBJECT,
                           SFWDBUS_MANAGER_INTEFCACE,
                           SFWDBUS_MANAGER_METHOD_LOAD_PLUGIN,
                           g_variant_new("(s)", name),
                           NULL,
                           G_DBUS_CALL_FLAGS_NO_AUTO_START,
                           -1,
                           priv->plg_load_cancellable,
                           sfwplugin_stm_load_cb,
                           sfwplugin_ref(self));
}

static void
sfwplugin_stm_cancel_load(SfwPlugin *self)
{
    SfwPluginPrivate *priv = sfwplugin_priv(self);
    cancellable_cancel(&priv->plg_load_cancellable);
}

static bool
sfwplugin_stm_pending_load(SfwPlugin *self)
{
    SfwPluginPrivate *priv = sfwplugin_priv(self);
    return priv->plg_load_cancellable != NULL;
}
