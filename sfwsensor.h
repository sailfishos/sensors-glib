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

#ifndef SFWSENSOR_H_
# define SFWSENSOR_H_

# include "sfwtypes.h"

# include <glib-object.h>

G_BEGIN_DECLS

# pragma GCC visibility push(default)

/* ========================================================================= *
 * Types
 * ========================================================================= */

typedef void (*SfwSensorHandler)(SfwSensor *sfwsensor, gpointer aptr);

/* ========================================================================= *
 * Prototypes
 * ========================================================================= */

/* ------------------------------------------------------------------------- *
 * SFWSENSOR_LIFECYCLE
 * ------------------------------------------------------------------------- */

SfwSensor *sfwsensor_new     (SfwSensorId id);
SfwSensor *sfwsensor_ref     (SfwSensor *self);
void       sfwsensor_unref   (SfwSensor *self);
void       sfwsensor_unref_cb(gpointer self);
void       sfwsensor_unref_at(SfwSensor **pself);

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

bool sfwsensor_is_valid(const SfwSensor *self);

/* ------------------------------------------------------------------------- *
 * SFWSENSOR_ACTIVE
 * ------------------------------------------------------------------------- */

bool sfwsensor_is_active(const SfwSensor *self);

/* ------------------------------------------------------------------------- *
 * SFWSENSOR_READING
 * ------------------------------------------------------------------------- */

SfwReading *sfwsensor_reading(SfwSensor *self);

/* ------------------------------------------------------------------------- *
 * SFWSENSOR_SIGNALS
 * ------------------------------------------------------------------------- */

gulong sfwsensor_add_valid_changed_handler  (SfwSensor *self, SfwSensorHandler handler, gpointer aptr);
gulong sfwsensor_add_active_changed_handler (SfwSensor *self, SfwSensorHandler handler, gpointer aptr);
gulong sfwsensor_add_reading_changed_handler(SfwSensor *self, SfwSensorHandler handler, gpointer aptr);
void   sfwsensor_remove_handler             (SfwSensor *self, gulong id);
void   sfwsensor_remove_handler_at          (SfwSensor *self, gulong *pid);

/* ------------------------------------------------------------------------- *
 * SFWSENSOR_ACCESSORS
 * ------------------------------------------------------------------------- */

int         sfwsensor_session_id(const SfwSensor *self);
SfwPlugin  *sfwsensor_plugin    (const SfwSensor *self);
SfwService *sfwsensor_service   (const SfwSensor *self);
const char *sfwsensor_name      (const SfwSensor *self);
const char *sfwsensor_object    (const SfwSensor *self);
const char *sfwsensor_interface (const SfwSensor *self);

# pragma GCC visibility pop

G_END_DECLS

#endif /* SFWSENSOR_H_ */
