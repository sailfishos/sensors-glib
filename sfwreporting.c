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

#include "sfwreporting.h"

#include "sfwservice.h"
#include "sfwsensor.h"
#include "sfwlogging.h"
#include "sfwdbus.h"
#include "utility.h"

/* ========================================================================= *
 * Constants
 * ========================================================================= */

#define ENABLE_DEFAULT false
#define ENABLE_INVALID (-1)

#define DEFAULT_DATARATE (0)
#define INVALID_DATARATE (-1)

#define DEFAULT_OVERRIDE false
#define INVALID_OVERRIDE (-1)

/* ========================================================================= *
 * Types
 * ========================================================================= */

typedef enum SfwReportingState
{
    SFWREPORTINGSTATE_INITIAL,
    SFWREPORTINGSTATE_DISABLED,
    SFWREPORTINGSTATE_RETHINK,
    SFWREPORTINGSTATE_STARTING,
    SFWREPORTINGSTATE_CONFIGURE,
    SFWREPORTINGSTATE_STARTED,
    SFWREPORTINGSTATE_STOPPING,
    SFWREPORTINGSTATE_STOPPED,
    SFWREPORTINGSTATE_FAILED,
    SFWREPORTINGSTATE_FINAL,
    SFWREPORTINGSTATE_COUNT
} SfwReportingState;

typedef struct SfwReportingPrivate
{
    SfwSensor         *rpt_sensor;
    gulong             rpt_sensor_changed_id;

    /* State */
    SfwReportingState  rpt_state;
    bool               rpt_valid;
    bool               rpt_active;
    guint              rpt_eval_state_id;

    /* Fail - Retry */
    guint              rpt_retry_delay_id;

    /* Start / Stop */
    bool               rpt_enable_wanted;
    int                rpt_enable_requested;
    int                rpt_enable_effective;
    GCancellable      *rpt_enable_cancellable;

    /* Datarate */
    double             rpt_datarate_wanted;
    double             rpt_datarate_requested;
    double             rpt_datarate_effective;
    GCancellable      *rpt_datarate_cancellable;

    /* Stand-by override */
    bool               rpt_override_wanted;
    int                rpt_override_requested;
    int                rpt_override_effective;
    GCancellable      *rpt_override_cancellable;

} SfwReportingPrivate;

struct SfwReporting
{
    GObject object;
};

typedef enum SfwReportingSignal
{
    SFWREPORTING_SIGNAL_VALID_CHANGED,
    SFWREPORTING_SIGNAL_ACTIVE_CHANGED,
    SFWREPORTING_SIGNAL_COUNT,
} SfwReportingSignal;

typedef GObjectClass SfwReportingClass;

/* ========================================================================= *
 * Macros
 * ========================================================================= */

# define sfwreporting_log_emit(LEV, FMT, ARGS...)\
     sfwlog_emit(LEV, "sfwreporting(%s): " FMT, sfwreporting_name(self), ##ARGS)

# define sfwreporting_log_crit(   FMT, ARGS...) sfwreporting_log_emit(SFWLOG_CRIT,    FMT, ##ARGS)
# define sfwreporting_log_err(    FMT, ARGS...) sfwreporting_log_emit(SFWLOG_ERR,     FMT, ##ARGS)
# define sfwreporting_log_warning(FMT, ARGS...) sfwreporting_log_emit(SFWLOG_WARNING, FMT, ##ARGS)
# define sfwreporting_log_notice( FMT, ARGS...) sfwreporting_log_emit(SFWLOG_NOTICE,  FMT, ##ARGS)
# define sfwreporting_log_info(   FMT, ARGS...) sfwreporting_log_emit(SFWLOG_INFO,    FMT, ##ARGS)
# define sfwreporting_log_debug(  FMT, ARGS...) sfwreporting_log_emit(SFWLOG_DEBUG,   FMT, ##ARGS)
# define sfwreporting_log_trace(  FMT, ARGS...) sfwreporting_log_emit(SFWLOG_TRACE,   FMT, ##ARGS)

/* ========================================================================= *
 * Prototypes
 * ========================================================================= */

/* ------------------------------------------------------------------------- *
 * SFWREPORTING_CLASS
 * ------------------------------------------------------------------------- */

static void sfwreporting_class_intern_init(gpointer klass);
static void sfwreporting_class_init       (SfwReportingClass *klass);

/* ------------------------------------------------------------------------- *
 * SFWREPORTING_TYPE
 * ------------------------------------------------------------------------- */

GType        sfwreporting_get_type     (void);
static GType sfwreporting_get_type_once(void);

/* ------------------------------------------------------------------------- *
 * SFWREPORTINGSTATE
 * ------------------------------------------------------------------------- */

static const char *sfwreportingstate_repr(SfwReportingState state);

/* ------------------------------------------------------------------------- *
 * SFWREPORTING_LIFECYCLE
 * ------------------------------------------------------------------------- */

static void   sfwreporting_init    (SfwReporting *self);
static void   sfwreporting_finalize(GObject *object);
SfwReporting *sfwreporting_new     (SfwSensor *sensor);
SfwReporting *sfwreporting_ref     (SfwReporting *self);
void          sfwreporting_unref   (SfwReporting *self);
void          sfwreporting_unref_at(SfwReporting **pself);

/* ------------------------------------------------------------------------- *
 * SFWREPORTING_CONTROL
 * ------------------------------------------------------------------------- */

bool sfwreporting_is_started  (const SfwReporting *self);
bool sfwreporting_is_stopped  (const SfwReporting *self);
void sfwreporting_start       (SfwReporting *self);
void sfwreporting_stop        (SfwReporting *self);
void sfwreporting_set_datarate(SfwReporting *self, double datarate_Hz);
void sfwreporting_set_interval(SfwReporting *self, int interval_us);
void sfwreporting_set_override(SfwReporting *self, bool override);

/* ------------------------------------------------------------------------- *
 * SFWREPORTING_VALID
 * ------------------------------------------------------------------------- */

bool        sfwreporting_is_valid (const SfwReporting *self);
static void sfwreporting_set_valid(SfwReporting *self, bool valid);

/* ------------------------------------------------------------------------- *
 * SFWREPORTING_ACTIVE
 * ------------------------------------------------------------------------- */

bool        sfwreporting_is_active (const SfwReporting *self);
static void sfwreporting_set_active(SfwReporting *self, bool active);

/* ------------------------------------------------------------------------- *
 * SFWREPORTING_SIGNALS
 * ------------------------------------------------------------------------- */

static gulong sfwreporting_add_handler               (SfwReporting *self, SfwReportingSignal signo, SfwReportingHandler handler, gpointer aptr);
gulong        sfwreporting_add_valid_changed_handler (SfwReporting *self, SfwReportingHandler handler, gpointer aptr);
gulong        sfwreporting_add_active_changed_handler(SfwReporting *self, SfwReportingHandler handler, gpointer aptr);
void          sfwreporting_remove_handler            (SfwReporting *self, gulong id);
static void   sfwreporting_emit_signal               (SfwReporting *self, SfwReportingSignal signo);

/* ------------------------------------------------------------------------- *
 * SFWREPORTING_ACCESSORS
 * ------------------------------------------------------------------------- */

SfwSensor                  *sfwreporting_sensor    (const SfwReporting *self);
static SfwReportingPrivate *sfwreporting_priv      (const SfwReporting *self);
static SfwService          *sfwreporting_service   (const SfwReporting *self);
static const char          *sfwreporting_name      (const SfwReporting *self);
static const char          *sfwreporting_object    (const SfwReporting *self);
static const char          *sfwreporting_interface (const SfwReporting *self);
static int                  sfwreporting_session_id(const SfwReporting *self);
static GDBusConnection     *sfwreporting_connection(const SfwReporting *self);

/* ------------------------------------------------------------------------- *
 * SFWREPORTING_SENSOR
 * ------------------------------------------------------------------------- */

static void sfwreporting_sensor_changed_cb (SfwSensor *sensor, gpointer aptr);
static void sfwreporting_detach_from_sensor(SfwReporting *self);
static void sfwreporting_attach_to_sensor  (SfwReporting *self, SfwSensor *sensor);

/* ------------------------------------------------------------------------- *
 * SFWREPORTING_STM_STATE
 * ------------------------------------------------------------------------- */

static const char        *sfwreporting_stm_state_name       (const SfwReporting *self);
static SfwReportingState  sfwreporting_stm_get_state        (const SfwReporting *self);
static void               sfwreporting_stm_set_state        (SfwReporting *self, SfwReportingState state);
static void               sfwreporting_stm_enter_state      (SfwReporting *self);
static void               sfwreporting_stm_leave_state      (SfwReporting *self);
static gboolean           sfwreporting_stm_eval_state_cb    (gpointer aptr);
static void               sfwreporting_stm_cancel_eval_state(SfwReporting *self);
static void               sfwreporting_stm_eval_state_later (SfwReporting *self);
static void               sfwreporting_stm_reset_state      (SfwReporting *self);

/* ------------------------------------------------------------------------- *
 * SFWREPORTING_STM_RETRY
 * ------------------------------------------------------------------------- */

static gboolean sfwreporting_stm_retry_delay_cb     (gpointer aptr);
static void     sfwreporting_stm_start_retry_delay  (SfwReporting *self);
static void     sfwreporting_stm_cancel_retry_delay (SfwReporting *self);
static bool     sfwreporting_stm_pending_retry_delay(const SfwReporting *self);

/* ------------------------------------------------------------------------- *
 * SFWREPORTING_STM_ENABLE
 * ------------------------------------------------------------------------- */

static void sfwreporting_stm_enable_cb     (GObject *object, GAsyncResult *res, gpointer aptr);
static void sfwreporting_stm_start_enable  (SfwReporting *self);
static void sfwreporting_stm_cancel_enable (SfwReporting *self);
static bool sfwreporting_stm_pending_enable(SfwReporting *self);

/* ------------------------------------------------------------------------- *
 * SFWREPORTING_STM_DATARATE
 * ------------------------------------------------------------------------- */

static void sfwreporting_stm_datarate_cb     (GObject *object, GAsyncResult *res, gpointer aptr);
static void sfwreporting_stm_start_datarate  (SfwReporting *self);
static void sfwreporting_stm_cancel_datarate (SfwReporting *self);
static bool sfwreporting_stm_pending_datarate(SfwReporting *self);

/* ------------------------------------------------------------------------- *
 * SFWREPORTING_STM_OVERRIDE
 * ------------------------------------------------------------------------- */

static void sfwreporting_stm_override_cb     (GObject *object, GAsyncResult *res, gpointer aptr);
static void sfwreporting_stm_start_override  (SfwReporting *self);
static void sfwreporting_stm_cancel_override (SfwReporting *self);
static bool sfwreporting_stm_pending_override(SfwReporting *self);

/* ========================================================================= *
 * SFWREPORTING_CLASS
 * ========================================================================= */

G_DEFINE_TYPE_WITH_PRIVATE(SfwReporting, sfwreporting, G_TYPE_OBJECT)
#define SFWREPORTING_TYPE (sfwreporting_get_type())
#define SFWREPORTING(obj) (G_TYPE_CHECK_INSTANCE_CAST(obj, SFWREPORTING_TYPE, SfwReporting))

static const char * const sfwreporting_signal_name[SFWREPORTING_SIGNAL_COUNT] =
{
    [SFWREPORTING_SIGNAL_VALID_CHANGED]  = "sfwreporting-valid-changed",
    [SFWREPORTING_SIGNAL_ACTIVE_CHANGED] = "sfwreporting-active-changed",
};

static guint sfwreporting_signal_id[SFWREPORTING_SIGNAL_COUNT] = { };

static void
sfwreporting_class_init(SfwReportingClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS(klass);

    object_class->finalize = sfwreporting_finalize;

    for( guint signo = 0; signo < SFWREPORTING_SIGNAL_COUNT; ++signo )
        sfwreporting_signal_id[signo] = g_signal_new(sfwreporting_signal_name[signo],
                                                     G_OBJECT_CLASS_TYPE(klass),
                                                     G_SIGNAL_RUN_FIRST,
                                                     0, NULL, NULL, NULL,
                                                     G_TYPE_NONE, 0);
}

/* ========================================================================= *
 * SFWREPORTINGSTATE
 * ========================================================================= */

static const char *
sfwreportingstate_repr(SfwReportingState state)
{
    static const char * const lut[SFWREPORTINGSTATE_COUNT] = {
        [SFWREPORTINGSTATE_INITIAL]   = "SFWREPORTINGSTATE_INITIAL",
        [SFWREPORTINGSTATE_DISABLED]  = "SFWREPORTINGSTATE_DISABLED",
        [SFWREPORTINGSTATE_RETHINK]   = "SFWREPORTINGSTATE_RETHINK",
        [SFWREPORTINGSTATE_STARTING]  = "SFWREPORTINGSTATE_STARTING",
        [SFWREPORTINGSTATE_CONFIGURE] = "SFWREPORTINGSTATE_CONFIGURE",
        [SFWREPORTINGSTATE_STARTED]   = "SFWREPORTINGSTATE_STARTED",
        [SFWREPORTINGSTATE_STOPPING]  = "SFWREPORTINGSTATE_STOPPING",
        [SFWREPORTINGSTATE_STOPPED]   = "SFWREPORTINGSTATE_STOPPED",
        [SFWREPORTINGSTATE_FAILED]    = "SFWREPORTINGSTATE_FAILED",
        [SFWREPORTINGSTATE_FINAL]     = "SFWREPORTINGSTATE_FINAL",
    };
    return lut[state];
}

/* ========================================================================= *
 * SFWREPORTING
 * ========================================================================= */

/* ------------------------------------------------------------------------- *
 * SFWREPORTING_LIFECYCLE
 * ------------------------------------------------------------------------- */

static void
sfwreporting_init(SfwReporting *self)
{
    sfwreporting_log_debug("self=%p", self);

    SfwReportingPrivate *priv = sfwreporting_priv(self);

    priv->rpt_sensor              = NULL;
    priv->rpt_sensor_changed_id   = 0;

    /* State */
    priv->rpt_state               = SFWREPORTINGSTATE_INITIAL;
    priv->rpt_valid               = false;
    priv->rpt_active              = false;
    priv->rpt_eval_state_id       = 0;

    /* Fail - Retry */
    priv->rpt_retry_delay_id      = 0;

    /* Start / Stop */
    priv->rpt_enable_wanted       = ENABLE_DEFAULT;
    priv->rpt_enable_requested    = ENABLE_INVALID;
    priv->rpt_enable_effective    = ENABLE_INVALID;
    priv->rpt_enable_cancellable  = NULL;

    /* Datarate */
    priv->rpt_datarate_wanted      = DEFAULT_DATARATE;
    priv->rpt_datarate_requested   = INVALID_DATARATE;
    priv->rpt_datarate_effective   = DEFAULT_DATARATE;
    priv->rpt_datarate_cancellable = NULL;

    /* Stand-by override */
    priv->rpt_override_wanted      = DEFAULT_OVERRIDE;
    priv->rpt_override_requested   = INVALID_OVERRIDE;
    priv->rpt_override_effective   = DEFAULT_OVERRIDE;
    priv->rpt_override_cancellable = NULL;
}

static void
sfwreporting_finalize(GObject *object)
{
    SfwReporting *self = SFWREPORTING(object);

    sfwreporting_stm_set_state(self, SFWREPORTINGSTATE_FINAL);
    sfwreporting_log_info("DELETED");
    sfwreporting_detach_from_sensor(self);

    G_OBJECT_CLASS(sfwreporting_parent_class)->finalize(object);
}

SfwReporting *
sfwreporting_new(SfwSensor *sensor)
{
    SfwReporting *self = g_object_new(SFWREPORTING_TYPE, NULL);

    sfwreporting_attach_to_sensor(self, sensor);
    sfwreporting_log_info("CREATED");

    sfwreporting_stm_set_state(self, SFWREPORTINGSTATE_DISABLED);
    return self;
}

SfwReporting *
sfwreporting_ref(SfwReporting *self)
{
    if( self ) {
        sfwreporting_log_debug("self=%p", self);
        g_object_ref(SFWREPORTING(self));
    }
    return self;
}

void
sfwreporting_unref(SfwReporting *self)
{
    if( self ) {
        sfwreporting_log_debug("self=%p", self);
        g_object_unref(SFWREPORTING(self));
    }
}

void
sfwreporting_unref_at(SfwReporting **pself)
{
    sfwreporting_unref(*pself), *pself = NULL;
}

/* ------------------------------------------------------------------------- *
 * SFWREPORTING_CONTROL
 * ------------------------------------------------------------------------- */

bool
sfwreporting_is_started(const SfwReporting *self)
{
    SfwReportingPrivate *priv = sfwreporting_priv(self);
    return priv ? priv->rpt_enable_wanted : false;
}

bool
sfwreporting_is_stopped(const SfwReporting *self)
{
    SfwReportingPrivate *priv = sfwreporting_priv(self);
    return priv ? !priv->rpt_enable_wanted : true;
}

void
sfwreporting_start(SfwReporting *self)
{
    SfwReportingPrivate *priv = sfwreporting_priv(self);
    if( !priv->rpt_enable_wanted ) {
        priv->rpt_enable_wanted = true;
        sfwreporting_log_info("starting");
        sfwreporting_stm_eval_state_later(self);
    }
}

void
sfwreporting_stop(SfwReporting *self)
{
    SfwReportingPrivate *priv = sfwreporting_priv(self);
    if( priv->rpt_enable_wanted ) {
        priv->rpt_enable_wanted = false;
        sfwreporting_log_info("stopping");
        sfwreporting_stm_eval_state_later(self);
    }
}

void
sfwreporting_set_datarate(SfwReporting *self, double datarate_Hz)
{
    SfwReportingPrivate *priv = sfwreporting_priv(self);
    priv->rpt_datarate_wanted = datarate_Hz;
    sfwreporting_stm_eval_state_later(self);
}

void
sfwreporting_set_interval(SfwReporting *self, int interval_us)
{
    double datarate_Hz = 0;
    if( interval_us > 0 )
        datarate_Hz = 1e6 / interval_us;
    sfwreporting_set_datarate(self, datarate_Hz);
}

void
sfwreporting_set_override(SfwReporting *self, bool override)
{
    SfwReportingPrivate *priv = sfwreporting_priv(self);
    priv->rpt_override_wanted = override;
    sfwreporting_stm_eval_state_later(self);
}

/* ------------------------------------------------------------------------- *
 * SFWREPORTING_VALID
 * ------------------------------------------------------------------------- */

bool
sfwreporting_is_valid(const SfwReporting *self)
{
    SfwReportingPrivate *priv = sfwreporting_priv(self);
    return priv ? priv->rpt_valid : false;
}

static void
sfwreporting_set_valid(SfwReporting *self, bool valid)
{
    SfwReportingPrivate *priv = sfwreporting_priv(self);
    if( priv && priv->rpt_valid != valid ) {
        sfwreporting_log_info("valid: %s -> %s",
                              priv->rpt_valid ? "true" : "false",
                              valid           ? "true" : "false");
        priv->rpt_valid = valid;
        sfwreporting_emit_signal(self, SFWREPORTING_SIGNAL_VALID_CHANGED);
    }
}

/* ------------------------------------------------------------------------- *
 * SFWREPORTING_ACTIVE
 * ------------------------------------------------------------------------- */

bool
sfwreporting_is_active(const SfwReporting *self)
{
    SfwReportingPrivate *priv = sfwreporting_priv(self);
    return priv ? priv->rpt_active : false;
}

static void
sfwreporting_set_active(SfwReporting *self, bool active)
{
    SfwReportingPrivate *priv = sfwreporting_priv(self);
    if( priv && priv->rpt_active != active ) {
        sfwreporting_log_info("active: %s -> %s",
                              priv->rpt_active ? "true" : "false",
                              active           ? "true" : "false");
        priv->rpt_active = active;
        sfwreporting_emit_signal(self, SFWREPORTING_SIGNAL_ACTIVE_CHANGED);
    }
}

/* ------------------------------------------------------------------------- *
 * SFWREPORTING_SIGNALS
 * ------------------------------------------------------------------------- */

static gulong
sfwreporting_add_handler(SfwReporting *self, SfwReportingSignal signo,
                         SfwReportingHandler handler, gpointer aptr)
{
    gulong id = 0;
    if( self && handler )
        id = g_signal_connect(self, sfwreporting_signal_name[signo],
                              G_CALLBACK(handler), aptr);
    sfwreporting_log_debug("self=%p sig=%s id=%lu", self,
                           sfwreporting_signal_name[signo], id);
    return id;
}

gulong
sfwreporting_add_valid_changed_handler(SfwReporting *self,
                                       SfwReportingHandler handler,
                                       gpointer aptr)
{
    return sfwreporting_add_handler(self, SFWREPORTING_SIGNAL_VALID_CHANGED,
                                    handler, aptr);
}

gulong
sfwreporting_add_active_changed_handler(SfwReporting *self, SfwReportingHandler handler,
                                        gpointer aptr)
{
    return sfwreporting_add_handler(self, SFWREPORTING_SIGNAL_ACTIVE_CHANGED,
                                    handler, aptr);
}

void
sfwreporting_remove_handler(SfwReporting *self, gulong id)
{
    if( self && id ) {
        sfwreporting_log_debug("self=%p id=%lu", self, id);
        g_signal_handler_disconnect(self, id);
    }
}

static void
sfwreporting_emit_signal(SfwReporting *self, SfwReportingSignal signo)
{
    sfwreporting_log_info("sig=%s id=%u",
                          sfwreporting_signal_name[signo],
                          sfwreporting_signal_id[signo]);
    g_signal_emit(self, sfwreporting_signal_id[signo], 0);
}

/* ------------------------------------------------------------------------- *
 * SFWREPORTING_ACCESSORS
 * ------------------------------------------------------------------------- */

// NB direct parent is extern ...

SfwSensor *
sfwreporting_sensor(const SfwReporting *self)
{
    SfwReportingPrivate *priv = sfwreporting_priv(self);
    return priv ? priv->rpt_sensor : NULL;
}

// ... the rest is local use only

static SfwReportingPrivate *
sfwreporting_priv(const SfwReporting *self)
{
    SfwReportingPrivate *priv = NULL;
    if( self )
        priv = sfwreporting_get_instance_private((SfwReporting *)self);
    return priv;
}

#ifdef DEAD_CODE
static SfwPlugin *
sfwreporting_plugin(const SfwReporting *self)
{
    return sfwsensor_plugin(sfwreporting_sensor(self));
}
#endif

static SfwService *
sfwreporting_service(const SfwReporting *self)
{
    return sfwsensor_service(sfwreporting_sensor(self));
}

static const char *
sfwreporting_name(const SfwReporting *self)
{
    return sfwsensor_name(sfwreporting_sensor(self));
}

static const char *
sfwreporting_object(const SfwReporting *self)
{
    return sfwsensor_object(sfwreporting_sensor(self));
}

static const char *
sfwreporting_interface(const SfwReporting *self)
{
    return sfwsensor_interface(sfwreporting_sensor(self));
}

static int
sfwreporting_session_id(const SfwReporting *self)
{
    return sfwsensor_session_id(sfwreporting_sensor(self));
}

static GDBusConnection *
sfwreporting_connection(const SfwReporting *self)
{
    return sfwservice_get_connection(sfwreporting_service(self));
}

/* ------------------------------------------------------------------------- *
 * SFWREPORTING_SENSOR
 * ------------------------------------------------------------------------- */

static void
sfwreporting_sensor_changed_cb(SfwSensor *sensor, gpointer aptr)
{
    (void)sensor;
    SfwReporting *self = aptr;
    sfwreporting_stm_reset_state(self);
}

static void
sfwreporting_detach_from_sensor(SfwReporting *self)
{
    SfwReportingPrivate *priv = sfwreporting_priv(self);
#if 0
    // FIXME can diagnostic noise be avoided?
    sfwsensor_remove_handler_at(priv->rpt_sensor,
                                &priv->rpt_sensor_changed_id);
#else
    // handler is already implicitly removed
    priv->rpt_sensor_changed_id = 0;
#endif
    priv->rpt_sensor = NULL;
}

static void
sfwreporting_attach_to_sensor(SfwReporting *self, SfwSensor *sensor)
{
    SfwReportingPrivate *priv = sfwreporting_priv(self);

    sfwreporting_detach_from_sensor(self);

    priv->rpt_sensor            = sensor;
    priv->rpt_sensor_changed_id =
        sfwsensor_add_valid_changed_handler(priv->rpt_sensor,
                                            sfwreporting_sensor_changed_cb,
                                            self);
}

/* ------------------------------------------------------------------------- *
 * SFWREPORTING_STM_STATE
 * ------------------------------------------------------------------------- */

static const char *
sfwreporting_stm_state_name(const SfwReporting *self)
{
    return sfwreportingstate_repr(sfwreporting_stm_get_state(self));
}

static SfwReportingState
sfwreporting_stm_get_state(const SfwReporting *self)
{
    SfwReportingPrivate *priv = sfwreporting_priv(self);
    return priv ? priv->rpt_state : SFWREPORTINGSTATE_DISABLED;
}

static void
sfwreporting_stm_set_state(SfwReporting *self, SfwReportingState state)
{
    SfwReportingPrivate *priv = sfwreporting_priv(self);
    if( priv->rpt_state == SFWREPORTINGSTATE_FINAL ) {
        /* No way out */
    }
    else if( priv->rpt_state != state ) {
        sfwreporting_log_info("state: %s -> %s",
                              sfwreportingstate_repr(priv->rpt_state),
                              sfwreportingstate_repr(state));
        sfwreporting_stm_leave_state(self);
        priv->rpt_state = state;
        sfwreporting_stm_enter_state(self);
        sfwreporting_stm_eval_state_later(self);
    }
}

static void
sfwreporting_stm_enter_state(SfwReporting *self)
{
    SfwReportingPrivate *priv = sfwreporting_priv(self);
    switch( sfwreporting_stm_get_state(self) ) {
    case SFWREPORTINGSTATE_INITIAL:
        break;
    case SFWREPORTINGSTATE_DISABLED:
        priv->rpt_enable_effective   = ENABLE_DEFAULT;
        priv->rpt_datarate_effective = DEFAULT_DATARATE;
        priv->rpt_override_effective = DEFAULT_OVERRIDE;
        break;
    case SFWREPORTINGSTATE_RETHINK:
        priv->rpt_enable_requested   = priv->rpt_enable_wanted;
        priv->rpt_datarate_requested = priv->rpt_datarate_wanted;
        priv->rpt_override_requested = priv->rpt_override_wanted;
        break;
    case SFWREPORTINGSTATE_STARTING:
        if( priv->rpt_enable_requested != priv->rpt_enable_effective )
            sfwreporting_stm_start_enable(self);
        break;
    case SFWREPORTINGSTATE_CONFIGURE:
        if( priv->rpt_datarate_requested != priv->rpt_datarate_effective )
            sfwreporting_stm_start_datarate(self);
        if( priv->rpt_override_requested != priv->rpt_override_effective )
            sfwreporting_stm_start_override(self);
        break;
    case SFWREPORTINGSTATE_STARTED:
        sfwreporting_set_valid(self, true);
        sfwreporting_set_active(self, true);
        break;
    case SFWREPORTINGSTATE_STOPPING:
        priv->rpt_datarate_effective = DEFAULT_DATARATE;
        priv->rpt_override_effective = DEFAULT_OVERRIDE;
        if( priv->rpt_enable_requested != priv->rpt_enable_effective )
            sfwreporting_stm_start_enable(self);
        break;
    case SFWREPORTINGSTATE_STOPPED:
        sfwreporting_set_valid(self, true);
        break;
    case SFWREPORTINGSTATE_FAILED:
        priv->rpt_enable_effective = ENABLE_INVALID;
        sfwreporting_stm_start_retry_delay(self);
        break;
    case SFWREPORTINGSTATE_FINAL:
        break;
    default:
        abort();
    }
}

static void
sfwreporting_stm_leave_state(SfwReporting *self)
{
    switch( sfwreporting_stm_get_state(self) ) {
    case SFWREPORTINGSTATE_INITIAL:
        break;
    case SFWREPORTINGSTATE_DISABLED:
        break;
    case SFWREPORTINGSTATE_RETHINK:
        break;
    case SFWREPORTINGSTATE_STARTING:
        sfwreporting_stm_cancel_enable(self);
        break;
    case SFWREPORTINGSTATE_CONFIGURE:
        sfwreporting_stm_cancel_datarate(self);
        sfwreporting_stm_cancel_override(self);
        break;
    case SFWREPORTINGSTATE_STARTED:
        sfwreporting_set_active(self, false);
        sfwreporting_set_valid(self, false);
        break;
    case SFWREPORTINGSTATE_STOPPING:
        sfwreporting_stm_cancel_enable(self);
        break;
    case SFWREPORTINGSTATE_STOPPED:
        sfwreporting_set_valid(self, false);
        break;
    case SFWREPORTINGSTATE_FAILED:
        sfwreporting_stm_cancel_retry_delay(self);
        break;
    case SFWREPORTINGSTATE_FINAL:
        break;
    default:
        abort();
    }
}

static gboolean
sfwreporting_stm_eval_state_cb(gpointer aptr)
{
    SfwReporting        *self = aptr;
    SfwReportingPrivate *priv = sfwreporting_priv(self);

    priv->rpt_eval_state_id = 0;

    sfwreporting_log_debug("eval state: %s", sfwreporting_stm_state_name(self));

    switch( sfwreporting_stm_get_state(self) ) {
    case SFWREPORTINGSTATE_INITIAL:
        break;
    case SFWREPORTINGSTATE_DISABLED:
        if( sfwsensor_is_valid(sfwreporting_sensor(self)) )
            sfwreporting_stm_set_state(self, SFWREPORTINGSTATE_RETHINK);
        break;
    case SFWREPORTINGSTATE_RETHINK:
        if( priv->rpt_enable_wanted )
            sfwreporting_stm_set_state(self, SFWREPORTINGSTATE_STARTING);
        else
            sfwreporting_stm_set_state(self, SFWREPORTINGSTATE_STOPPING);
        break;
    case SFWREPORTINGSTATE_STARTING:
        if( sfwreporting_stm_pending_enable(self) )
            break;
        if( priv->rpt_enable_effective != priv->rpt_enable_requested )
            sfwreporting_stm_set_state(self, SFWREPORTINGSTATE_FAILED);
        else
            sfwreporting_stm_set_state(self, SFWREPORTINGSTATE_CONFIGURE);
        break;
    case SFWREPORTINGSTATE_CONFIGURE:
        if( sfwreporting_stm_pending_datarate(self) )
            break;
        if( sfwreporting_stm_pending_override(self) )
            break;

        if( priv->rpt_datarate_effective != priv->rpt_datarate_requested ||
            priv->rpt_override_effective != priv->rpt_override_requested )
            sfwreporting_stm_set_state(self, SFWREPORTINGSTATE_FAILED);
        else
            sfwreporting_stm_set_state(self, SFWREPORTINGSTATE_STARTED);
        break;
    case SFWREPORTINGSTATE_STARTED:
        if( priv->rpt_enable_wanted != priv->rpt_enable_effective ||
            priv->rpt_datarate_wanted != priv->rpt_datarate_effective ||
            priv->rpt_override_wanted != priv->rpt_override_effective )
            sfwreporting_stm_set_state(self, SFWREPORTINGSTATE_RETHINK);
        break;
    case SFWREPORTINGSTATE_STOPPING:
            if( sfwreporting_stm_pending_enable(self) )
            break;
        if( priv->rpt_enable_effective != priv->rpt_enable_requested )
            sfwreporting_stm_set_state(self, SFWREPORTINGSTATE_FAILED);
        else
            sfwreporting_stm_set_state(self, SFWREPORTINGSTATE_STOPPED);
        break;
    case SFWREPORTINGSTATE_STOPPED:
        if( priv->rpt_enable_wanted != priv->rpt_enable_effective )
            sfwreporting_stm_set_state(self, SFWREPORTINGSTATE_RETHINK);
        break;
    case SFWREPORTINGSTATE_FAILED:
        if( !sfwreporting_stm_pending_retry_delay(self) )
            sfwreporting_stm_set_state(self, SFWREPORTINGSTATE_RETHINK);
        break;
    case SFWREPORTINGSTATE_FINAL:
        break;
    default:
        abort();
    }

    return G_SOURCE_REMOVE;
}

static void
sfwreporting_stm_cancel_eval_state(SfwReporting *self)
{
    SfwReportingPrivate *priv = sfwreporting_priv(self);
    if( priv->rpt_eval_state_id ) {
        sfwreporting_log_debug("cancel state eval");
        g_source_remove(priv->rpt_eval_state_id),
            priv->rpt_eval_state_id = 0;
    }
}

static void
sfwreporting_stm_eval_state_later(SfwReporting *self)
{
    SfwReportingPrivate *priv = sfwreporting_priv(self);
    if( sfwreporting_stm_get_state(self) == SFWREPORTINGSTATE_FINAL ) {
        sfwreporting_stm_cancel_eval_state(self);
    }
    else if( !priv->rpt_eval_state_id ) {
        priv->rpt_eval_state_id = g_idle_add(sfwreporting_stm_eval_state_cb, self);
        sfwreporting_log_debug("schedule state eval");
    }
}

static void
sfwreporting_stm_reset_state(SfwReporting *self)
{
    if( self ) {
        sfwreporting_stm_set_state(self, SFWREPORTINGSTATE_DISABLED);
        sfwreporting_stm_eval_state_later(self);
    }
}

/* ------------------------------------------------------------------------- *
 * SFWREPORTING_STM_RETRY
 * ------------------------------------------------------------------------- */

static gboolean
sfwreporting_stm_retry_delay_cb(gpointer aptr)
{
    SfwReporting        *self = aptr;
    SfwReportingPrivate *priv = sfwreporting_priv(self);
    sfwreporting_log_debug("trigger retry");
    priv->rpt_retry_delay_id = 0;
    sfwreporting_stm_eval_state_later(self);
    return G_SOURCE_REMOVE;
}

static void
sfwreporting_stm_start_retry_delay(SfwReporting *self)
{
    SfwReportingPrivate *priv = sfwreporting_priv(self);
    if( gutil_timeout_start(&priv->rpt_retry_delay_id, 5000,
                            sfwreporting_stm_retry_delay_cb, self) )
        sfwreporting_log_debug("schedule retry");
}

static void
sfwreporting_stm_cancel_retry_delay(SfwReporting *self)
{
    SfwReportingPrivate *priv = sfwreporting_priv(self);
    if( gutil_timeout_stop(&priv->rpt_retry_delay_id) )
        sfwreporting_log_debug("cancel retry");
}

static bool
sfwreporting_stm_pending_retry_delay(const SfwReporting *self)
{
    SfwReportingPrivate *priv = sfwreporting_priv(self);
    return priv ? priv->rpt_retry_delay_id : false;
}

/* ------------------------------------------------------------------------- *
 * SFWREPORTING_STM_ENABLE
 * ------------------------------------------------------------------------- */

static void
sfwreporting_stm_enable_cb(GObject *object, GAsyncResult *res, gpointer aptr)
{
    SfwReporting        *self = aptr;
    SfwReportingPrivate *priv = sfwreporting_priv(self);
    GDBusConnection     *con  = G_DBUS_CONNECTION(object);
    bool                 ack  = false;
    GError              *err  = NULL;
    GVariant            *rsp  = g_dbus_connection_call_finish(con, res, &err);

    if( !rsp )
        sfwreporting_log_err("err: %s", error_message(err));
    else
        ack = true;

    if( cancellable_finish(&priv->rpt_enable_cancellable) ) {
        if( ack )
            priv->rpt_enable_effective = priv->rpt_enable_requested;
        sfwreporting_stm_eval_state_later(self);
    }

    g_clear_error(&err);
    gutil_variant_unref(rsp);
    sfwreporting_unref(self);
}

static void
sfwreporting_stm_start_enable(SfwReporting *self)
{
    SfwReportingPrivate *priv       = sfwreporting_priv(self);
    const char          *object     = sfwreporting_object(self);
    const char          *interface  = sfwreporting_interface(self);
    gint                 session_id = sfwreporting_session_id(self);
    const char          *method     = (priv->rpt_enable_requested
                                       ? SFWDBUS_SENSOR_METHOD_START
                                       : SFWDBUS_SENSOR_METHOD_STOP);

    priv->rpt_enable_effective = ENABLE_INVALID;

    cancellable_start(&priv->rpt_enable_cancellable);
    g_dbus_connection_call(sfwreporting_connection(self),
                           SFWDBUS_SERVICE,
                           object,
                           interface,
                           method,
                           g_variant_new("(i)", session_id),
                           NULL,
                           G_DBUS_CALL_FLAGS_NO_AUTO_START,
                           -1,
                           priv->rpt_enable_cancellable,
                           sfwreporting_stm_enable_cb,
                           sfwreporting_ref(self));
}
static void
sfwreporting_stm_cancel_enable(SfwReporting *self)
{
    SfwReportingPrivate *priv = sfwreporting_priv(self);
    cancellable_cancel(&priv->rpt_enable_cancellable);
}

static bool
sfwreporting_stm_pending_enable(SfwReporting *self)
{
    SfwReportingPrivate *priv = sfwreporting_priv(self);
    return priv ? priv->rpt_enable_cancellable : false;
}

/* ------------------------------------------------------------------------- *
 * SFWREPORTING_STM_DATARATE
 * ------------------------------------------------------------------------- */

static void
sfwreporting_stm_datarate_cb(GObject *object, GAsyncResult *res, gpointer aptr)
{
    SfwReporting        *self = aptr;
    SfwReportingPrivate *priv = sfwreporting_priv(self);
    GDBusConnection     *con  = G_DBUS_CONNECTION(object);
    bool                 ack  = false;
    GError              *err  = NULL;
    GVariant            *rsp  = g_dbus_connection_call_finish(con, res, &err);

    if( !rsp )
        sfwreporting_log_err("err: %s", error_message(err));
    else
        ack = true;

    if( cancellable_finish(&priv->rpt_datarate_cancellable) ) {
        if( ack )
            priv->rpt_datarate_effective = priv->rpt_datarate_requested;
        sfwreporting_stm_eval_state_later(self);
    }

    gutil_variant_unref(rsp);
    g_clear_error(&err);
    sfwreporting_unref(self);
}

static void
sfwreporting_stm_start_datarate(SfwReporting *self)
{
    SfwReportingPrivate *priv       = sfwreporting_priv(self);
    const char          *object     = sfwreporting_object(self);
    const char          *interface  = sfwreporting_interface(self);
    gint                 session_id = sfwreporting_session_id(self);

    priv->rpt_datarate_effective = INVALID_DATARATE;

    cancellable_start(&priv->rpt_datarate_cancellable);
    g_dbus_connection_call(sfwreporting_connection(self),
                           SFWDBUS_SERVICE,
                           object,
                           interface,
                           SFWDBUS_SENSOR_METHOD_SET_DATARATE,
                           g_variant_new("(id)", session_id,
                                         priv->rpt_datarate_wanted),
                           NULL,
                           G_DBUS_CALL_FLAGS_NO_AUTO_START,
                           -1,
                           priv->rpt_datarate_cancellable,
                           sfwreporting_stm_datarate_cb,
                           sfwreporting_ref(self));
}
static void
sfwreporting_stm_cancel_datarate(SfwReporting *self)
{
    SfwReportingPrivate *priv = sfwreporting_priv(self);
    cancellable_cancel(&priv->rpt_datarate_cancellable);
}

static bool
sfwreporting_stm_pending_datarate(SfwReporting *self)
{
    SfwReportingPrivate *priv = sfwreporting_priv(self);
    return priv ? priv->rpt_datarate_cancellable : false;
}

/* ------------------------------------------------------------------------- *
 * SFWREPORTING_STM_OVERRIDE
 * ------------------------------------------------------------------------- */

static void
sfwreporting_stm_override_cb(GObject *object, GAsyncResult *res, gpointer aptr)
{
    SfwReporting        *self = aptr;
    SfwReportingPrivate *priv = sfwreporting_priv(self);
    GDBusConnection     *con  = G_DBUS_CONNECTION(object);
    bool                 ack  = false;
    GError              *err  = NULL;
    GVariant            *rsp  = g_dbus_connection_call_finish(con, res, &err);

    if( !rsp )
        sfwreporting_log_err("err: %s", error_message(err));
    else
        ack = true;

    if( cancellable_finish(&priv->rpt_override_cancellable) ) {
        /* Note: Failures to adjust standby override are ignored as
         *       it is not supported by some sensors in some devices.
         */
        if( !ack )
            sfwreporting_log_warning("failed to set standby override");
        priv->rpt_override_effective = priv->rpt_override_requested;
        sfwreporting_stm_eval_state_later(self);
    }

    gutil_variant_unref(rsp);
    g_clear_error(&err);
    sfwreporting_unref(self);
}

static void
sfwreporting_stm_start_override(SfwReporting *self)
{
    SfwReportingPrivate *priv       = sfwreporting_priv(self);
    const char          *object     = sfwreporting_object(self);
    const char          *interface  = sfwreporting_interface(self);
    gint                 session_id = sfwreporting_session_id(self);

    priv->rpt_override_effective = INVALID_OVERRIDE;

    cancellable_start(&priv->rpt_override_cancellable);
    gboolean value = priv->rpt_override_wanted;
    g_dbus_connection_call(sfwreporting_connection(self),
                           SFWDBUS_SERVICE,
                           object,
                           interface,
                           SFWDBUS_SENSOR_METHOD_SET_OVERRIDE,
                           g_variant_new("(ib)", session_id, value),
                           NULL,
                           G_DBUS_CALL_FLAGS_NO_AUTO_START,
                           -1,
                           priv->rpt_override_cancellable,
                           sfwreporting_stm_override_cb,
                           sfwreporting_ref(self));
}
static void
sfwreporting_stm_cancel_override(SfwReporting *self)
{
    SfwReportingPrivate *priv = sfwreporting_priv(self);
    cancellable_cancel(&priv->rpt_override_cancellable);
}

static bool
sfwreporting_stm_pending_override(SfwReporting *self)
{
    SfwReportingPrivate *priv = sfwreporting_priv(self);
    return priv ? priv->rpt_override_cancellable : false;
}
