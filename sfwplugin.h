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

#ifndef SFWPLUGIN_H_
# define SFWPLUGIN_H_

# include "sfwtypes.h"

# include <glib-object.h>

G_BEGIN_DECLS

# pragma GCC visibility push(default)

/* ========================================================================= *
 * Types
 * ========================================================================= */

typedef void (*SfwPluginHandler)(SfwPlugin *sfwplugin, gpointer aptr);

/* ========================================================================= *
 * Prototypes
 * ========================================================================= */

/* ------------------------------------------------------------------------- *
 * SFWPLUGIN_LIFECYCLE
 * ------------------------------------------------------------------------- */

SfwPlugin *sfwplugin_instance(SfwSensorId id);
SfwPlugin *sfwplugin_ref     (SfwPlugin *self);
void       sfwplugin_unref   (SfwPlugin *self);
void       sfwplugin_unref_at(SfwPlugin **pself);

/* ------------------------------------------------------------------------- *
 * SFWPLUGIN_VALID
 * ------------------------------------------------------------------------- */

bool sfwplugin_is_valid(const SfwPlugin *self);

/* ------------------------------------------------------------------------- *
 * SFWPLUGIN_SIGNALS
 * ------------------------------------------------------------------------- */

gulong sfwplugin_add_valid_changed_handler(SfwPlugin *self, SfwPluginHandler handler, gpointer aptr);
void   sfwplugin_remove_handler           (SfwPlugin *self, gulong id);
void   sfwplugin_remove_handler_at        (SfwPlugin *self, gulong *pid);

/* ------------------------------------------------------------------------- *
 * SFWPLUGIN_ACCESSORS
 * ------------------------------------------------------------------------- */

SfwSensorId  sfwplugin_id       (const SfwPlugin *self);
SfwService  *sfwplugin_service  (const SfwPlugin *self);
const char  *sfwplugin_name     (const SfwPlugin *self);
const char  *sfwplugin_object   (const SfwPlugin *self);
const char  *sfwplugin_interface(const SfwPlugin *self);

# pragma GCC visibility pop

G_END_DECLS

#endif /* SFWPLUGIN_H_ */
