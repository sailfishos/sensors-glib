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

#include "sfwsensor.h"

#include "sfwservice.h"
#include "sfwplugin.h"
#include "sfwreporting.h"
#include "sfwlogging.h"
#include "sfwdbus.h"
#include "utility.h"

#include <inttypes.h>

/* ========================================================================= *
 * Constants
 * ========================================================================= */

/** Connect path to sensord data unix domain socket  */
# define SENSORFW_DATA_SOCKET                   "/run/sensord.sock"

/** Placeholder session id value */
#define SESSION_ID_INVALID (-1)

/* ========================================================================= *
 * Types
 * ========================================================================= */

typedef enum SfwSensorState
{
    SFWSENSORSTATE_INITIAL,
    SFWSENSORSTATE_DISABLED,
    SFWSENSORSTATE_SESSION,
    SFWSENSORSTATE_PROPERTIES,
    SFWSENSORSTATE_CONNECT,
    SFWSENSORSTATE_READY,
    SFWSENSORSTATE_FAILED,
    SFWSENSORSTATE_FINAL,
    SFWSENSORSTATE_COUNT
} SfwSensorState;

typedef struct SfwSensorPrivate
{
    SfwPlugin      *sns_plugin;
    gulong          sns_plugin_changed_id;
    bool            sns_valid;
    bool            sns_active;
    SfwSensorState  sns_state;
    int             sns_session_id;
    guint           sns_eval_state_id;
    GCancellable   *sns_get_properties_cancellable;
    GCancellable   *sns_request_session_cancellable;
    GCancellable   *sns_release_session_cancellable;
    guint           sns_retry_delay_id;
    GHashTable     *sns_properties;
    int             sns_socket_fd;
    guint           sns_socket_tx_id;
    GIOFunc         sns_socket_tx_cb;
    guint           sns_socket_rx_id;
    GIOFunc         sns_socket_rx_cb;
    SfwReporting   *sns_reporting;
    SfwReading      sns_reading;
    gulong          sns_reporting_active_changed_id;
} SfwSensorPrivate;

struct SfwSensor
{
    GObject object;
};

typedef enum SfwSensorSignal
{
    SFWSENSOR_SIGNAL_VALID_CHANGED,
    SFWSENSOR_SIGNAL_READING_CHANGED,
    SFWSENSOR_SIGNAL_ACTIVE_CHANGED,
    SFWSENSOR_SIGNAL_COUNT,
} SfwSensorSignal;

typedef GObjectClass SfwSensorClass;

/* ========================================================================= *
 * Macros
 * ========================================================================= */

# define sfwsensor_log_emit(LEV, FMT, ARGS...)\
     sfwlog_emit(LEV, "sfsensor(%s): " FMT, sfwsensor_name(self), ##ARGS)

# define sfwsensor_log_crit(   FMT, ARGS...) sfwsensor_log_emit(SFWLOG_CRIT,    FMT, ##ARGS)
# define sfwsensor_log_err(    FMT, ARGS...) sfwsensor_log_emit(SFWLOG_ERR,     FMT, ##ARGS)
# define sfwsensor_log_warning(FMT, ARGS...) sfwsensor_log_emit(SFWLOG_WARNING, FMT, ##ARGS)
# define sfwsensor_log_notice( FMT, ARGS...) sfwsensor_log_emit(SFWLOG_NOTICE,  FMT, ##ARGS)
# define sfwsensor_log_info(   FMT, ARGS...) sfwsensor_log_emit(SFWLOG_INFO,    FMT, ##ARGS)
# define sfwsensor_log_debug(  FMT, ARGS...) sfwsensor_log_emit(SFWLOG_DEBUG,   FMT, ##ARGS)
# define sfwsensor_log_trace(  FMT, ARGS...) sfwsensor_log_emit(SFWLOG_TRACE,   FMT, ##ARGS)

/* ========================================================================= *
 * Prototypes
 * ========================================================================= */

/* ------------------------------------------------------------------------- *
 * SFWSENSOR_CLASS
 * ------------------------------------------------------------------------- */

static void sfwsensor_class_intern_init(gpointer klass);
static void sfwsensor_class_init       (SfwSensorClass *klass);

/* ------------------------------------------------------------------------- *
 * SFWSENSOR_TYPE
 * ------------------------------------------------------------------------- */

GType        sfwsensor_get_type     (void);
static GType sfwsensor_get_type_once(void);

/* ------------------------------------------------------------------------- *
 * SFWSENSORSTATE
 * ------------------------------------------------------------------------- */

static const char *sfwsensorstate_repr(SfwSensorState state);

/* ------------------------------------------------------------------------- *
 * SFWSENSOR_LIFECYCLE
 * ------------------------------------------------------------------------- */

static void  sfwsensor_init    (SfwSensor *self);
static void  sfwsensor_finalize(GObject *object);
SfwSensor   *sfwsensor_new     (SfwSensorId id);
SfwSensor   *sfwsensor_ref     (SfwSensor *self);
void         sfwsensor_unref   (SfwSensor *self);
void         sfwsensor_unref_cb(gpointer self);
void         sfwsensor_unref_at(SfwSensor **pself);

/* ------------------------------------------------------------------------- *
 * SFWSENSOR_CONTROL
 * ------------------------------------------------------------------------- */

void sfwsensor_start       (SfwSensor *self);
void sfwsensor_stop        (SfwSensor *self);
void sfwsensor_set_datarate(SfwSensor *self, double datarate_hz);
void sfwsensor_set_alwayson(SfwSensor *self, bool alwayson);

/* ------------------------------------------------------------------------- *
 * SFWSENSOR_VALID
 * ------------------------------------------------------------------------- */

bool        sfwsensor_is_valid (const SfwSensor *self);
static void sfwsensor_set_valid(SfwSensor *self, bool valid);

/* ------------------------------------------------------------------------- *
 * SFWSENSOR_ACTIVE
 * ------------------------------------------------------------------------- */

bool        sfwsensor_is_active                  (const SfwSensor *self);
static void sfwsensor_eval_active                (SfwSensor *self);
static void sfwsensor_reporting_active_changed_cb(SfwReporting *sfwreporting, gpointer aptr);

/* ------------------------------------------------------------------------- *
 * SFWSENSOR_READING
 * ------------------------------------------------------------------------- */

SfwReading *sfwsensor_reading(SfwSensor *self);

/* ------------------------------------------------------------------------- *
 * SFWSENSOR_SIGNALS
 * ------------------------------------------------------------------------- */

static gulong sfwsensor_add_handler                (SfwSensor *self, SfwSensorSignal signo, SfwSensorHandler handler, gpointer aptr);
gulong        sfwsensor_add_valid_changed_handler  (SfwSensor *self, SfwSensorHandler handler, gpointer aptr);
gulong        sfwsensor_add_active_changed_handler (SfwSensor *self, SfwSensorHandler handler, gpointer aptr);
gulong        sfwsensor_add_reading_changed_handler(SfwSensor *self, SfwSensorHandler handler, gpointer aptr);
void          sfwsensor_remove_handler             (SfwSensor *self, gulong id);
void          sfwsensor_remove_handler_at          (SfwSensor *self, gulong *pid);
static void   sfwsensor_emit_signal                (SfwSensor *self, SfwSensorSignal signo);

/* ------------------------------------------------------------------------- *
 * SFWSENSOR_ACCESSORS
 * ------------------------------------------------------------------------- */

static SfwSensorPrivate *sfwsensor_priv      (const SfwSensor *self);
int                      sfwsensor_session_id(const SfwSensor *self);
SfwPlugin               *sfwsensor_plugin    (const SfwSensor *self);
SfwService              *sfwsensor_service   (const SfwSensor *self);
const char              *sfwsensor_name      (const SfwSensor *self);
const char              *sfwsensor_object    (const SfwSensor *self);
const char              *sfwsensor_interface (const SfwSensor *self);

/* ------------------------------------------------------------------------- *
 * SFWSENSOR_PLUGIN
 * ------------------------------------------------------------------------- */

static void sfwsensor_plugin_changed_cb (SfwPlugin *sfwplugin, gpointer aptr);
static void sfwsensor_detach_from_plugin(SfwSensor *self);
static void sfwsensor_attach_to_plugin  (SfwSensor *self, SfwSensorId id);

/* ------------------------------------------------------------------------- *
 * SFWSENSOR_STM_STATE
 * ------------------------------------------------------------------------- */

static const char     *sfwsensor_stm_state_name       (const SfwSensor *self);
static SfwSensorState  sfwsensor_stm_get_state        (const SfwSensor *self);
static void            sfwsensor_stm_set_state        (SfwSensor *self, SfwSensorState state);
static void            sfwsensor_stm_enter_state      (SfwSensor *self);
static void            sfwsensor_stm_leave_state      (SfwSensor *self);
static gboolean        sfwsensor_stm_eval_state_cb    (gpointer aptr);
static void            sfwsensor_stm_cancel_eval_state(SfwSensor *self);
static void            sfwsensor_stm_eval_state_later (SfwSensor *self);
static void            sfwsensor_stm_reset_state      (SfwSensor *self);

/* ------------------------------------------------------------------------- *
 * SFWSENSOR_STM_RETRY
 * ------------------------------------------------------------------------- */

static gboolean sfwsensor_stm_retry_delay_cb     (gpointer aptr);
static void     sfwsensor_stm_start_retry_delay  (SfwSensor *self);
static void     sfwsensor_stm_cancel_retry_delay (SfwSensor *self);
static bool     sfwsensor_stm_pending_retry_delay(const SfwSensor *self);

/* ------------------------------------------------------------------------- *
 * SFWSENSOR_STM_SESSION
 * ------------------------------------------------------------------------- */

static void sfwsensor_stm_request_session_cb     (GObject *object, GAsyncResult *res, gpointer aptr);
static void sfwsensor_stm_start_request_session  (SfwSensor *self);
static void sfwsensor_stm_cancel_request_session (SfwSensor *self);
static bool sfwsensor_stm_pending_request_session(const SfwSensor *self);
static void sfwsensor_stm_release_session_cb     (GObject *object, GAsyncResult *res, gpointer aptr);
static void sfwsensor_stm_start_release_session  (SfwSensor *self);
static void sfwsensor_stm_cancel_release_session (SfwSensor *self);
static bool sfwsensor_stm_pending_release_session(const SfwSensor *self);

/* ------------------------------------------------------------------------- *
 * SFWSENSOR_STM_PROPERTIES
 * ------------------------------------------------------------------------- */

static void sfwsensor_stm_update_property       (SfwSensor *self, const char *key, GVariant *val);
static void sfwsensor_stm_get_properties_cb     (GObject *object, GAsyncResult *res, gpointer aptr);
static void sfwsensor_stm_start_get_properties  (SfwSensor *self);
static void sfwsensor_stm_cancel_get_properties (SfwSensor *self);
static bool sfwsensor_stm_pending_get_properties(const SfwSensor *self);

/* ------------------------------------------------------------------------- *
 * SFWSENSOR_STM_SOCKET
 * ------------------------------------------------------------------------- */

static gboolean sfwsensor_stm_socket_rx_unexpected    (GIOChannel *chn, GIOCondition cnd, gpointer aptr);
static gboolean sfwsensor_stm_socket_rx_handshake     (GIOChannel *chn, GIOCondition cnd, gpointer aptr);
static gboolean sfwsensor_stm_socket_rx_reading       (GIOChannel *chn, GIOCondition cnd, gpointer aptr);
static gboolean sfwsensor_stm_socket_tx_unexpected    (GIOChannel *chn, GIOCondition cnd, gpointer aptr);
static gboolean sfwsensor_stm_socket_tx_handshake     (GIOChannel *chn, GIOCondition cnd, gpointer aptr);
static gboolean sfwsensor_stm_socket_tx_cb            (GIOChannel *chn, GIOCondition cnd, gpointer aptr);
static gboolean sfwsensor_stm_socket_rx_cb            (GIOChannel *chn, GIOCondition cnd, gpointer aptr);
static bool     sfwsensor_stm_socket_connect          (SfwSensor *self);
static void     sfwsensor_stm_socket_disconnect       (SfwSensor *self);
static bool     sfwsensor_stm_pending_socket_handshake(const SfwSensor *self);
static bool     sfwsensor_stm_socket_ready_to_receive (const SfwSensor *self);

/* ========================================================================= *
 * SFWSENSOR_CLASS
 * ========================================================================= */

G_DEFINE_TYPE_WITH_PRIVATE(SfwSensor, sfwsensor, G_TYPE_OBJECT)
#define SFWSENSOR_TYPE (sfwsensor_get_type())
#define SFWSENSOR(obj) (G_TYPE_CHECK_INSTANCE_CAST(obj, SFWSENSOR_TYPE, SfwSensor))

static const char * const sfwsensor_signal_name[SFWSENSOR_SIGNAL_COUNT] =
{
    [SFWSENSOR_SIGNAL_VALID_CHANGED]   = "sfwsensor-valid-changed",
    [SFWSENSOR_SIGNAL_READING_CHANGED] = "sfwsensor-reading-changed",
    [SFWSENSOR_SIGNAL_ACTIVE_CHANGED]  = "sfwsensor-active-changed",
};

static guint sfwsensor_signal_id[SFWSENSOR_SIGNAL_COUNT] = { };

static void
sfwsensor_class_init(SfwSensorClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS(klass);

    object_class->finalize = sfwsensor_finalize;

    for( guint signo = 0; signo < SFWSENSOR_SIGNAL_COUNT; ++signo )
        sfwsensor_signal_id[signo] = g_signal_new(sfwsensor_signal_name[signo],
                                                  G_OBJECT_CLASS_TYPE(klass),
                                                  G_SIGNAL_RUN_FIRST,
                                                  0, NULL, NULL, NULL,
                                                  G_TYPE_NONE, 0);
}

/* ========================================================================= *
 * SFWSENSORSTATE
 * ========================================================================= */

static const char *
sfwsensorstate_repr(SfwSensorState state)
{
    const char *repr = "SFWSENSORSTATE_INVALID";
    switch( state ) {
    case SFWSENSORSTATE_INITIAL:    repr = "SFWSENSORSTATE_INITIAL";    break;
    case SFWSENSORSTATE_DISABLED:   repr = "SFWSENSORSTATE_DISABLED";   break;
    case SFWSENSORSTATE_SESSION:    repr = "SFWSENSORSTATE_SESSION";    break;
    case SFWSENSORSTATE_PROPERTIES: repr = "SFWSENSORSTATE_PROPERTIES"; break;
    case SFWSENSORSTATE_CONNECT:    repr = "SFWSENSORSTATE_CONNECT";    break;
    case SFWSENSORSTATE_READY:      repr = "SFWSENSORSTATE_READY";      break;
    case SFWSENSORSTATE_FAILED:     repr = "SFWSENSORSTATE_FAILED";     break;
    case SFWSENSORSTATE_FINAL:      repr = "SFWSENSORSTATE_FINAL";      break;
    default: break;
    }
    return repr;
}

/* ========================================================================= *
 * SFWSENSOR
 * ========================================================================= */

/* ------------------------------------------------------------------------- *
 * SFWSENSOR_LIFECYCLE
 * ------------------------------------------------------------------------- */

static void
sfwsensor_init(SfwSensor *self)
{
    SfwSensorPrivate *priv = sfwsensor_priv(self);

    priv->sns_plugin            = NULL;
    priv->sns_plugin_changed_id = 0;
    priv->sns_valid             = false;
    priv->sns_active            = false;
    priv->sns_state             = SFWSENSORSTATE_INITIAL;
    priv->sns_session_id        = SESSION_ID_INVALID;
    priv->sns_eval_state_id     = 0;

    priv->sns_get_properties_cancellable  = NULL;
    priv->sns_request_session_cancellable = NULL;
    priv->sns_release_session_cancellable = NULL;

    priv->sns_retry_delay_id    = 0;
    priv->sns_properties        =
        g_hash_table_new_full(g_str_hash, g_str_equal, g_free, gutil_variant_unref_cb);
    priv->sns_socket_fd         = -1;
    priv->sns_socket_tx_id      = 0;
    priv->sns_socket_rx_id      = 0;
    priv->sns_socket_tx_cb      = sfwsensor_stm_socket_tx_unexpected;
    priv->sns_socket_rx_cb      = sfwsensor_stm_socket_rx_unexpected;
    priv->sns_reporting         = sfwreporting_new(self);
    priv->sns_reading.sensor_id = SFW_SENSOR_ID_INVALID;
    priv->sns_valid             = false;

    priv->sns_reporting_active_changed_id =
        sfwreporting_add_active_changed_handler(priv->sns_reporting,
                                                sfwsensor_reporting_active_changed_cb,
                                                self);
}

static void
sfwsensor_finalize(GObject *object)
{
    SfwSensor        *self = SFWSENSOR(object);
    SfwSensorPrivate *priv = sfwsensor_priv(self);
    sfwsensor_log_info("DELETED");

    sfwreporting_remove_handler(priv->sns_reporting, priv->sns_reporting_active_changed_id),
        priv->sns_reporting_active_changed_id = 0;

    sfwsensor_stm_set_state(self, SFWSENSORSTATE_FINAL);

    sfwreporting_unref_at(&priv->sns_reporting);
    sfwsensor_detach_from_plugin(self);

    g_hash_table_unref(priv->sns_properties),
        priv->sns_properties = NULL;

    G_OBJECT_CLASS(sfwsensor_parent_class)->finalize(object);
}

SfwSensor *
sfwsensor_new(SfwSensorId id)
{
    SfwSensor *self = g_object_new(SFWSENSOR_TYPE, NULL);
    sfwsensor_log_debug("self=%p", self);

    sfwsensor_attach_to_plugin(self, id);

    sfwsensor_log_info("CREATED");
    return self;
}

SfwSensor *
sfwsensor_ref(SfwSensor *self)
{
    if( self ) {
        sfwsensor_log_debug("self=%p", self);
        g_object_ref(SFWSENSOR(self));
    }
    return self;
}

void
sfwsensor_unref(SfwSensor *self)
{
    if( self ) {
        sfwsensor_log_debug("self=%p", self);
        g_object_unref(SFWSENSOR(self));
    }
}

void
sfwsensor_unref_cb(gpointer self)
{
    g_object_unref(self);
}

void
sfwsensor_unref_at(SfwSensor **pself)
{
    sfwsensor_unref(*pself), *pself = NULL;
}

/* ------------------------------------------------------------------------- *
 * SFWSENSOR_CONTROL
 * ------------------------------------------------------------------------- */

void
sfwsensor_start(SfwSensor *self)
{
    SfwSensorPrivate *priv = sfwsensor_priv(self);
    if( priv )
        sfwreporting_start(priv->sns_reporting);
}

void
sfwsensor_stop(SfwSensor *self)
{
    SfwSensorPrivate *priv = sfwsensor_priv(self);
    if( priv )
        sfwreporting_stop(priv->sns_reporting);
}

void
sfwsensor_set_datarate(SfwSensor *self, double datarate_hz)
{
    SfwSensorPrivate *priv = sfwsensor_priv(self);
    if( priv )
        sfwreporting_set_datarate(priv->sns_reporting, datarate_hz);
}

void
sfwsensor_set_alwayson(SfwSensor *self, bool alwayson)
{
    SfwSensorPrivate *priv = sfwsensor_priv(self);
    if( priv )
        sfwreporting_set_override(priv->sns_reporting, alwayson);
}

/* ------------------------------------------------------------------------- *
 * SFWSENSOR_VALID
 * ------------------------------------------------------------------------- */

bool
sfwsensor_is_valid(const SfwSensor *self)
{
    SfwSensorPrivate *priv = sfwsensor_priv(self);
    return priv ? priv->sns_valid : false;
}

static void
sfwsensor_set_valid(SfwSensor *self, bool valid)
{
    SfwSensorPrivate *priv = sfwsensor_priv(self);
    if( priv && priv->sns_valid != valid ) {
        sfwsensor_log_info("valid: %s -> %s",
                           priv->sns_valid ? "true" : "false",
                           valid           ? "true" : "false");
        priv->sns_valid = valid;
        sfwsensor_emit_signal(self, SFWSENSOR_SIGNAL_VALID_CHANGED);
    }
}

bool
sfwsensor_is_active(const SfwSensor *self)
{
    SfwSensorPrivate *priv = sfwsensor_priv(self);
    return priv ? priv->sns_active : false;
}

static void
sfwsensor_eval_active(SfwSensor *self)
{
    SfwSensorPrivate *priv = sfwsensor_priv(self);
    if( priv ) {
        bool active = sfwreporting_is_active(priv->sns_reporting);
        if( priv->sns_active != active ) {
            sfwsensor_log_info("active: %s -> %s",
                               priv->sns_active ? "true" : "false",
                               active           ? "true" : "false");
            priv->sns_active = active;
            sfwsensor_emit_signal(self, SFWSENSOR_SIGNAL_ACTIVE_CHANGED);
        }
    }
}

/* ------------------------------------------------------------------------- *
 * SFWSENSOR_READING
 * ------------------------------------------------------------------------- */

SfwReading *
sfwsensor_reading(SfwSensor *self)
{
    SfwSensorPrivate *priv = sfwsensor_priv(self);
    return priv ? &priv->sns_reading : NULL;
}

/* ------------------------------------------------------------------------- *
 * SFWSENSOR_SIGNALS
 * ------------------------------------------------------------------------- */

static gulong
sfwsensor_add_handler(SfwSensor *self, SfwSensorSignal signo,
                      SfwSensorHandler handler, gpointer aptr)
{
    gulong id = 0;
    if( self && handler )
        id = g_signal_connect(self, sfwsensor_signal_name[signo],
                              G_CALLBACK(handler), aptr);
    sfwsensor_log_debug("sig=%s id=%lu", sfwsensor_signal_name[signo], id);
    return id;
}

gulong
sfwsensor_add_valid_changed_handler(SfwSensor *self, SfwSensorHandler handler,
                                    gpointer aptr)
{
    return sfwsensor_add_handler(self, SFWSENSOR_SIGNAL_VALID_CHANGED,
                                 handler, aptr);
}

gulong
sfwsensor_add_active_changed_handler(SfwSensor *self, SfwSensorHandler handler,
                                     gpointer aptr)
{
    return sfwsensor_add_handler(self, SFWSENSOR_SIGNAL_ACTIVE_CHANGED,
                                 handler, aptr);
}

gulong
sfwsensor_add_reading_changed_handler(SfwSensor *self,
                                      SfwSensorHandler handler,
                                      gpointer aptr)
{
    return sfwsensor_add_handler(self, SFWSENSOR_SIGNAL_READING_CHANGED,
                                 handler, aptr);
}

void
sfwsensor_remove_handler(SfwSensor *self, gulong id)
{
    if( self && id ) {
        sfwsensor_log_debug("id=%lu", id);
        g_signal_handler_disconnect(self, id);
    }
}

void
sfwsensor_remove_handler_at(SfwSensor *self, gulong *pid)
{
    sfwsensor_remove_handler(self, *pid), *pid = 0;
}

static void
sfwsensor_emit_signal(SfwSensor *self, SfwSensorSignal signo)
{
    sfwsensor_log_debug("sig=%s id=%u",
                        sfwsensor_signal_name[signo],
                        sfwsensor_signal_id[signo]);
    g_signal_emit(self, sfwsensor_signal_id[signo], 0);
}

/* ------------------------------------------------------------------------- *
 * SFWSENSOR_ACCESSORS
 * ------------------------------------------------------------------------- */

static SfwSensorPrivate *
sfwsensor_priv(const SfwSensor *self)
{
    SfwSensorPrivate *priv = NULL;
    if( self )
        priv = sfwsensor_get_instance_private((SfwSensor *)self);
    return priv;
}

int
sfwsensor_session_id(const SfwSensor *self)
{
    SfwSensorPrivate *priv = sfwsensor_priv(self);
    return priv ? priv->sns_session_id : SESSION_ID_INVALID;
}

SfwPlugin *
sfwsensor_plugin(const SfwSensor *self)
{
    SfwSensorPrivate *priv = sfwsensor_priv(self);
    return priv ? priv->sns_plugin : NULL;
}

SfwService *
sfwsensor_service(const SfwSensor *self)
{
    return sfwplugin_service(sfwsensor_plugin(self));
}

const char *
sfwsensor_name(const SfwSensor *self)
{
    return sfwplugin_name(sfwsensor_plugin(self));
}

const char *
sfwsensor_object(const SfwSensor *self)
{
    return sfwplugin_object(sfwsensor_plugin(self));
}

const char *
sfwsensor_interface(const SfwSensor *self)
{
    return sfwplugin_interface(sfwsensor_plugin(self));
}

/* ------------------------------------------------------------------------- *
 * SFWSENSOR_PLUGIN
 * ------------------------------------------------------------------------- */

static void
sfwsensor_plugin_changed_cb(SfwPlugin *sfwplugin, gpointer aptr)
{
    (void)sfwplugin;
    SfwSensor *self = aptr;
    sfwsensor_stm_reset_state(self);
}

static void
sfwsensor_detach_from_plugin(SfwSensor *self)
{
    SfwSensorPrivate *priv = sfwsensor_priv(self);
    sfwplugin_remove_handler_at(priv->sns_plugin,
                                &priv->sns_plugin_changed_id);
    sfwplugin_unref_at(&priv->sns_plugin);
    priv->sns_reading.sensor_id = SFW_SENSOR_ID_INVALID;
}

static void
sfwsensor_attach_to_plugin(SfwSensor *self, SfwSensorId id)
{
    SfwSensorPrivate *priv = sfwsensor_priv(self);

    priv->sns_reading.sensor_id = id;
    priv->sns_plugin            = sfwplugin_instance(id);
    priv->sns_plugin_changed_id =
        sfwplugin_add_valid_changed_handler(priv->sns_plugin,
                                            sfwsensor_plugin_changed_cb,
                                            self);
    sfwsensor_stm_reset_state(self);
}

static void
sfwsensor_reporting_active_changed_cb(SfwReporting *sfwreporting, gpointer aptr)
{
    (void)sfwreporting;
    SfwSensor *self = aptr;
    sfwsensor_eval_active(self);
}

/* ------------------------------------------------------------------------- *
 * SFWSENSOR_STM_STATE
 * ------------------------------------------------------------------------- */

static const char *
sfwsensor_stm_state_name(const SfwSensor *self)
{
    return sfwsensorstate_repr(sfwsensor_stm_get_state(self));
}

static SfwSensorState
sfwsensor_stm_get_state(const SfwSensor *self)
{
    SfwSensorPrivate *priv = sfwsensor_priv(self);
    return priv ? priv->sns_state : SFWSENSORSTATE_DISABLED;
}

static void
sfwsensor_stm_set_state(SfwSensor *self, SfwSensorState state)
{
    SfwSensorPrivate *priv = sfwsensor_priv(self);
    if( priv->sns_state == SFWSENSORSTATE_FINAL ) {
        /* No way out */
    }
    else if( priv->sns_state != state ) {
        sfwsensor_log_info("state: %s -> %s",
                           sfwsensorstate_repr(priv->sns_state),
                           sfwsensorstate_repr(state));
        sfwsensor_stm_leave_state(self);
        priv->sns_state = state;
        sfwsensor_stm_enter_state(self);
        sfwsensor_stm_eval_state_later(self);
    }
}

static void
sfwsensor_stm_enter_state(SfwSensor *self)
{
    switch( sfwsensor_stm_get_state(self) ) {
    case SFWSENSORSTATE_INITIAL:
        break;
    case SFWSENSORSTATE_DISABLED:
        sfwsensor_stm_socket_disconnect(self);
        sfwsensor_stm_start_release_session(self);
        break;
    case SFWSENSORSTATE_SESSION:
        /* Sensor D-Bus objects are made available on the first
         * client session open. Thus expectation is that we need
         * to acquire session id before making e.g. property
         * queries.
         */
        sfwsensor_stm_start_request_session(self);
        break;
    case SFWSENSORSTATE_PROPERTIES:
        sfwsensor_stm_start_get_properties(self);
        break;
    case SFWSENSORSTATE_CONNECT:
        sfwsensor_stm_socket_connect(self);
        break;
    case SFWSENSORSTATE_READY:
        sfwsensor_set_valid(self, true);
        break;
    case SFWSENSORSTATE_FAILED:
        sfwsensor_stm_socket_disconnect(self);
        sfwsensor_stm_start_retry_delay(self);
        break;
    case SFWSENSORSTATE_FINAL:
        sfwsensor_stm_socket_disconnect(self);
        break;
    default:
        abort();
    }
}

static void
sfwsensor_stm_leave_state(SfwSensor *self)
{
    switch( sfwsensor_stm_get_state(self) ) {
    case SFWSENSORSTATE_INITIAL:
        break;
    case SFWSENSORSTATE_DISABLED:
        sfwsensor_stm_cancel_release_session(self);
        break;
    case SFWSENSORSTATE_SESSION:
        sfwsensor_stm_cancel_request_session(self);
        break;
    case SFWSENSORSTATE_PROPERTIES:
        sfwsensor_stm_cancel_get_properties(self);
        break;
    case SFWSENSORSTATE_CONNECT:
        break;
    case SFWSENSORSTATE_READY:
        sfwsensor_set_valid(self, false);
        sfwsensor_stm_socket_disconnect(self);
        break;
    case SFWSENSORSTATE_FAILED:
        sfwsensor_stm_cancel_retry_delay(self);
        break;
    case SFWSENSORSTATE_FINAL:
        break;
    default:
        abort();
    }
}

static gboolean
sfwsensor_stm_eval_state_cb(gpointer aptr)
{
    SfwSensor        *self = aptr;
    SfwSensorPrivate *priv = sfwsensor_priv(self);

    priv->sns_eval_state_id = 0;

    sfwsensor_log_debug("eval state: %s", sfwsensor_stm_state_name(self));

    switch( sfwsensor_stm_get_state(self) ) {
    case SFWSENSORSTATE_INITIAL:
        break;
    case SFWSENSORSTATE_DISABLED:
        if( sfwsensor_stm_pending_release_session(self) )
            break;
        if( sfwplugin_is_valid(sfwsensor_plugin(self)) )
            sfwsensor_stm_set_state(self, SFWSENSORSTATE_SESSION);
        break;
    case SFWSENSORSTATE_SESSION:
        if( sfwsensor_stm_pending_request_session(self) )
            break;
        if( sfwsensor_session_id(self) == SESSION_ID_INVALID )
            sfwsensor_stm_set_state(self, SFWSENSORSTATE_FAILED);
        else
            sfwsensor_stm_set_state(self, SFWSENSORSTATE_PROPERTIES);
        break;
    case SFWSENSORSTATE_PROPERTIES:
        if( sfwsensor_stm_pending_get_properties(self) )
            break;
        sfwsensor_stm_set_state(self, SFWSENSORSTATE_CONNECT);
        break;
    case SFWSENSORSTATE_CONNECT:
        if( sfwsensor_stm_pending_socket_handshake(self) )
            break;
        if( !sfwsensor_stm_socket_ready_to_receive(self) )
            sfwsensor_stm_set_state(self, SFWSENSORSTATE_FAILED);
        else
            sfwsensor_stm_set_state(self, SFWSENSORSTATE_READY);
        break;
    case SFWSENSORSTATE_READY:
        break;
    case SFWSENSORSTATE_FAILED:
        if( sfwsensor_stm_pending_retry_delay(self) )
            break;
        sfwsensor_stm_set_state(self, SFWSENSORSTATE_SESSION);
        break;
    case SFWSENSORSTATE_FINAL:
        break;
    default:
        abort();
    }

    return G_SOURCE_REMOVE;
}

static void
sfwsensor_stm_cancel_eval_state(SfwSensor *self) {
    SfwSensorPrivate *priv = sfwsensor_priv(self);
    if( priv->sns_eval_state_id ) {
        g_source_remove(priv->sns_eval_state_id),
            priv->sns_eval_state_id = 0;
    }
}

static void
sfwsensor_stm_eval_state_later(SfwSensor *self)
{
    SfwSensorPrivate *priv = sfwsensor_priv(self);
    if( sfwsensor_stm_get_state(self) == SFWSENSORSTATE_FINAL ) {
        sfwsensor_stm_cancel_eval_state(self);
    }
    else if( !priv->sns_eval_state_id ) {
        priv->sns_eval_state_id = g_idle_add(sfwsensor_stm_eval_state_cb, self);
    }
}

static void
sfwsensor_stm_reset_state(SfwSensor *self)
{
    if( self ) {
        sfwsensor_stm_set_state(self, SFWSENSORSTATE_DISABLED);
        sfwsensor_stm_eval_state_later(self);
    }
}

/* ------------------------------------------------------------------------- *
 * SFWSENSOR_STM_RETRY
 * ------------------------------------------------------------------------- */

static gboolean
sfwsensor_stm_retry_delay_cb(gpointer aptr)
{
    SfwSensor        *self = aptr;
    SfwSensorPrivate *priv = sfwsensor_priv(self);
    sfwsensor_log_debug("trigger retry");
    priv->sns_retry_delay_id = 0;
    sfwsensor_stm_eval_state_later(self);
    return G_SOURCE_REMOVE;
}

static void
sfwsensor_stm_start_retry_delay(SfwSensor *self)
{
    SfwSensorPrivate *priv = sfwsensor_priv(self);
    if( gutil_timeout_start(&priv->sns_retry_delay_id, 5000,
                            sfwsensor_stm_retry_delay_cb, self) )
        sfwsensor_log_debug("schedule retry");
}

static void
sfwsensor_stm_cancel_retry_delay(SfwSensor *self)
{
    SfwSensorPrivate *priv = sfwsensor_priv(self);
    if( gutil_timeout_stop(&priv->sns_retry_delay_id) )
        sfwsensor_log_debug("cancel retry");
}

static bool
sfwsensor_stm_pending_retry_delay(const SfwSensor *self)
{
    SfwSensorPrivate *priv = sfwsensor_priv(self);
    return priv ? priv->sns_retry_delay_id : false;
}

/* ------------------------------------------------------------------------- *
 * SFWSENSOR_STM_SESSION
 * ------------------------------------------------------------------------- */

static void
sfwsensor_stm_request_session_cb(GObject *object, GAsyncResult *res, gpointer aptr)
{
    SfwSensor        *self       = aptr;
    SfwSensorPrivate *priv       = sfwsensor_priv(self);
    GDBusConnection  *con        = G_DBUS_CONNECTION(object);
    GError           *err        = NULL;
    GVariant         *rsp        = g_dbus_connection_call_finish(con, res, &err);
    gint              session_id = SESSION_ID_INVALID;

    if( !rsp )
        sfwsensor_log_err("err: %s", error_message(err));
    else
        g_variant_get(rsp, "(i)", &session_id);

    if( cancellable_finish(&priv->sns_request_session_cancellable) ) {
        if( session_id == SESSION_ID_INVALID )
            sfwsensor_log_warning("failed to acquire sensor session");
        else
            priv->sns_session_id = session_id;
        sfwsensor_stm_eval_state_later(self);
    }

    gutil_variant_unref(rsp);
    g_clear_error(&err);
    sfwsensor_unref(self);
}

static void
sfwsensor_stm_start_request_session(SfwSensor *self)
{
    SfwSensorPrivate *priv       = sfwsensor_priv(self);
    int               session_id = sfwsensor_session_id(self);

    if( session_id == SESSION_ID_INVALID ) {
        const char      *name       = sfwsensor_name(self);
        gint64           pid        = getpid();
        SfwService       *service   = sfwsensor_service(self);
        GDBusConnection *connection = sfwservice_get_connection(service);
        cancellable_start(&priv->sns_request_session_cancellable);
        g_dbus_connection_call(connection,
                               SFWDBUS_SERVICE,
                               SFWDBUS_MANAGER_OBJECT,
                               SFWDBUS_MANAGER_INTEFCACE,
                               SFWDBUS_MANAGER_METHOD_START_SESSION,
                               g_variant_new("(sx)", name, pid),
                               NULL,
                               G_DBUS_CALL_FLAGS_NO_AUTO_START,
                               -1,
                               priv->sns_request_session_cancellable,
                               sfwsensor_stm_request_session_cb,
                               sfwsensor_ref(self));
    }
}

static void
sfwsensor_stm_cancel_request_session(SfwSensor *self)
{
    SfwSensorPrivate *priv = sfwsensor_priv(self);
    cancellable_cancel(&priv->sns_request_session_cancellable);
}

static bool
sfwsensor_stm_pending_request_session(const SfwSensor *self)
{
    SfwSensorPrivate *priv = sfwsensor_priv(self);

    bool pending = (priv->sns_request_session_cancellable != NULL);
    if( pending )
        sfwsensor_log_debug("PENDING request sensor");
    return pending;
}

static void
sfwsensor_stm_release_session_cb(GObject *object, GAsyncResult *res, gpointer aptr)
{
    SfwSensor        *self = aptr;
    SfwSensorPrivate *priv = sfwsensor_priv(self);
    GDBusConnection *con   = G_DBUS_CONNECTION(object);
    gboolean         ack   = false;
    GError           *err  = NULL;
    GVariant         *rsp  = g_dbus_connection_call_finish(con, res, &err);

    if( !rsp )
        sfwsensor_log_err("err: %s", error_message(err));
    else
        g_variant_get(rsp, "(b)", &ack);

    if( cancellable_finish(&priv->sns_release_session_cancellable) ) {
        if( !ack )
            sfwsensor_log_warning("failed to release sensor session");
        sfwsensor_stm_eval_state_later(self);
    }

    g_clear_error(&err);
    gutil_variant_unref(rsp);
    sfwsensor_unref(self);
}

static void
sfwsensor_stm_start_release_session(SfwSensor *self)
{
    SfwSensorPrivate *priv       = sfwsensor_priv(self);
    int               session_id = priv->sns_session_id;

    /* Have a session to release? */
    if( session_id == SESSION_ID_INVALID )
        goto EXIT;

    /* Remove session from bookkeeping */
    priv->sns_session_id = SESSION_ID_INVALID;

    /* Still have a service to communicate with? */
    SfwService *service = sfwsensor_service(self);
    if( !sfwservice_is_valid(service) ) {
        sfwsensor_stm_eval_state_later(self);
        goto EXIT;
    }

    const char      *name       = sfwsensor_name(self);
    GDBusConnection *connection = sfwservice_get_connection(service);
    cancellable_start(&priv->sns_release_session_cancellable);
    g_dbus_connection_call(connection,
                           SFWDBUS_SERVICE,
                           SFWDBUS_MANAGER_OBJECT,
                           SFWDBUS_MANAGER_INTEFCACE,
                           SFWDBUS_MANAGER_METHOD_STOP_SESSION,
                           g_variant_new("(s)", name),
                           NULL,
                           G_DBUS_CALL_FLAGS_NO_AUTO_START,
                           -1,
                           priv->sns_release_session_cancellable,
                           sfwsensor_stm_release_session_cb,
                           sfwsensor_ref(self));
EXIT:
    return;
}

static void
sfwsensor_stm_cancel_release_session(SfwSensor *self)
{
    SfwSensorPrivate *priv = sfwsensor_priv(self);
    cancellable_cancel(&priv->sns_release_session_cancellable);
}

static bool
sfwsensor_stm_pending_release_session(const SfwSensor *self)
{
    SfwSensorPrivate *priv = sfwsensor_priv(self);

    bool pending = (priv->sns_release_session_cancellable != NULL);
    if( pending )
        sfwsensor_log_debug("PENDING release sensor");
    return pending;
}

/* ------------------------------------------------------------------------- *
 * SFWSENSOR_STM_PROPERTIES
 * ------------------------------------------------------------------------- */

#define DBUS_PROPERTIES_INTERFACE      "org.freedesktop.DBus.Properties"
#define DBUS_PROPERTIES_METHOD_GET_ALL "GetAll"

static void
sfwsensor_stm_update_property(SfwSensor *self, const char *key, GVariant *val)
{
    SfwSensorPrivate *priv = sfwsensor_priv(self);

    GVariant *old = g_hash_table_lookup(priv->sns_properties, key);
    if( !gutil_variant_equal(old, val) ) {
        if( sfwlog_p(SFWLOG_INFO) ) {
            gchar *txt = val ? g_variant_print(val, false) : NULL;
            sfwsensor_log_info("property: %s = %s", key, txt ?: "null");
            g_free(txt);
        }
        if( val )
            g_hash_table_insert(priv->sns_properties, g_strdup(key),
                                g_variant_ref(val));
        else
            g_hash_table_remove(priv->sns_properties, key);
    }
}

static void
sfwsensor_stm_get_properties_cb(GObject *object, GAsyncResult *res, gpointer aptr)
{
    SfwSensor        *self = aptr;
    SfwSensorPrivate *priv = sfwsensor_priv(self);
    GDBusConnection  *con  = G_DBUS_CONNECTION(object);
    GError           *err  = NULL;
    GVariant         *rsp  = g_dbus_connection_call_finish(con, res, &err);

    if( !rsp )
        sfwsensor_log_err("err: %s", error_message(err));

    if( cancellable_finish(&priv->sns_get_properties_cancellable) ) {
        bool ack = false;
        if( rsp ) {
            GVariant *array = NULL;
            g_variant_get(rsp, "(@a{sv})", &array);
            if( array ) {
                ack = true;
                GVariantIter iter;
                gchar *key;
                GVariant *val;
                g_variant_iter_init(&iter, array);
                while( g_variant_iter_next (&iter, "{sv}", &key, &val) ) {
                    sfwsensor_stm_update_property(self, key, val);
                    g_variant_unref(val);
                    g_free(key);
                }
                g_variant_unref(array);
            }
        }
        if( !ack )
            sfwsensor_log_warning("failed to query properties");
        sfwsensor_stm_eval_state_later(self);
    }

    g_clear_error(&err);
    gutil_variant_unref(rsp);
    sfwsensor_unref(self);
}

static void
sfwsensor_stm_start_get_properties(SfwSensor *self)
{
    SfwSensorPrivate *priv       = sfwsensor_priv(self);
    SfwPlugin        *plugin     = sfwsensor_plugin(self);
    SfwSensorId       id         = sfwplugin_id(plugin);
    SfwService       *service    = sfwplugin_service(plugin);
    GDBusConnection  *connection = sfwservice_get_connection(service);
    const char       *object     = sfwsensorid_object(id);
    const char       *interface  = sfwsensorid_interface(id);

    cancellable_start(&priv->sns_get_properties_cancellable);
    g_dbus_connection_call(connection,
                           SFWDBUS_SERVICE,
                           object,
                           DBUS_PROPERTIES_INTERFACE,
                           DBUS_PROPERTIES_METHOD_GET_ALL,
                           g_variant_new("(s)", interface),
                           NULL,
                           G_DBUS_CALL_FLAGS_NO_AUTO_START,
                           -1,
                           priv->sns_get_properties_cancellable,
                           sfwsensor_stm_get_properties_cb,
                           sfwsensor_ref(self));
}

static void
sfwsensor_stm_cancel_get_properties(SfwSensor *self)
{
    SfwSensorPrivate *priv = sfwsensor_priv(self);
    cancellable_cancel(&priv->sns_get_properties_cancellable);
}

static bool
sfwsensor_stm_pending_get_properties(const SfwSensor *self)
{
    SfwSensorPrivate *priv = sfwsensor_priv(self);

    bool pending = (priv->sns_get_properties_cancellable != NULL);
    if( pending )
        sfwsensor_log_debug("PENDING get properties");
    return pending;
}

/* ------------------------------------------------------------------------- *
 * SFWSENSOR_STM_SOCKET
 * ------------------------------------------------------------------------- */

static gboolean
sfwsensor_stm_socket_rx_unexpected(GIOChannel *chn, GIOCondition cnd, gpointer aptr)
{
    (void)chn;
    (void)cnd;

    SfwSensor        *self = aptr;
    SfwSensorPrivate *priv = sfwsensor_priv(self);
    sfwsensor_log_err("unexpected data connection input");
    priv->sns_socket_rx_id = 0;
    sfwsensor_stm_set_state(self, SFWSENSORSTATE_FAILED);
    return G_SOURCE_REMOVE;
}

static gboolean
sfwsensor_stm_socket_rx_handshake(GIOChannel *chn, GIOCondition cnd, gpointer aptr)
{
    (void)chn;
    (void)cnd;

    SfwSensor        *self = aptr;
    SfwSensorPrivate *priv = sfwsensor_priv(self);
    char              ack  = 0;
    ssize_t           rc   = socket_read(priv->sns_socket_fd, &ack, sizeof ack);

    if( (size_t)rc != sizeof ack ) {
        sfwsensor_log_err("failed to receive data connection handshake");
    }
    if( ack != '\n' ) {
        sfwsensor_log_err("incorrect data connection handshake: %d", ack);
        priv->sns_socket_rx_id = 0;
        sfwsensor_stm_set_state(self, SFWSENSORSTATE_FAILED);
        return G_SOURCE_REMOVE;
    }
    sfwsensor_log_info("data connection handshake received");

    if( !socket_set_blocking(priv->sns_socket_fd, true) ) {
        sfwsensor_log_err("failed to set blocking io mode: %m");
        priv->sns_socket_rx_id = 0;
        sfwsensor_stm_set_state(self, SFWSENSORSTATE_FAILED);
        return G_SOURCE_REMOVE;
    }

    priv->sns_socket_rx_cb = sfwsensor_stm_socket_rx_reading;
    sfwsensor_stm_eval_state_later(self);
    return G_SOURCE_CONTINUE;
}

static gboolean
sfwsensor_stm_socket_rx_reading(GIOChannel *chn, GIOCondition cnd, gpointer aptr)
{
    (void)chn;
    (void)cnd;

    SfwSensor        *self   = aptr;
    SfwSensorPrivate *priv   = sfwsensor_priv(self);
    gboolean          result = G_SOURCE_REMOVE;

    uint32_t cnt = 0;
    if( socket_read(priv->sns_socket_fd, &cnt, sizeof cnt) != sizeof cnt ) {
        sfwsensor_log_err("failed to read sample count");
        goto EXIT;
    }
    if( cnt < 1 || cnt > 16 ) {
        sfwsensor_log_err("suspicious sample count: %" PRIu32, cnt);
        goto EXIT;
    }
    sfwsensor_log_debug("sample count: %" PRIu32, cnt);

    const size_t blk = sfwsensorid_sample_size(priv->sns_reading.sensor_id);
    if( blk < sizeof(uint32_t) || blk > sizeof(SfwSample) ) {
        sfwsensor_log_err("suspicious sample size: %zu", blk);
        goto EXIT;
    }
    for( uint32_t i = 0; i < cnt; ++i ) {
        ssize_t done = socket_read(priv->sns_socket_fd,
                                   &priv->sns_reading.sample, blk);
        if( done == -1 ) {
            sfwsensor_log_err("reading: %m");
            goto EXIT;
        }
        if( done == 0 ) {
            sfwsensor_log_err("reading: EOF");
            goto EXIT;
        }
        if( (size_t)done != blk ) {
            sfwsensor_log_err("reading: NAK");
            goto EXIT;
        }
        if( sfwreporting_is_active(priv->sns_reporting) ) {
            sfwreading_normalize(&priv->sns_reading);
            sfwsensor_emit_signal(self, SFWSENSOR_SIGNAL_READING_CHANGED);
        }
        else {
            sfwreading_normalize(&priv->sns_reading);
            sfwsensor_log_debug("IGNORED[%"PRIu32"]: %s", i, sfwreading_repr(&priv->sns_reading));
        }
    }
    result = G_SOURCE_CONTINUE;
EXIT:
    if( result == G_SOURCE_REMOVE ) {
        priv->sns_socket_rx_id = 0;
        sfwsensor_stm_set_state(self, SFWSENSORSTATE_FAILED);
    }
    return result;
}

static gboolean
sfwsensor_stm_socket_tx_unexpected(GIOChannel *chn, GIOCondition cnd, gpointer aptr)
{
    (void)chn;
    (void)cnd;

    SfwSensor        *self = aptr;
    SfwSensorPrivate *priv = sfwsensor_priv(self);
    sfwsensor_log_err("unexpected data connection output");
    priv->sns_socket_tx_id = 0;
    sfwsensor_stm_set_state(self, SFWSENSORSTATE_FAILED);
    return G_SOURCE_REMOVE;
}

static gboolean
sfwsensor_stm_socket_tx_handshake(GIOChannel *chn, GIOCondition cnd, gpointer aptr)
{
    (void)chn;
    (void)cnd;

    SfwSensor        *self = aptr;
    SfwSensorPrivate *priv = sfwsensor_priv(self);

    priv->sns_socket_tx_id = 0;

    int32_t id = sfwsensor_session_id(self);
    ssize_t rc = socket_write(priv->sns_socket_fd, &id, sizeof id);
    if( (size_t)rc != sizeof id ) {
        sfwsensor_log_err("failed to send data connection handshake");
        sfwsensor_stm_set_state(self, SFWSENSORSTATE_FAILED);
    }
    else {
        sfwsensor_log_info("data connection handshake sent");
        priv->sns_socket_rx_cb = sfwsensor_stm_socket_rx_handshake;
        sfwsensor_stm_eval_state_later(self);
    }

    return G_SOURCE_REMOVE;
}

static gboolean
sfwsensor_stm_socket_tx_cb(GIOChannel *chn, GIOCondition cnd, gpointer aptr)
{
    (void)chn;
    (void)cnd;

    SfwSensor        *self = aptr;
    SfwSensorPrivate *priv = sfwsensor_priv(self);
    return priv->sns_socket_tx_cb(chn, cnd, self);
}

static gboolean
sfwsensor_stm_socket_rx_cb(GIOChannel *chn, GIOCondition cnd, gpointer aptr)
{
    (void)chn;
    (void)cnd;

    SfwSensor        *self = aptr;
    SfwSensorPrivate *priv = sfwsensor_priv(self);
    return priv->sns_socket_rx_cb(chn, cnd, self);
}

static bool
sfwsensor_stm_socket_connect(SfwSensor *self)
{
    SfwSensorPrivate *priv = sfwsensor_priv(self);
    sfwsensor_stm_socket_disconnect(self);

    sfwsensor_log_info("data connect");

    bool  ack   = false;
    int   fd    = -1;
    guint rx_id = 0;
    guint tx_id = 0;

    if( (fd = socket_open(SENSORFW_DATA_SOCKET)) == -1 )
        goto EXIT;

    if( !(tx_id = socket_add_notify(fd, false, G_IO_OUT, sfwsensor_stm_socket_tx_cb, self)) )
        goto EXIT;

    if( !(rx_id = socket_add_notify(fd, false, G_IO_IN, sfwsensor_stm_socket_rx_cb, self)) )
        goto EXIT;

    priv->sns_socket_rx_cb = sfwsensor_stm_socket_rx_unexpected;
    priv->sns_socket_tx_cb = sfwsensor_stm_socket_tx_handshake;
    priv->sns_socket_rx_id = rx_id, rx_id = 0;
    priv->sns_socket_tx_id = tx_id, tx_id = 0;
    priv->sns_socket_fd    = fd, fd = -1;

    ack = true;

EXIT:
    gutil_source_remove_at(&tx_id);
    gutil_source_remove_at(&rx_id);
    socket_close_at(&fd);

    return ack;
}

static void
sfwsensor_stm_socket_disconnect(SfwSensor *self)
{
    SfwSensorPrivate *priv = sfwsensor_priv(self);
    priv->sns_socket_tx_cb = sfwsensor_stm_socket_tx_unexpected;
    priv->sns_socket_rx_cb = sfwsensor_stm_socket_rx_unexpected;
    gutil_source_remove_at(&priv->sns_socket_tx_id);
    gutil_source_remove_at(&priv->sns_socket_rx_id);
    if( socket_close_at(&priv->sns_socket_fd) )
        sfwsensor_log_info("data disconnect");
}

static bool
sfwsensor_stm_pending_socket_handshake(const SfwSensor *self)
{
    SfwSensorPrivate *priv = sfwsensor_priv(self);

    bool pending = priv->sns_socket_fd != -1 && priv->sns_socket_tx_id != 0;
    if( pending )
        sfwsensor_log_info("pending handshake");
    return pending;
}

static bool
sfwsensor_stm_socket_ready_to_receive(const SfwSensor *self)
{
    SfwSensorPrivate *priv = sfwsensor_priv(self);

    bool ready = priv->sns_socket_fd != -1 && priv->sns_socket_rx_id != 0;
    if( !ready )
        sfwsensor_log_info("not ready to receive");
    return ready;
}
