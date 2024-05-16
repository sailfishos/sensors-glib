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

#ifndef SFWREPORTING_H_
# define SFWREPORTING_H_

# include "sfwtypes.h"

# include <glib-object.h>

G_BEGIN_DECLS

# pragma GCC visibility push(default)

/* ========================================================================= *
 * Types
 * ========================================================================= */

typedef void (*SfwReportingHandler)(SfwReporting *sfwreporting, gpointer aptr);

/* ========================================================================= *
 * Prototypes
 * ========================================================================= */

/* ------------------------------------------------------------------------- *
 * SFWREPORTING_LIFECYCLE
 * ------------------------------------------------------------------------- */

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

bool sfwreporting_is_valid(const SfwReporting *self);

/* ------------------------------------------------------------------------- *
 * SFWREPORTING_ACTIVE
 * ------------------------------------------------------------------------- */

bool sfwreporting_is_active(const SfwReporting *self);

/* ------------------------------------------------------------------------- *
 * SFWREPORTING_SIGNALS
 * ------------------------------------------------------------------------- */

gulong sfwreporting_add_valid_changed_handler (SfwReporting *self, SfwReportingHandler handler, gpointer aptr);
gulong sfwreporting_add_active_changed_handler(SfwReporting *self, SfwReportingHandler handler, gpointer aptr);
void   sfwreporting_remove_handler            (SfwReporting *self, gulong id);

/* ------------------------------------------------------------------------- *
 * SFWREPORTING_ACCESSORS
 * ------------------------------------------------------------------------- */

SfwSensor *sfwreporting_sensor(const SfwReporting *self);

# pragma GCC visibility pop

G_END_DECLS

#endif /* SFWREPORTING_H_ */
