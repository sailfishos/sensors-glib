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

#include "sfwtypes.h"

#include "sfwlogging.h"
#include "sfwdbus.h"

#include <stdio.h>
#include <inttypes.h>

/* ========================================================================= *
 * Constants
 * ========================================================================= */

#define GRAVITY_EARTH_THOUSANDTH  (0.00980665f)
#define MILLI                     (1e-3f)
#define NANO                      (1e-9f)

/* ========================================================================= *
 * Types
 * ========================================================================= */

typedef const char *(*SfwSampleReprFunc)(const void *aptr);
typedef void (*SfwReadingNormalizeFunc)(SfwReading *reading);

typedef struct SfwSensorInfo
{
    const char             *sti_sensor_name;
    const char             *sti_sensor_object;
    const char             *sti_sensor_interface;
    const char             *sti_value_method;
    size_t                  sti_sample_size;
    SfwSampleReprFunc       sti_sample_repr_cb;
    SfwReadingNormalizeFunc sti_normalize_cb;
} SfwSensorInfo;

/* ========================================================================= *
 * Prototypes
 * ========================================================================= */

/* ------------------------------------------------------------------------- *
 * SFWSENSORID
 * ------------------------------------------------------------------------- */

bool                        sfwsensorid_is_valid   (SfwSensorId id);
static const SfwSensorInfo *sfwsensorid_info       (SfwSensorId id);
const char                 *sfwsensorid_name       (SfwSensorId id);
size_t                      sfwsensorid_sample_size(SfwSensorId id);
const char                 *sfwsensorid_interface  (SfwSensorId id);
const char                 *sfwsensorid_object     (SfwSensorId id);

/* ------------------------------------------------------------------------- *
 * SFWREADING
 * ------------------------------------------------------------------------- */

SfwSensorId                   sfwreading_sensor_id       (const SfwReading *self);
const char                   *sfwreading_repr            (const SfwReading *self);
void                          sfwreading_normalize       (SfwReading *self);
static void                   sfwreading_accelerometer_cb(SfwReading *reading);
static void                   sfwreading_gyroscope_cb    (SfwReading *reading);
static void                   sfwreading_magnetometer_cb (SfwReading *reading);
static void                   sfwreading_compass_cb      (SfwReading *reading);
const SfwSampleXyz           *sfwreading_xyz             (const SfwReading *self);
const SfwSampleAls           *sfwreading_als             (const SfwReading *self);
const SfwSampleProximity     *sfwreading_proximity       (const SfwReading *self);
const SfwSampleOrientation   *sfwreading_orientation     (const SfwReading *self);
const SfwSampleAccelerometer *sfwreading_accelerometer   (const SfwReading *self);
const SfwSampleCompass       *sfwreading_compass         (const SfwReading *self);
const SfwSampleGyroscope     *sfwreading_gyroscope       (const SfwReading *self);
const SfwSampleLid           *sfwreading_lid             (const SfwReading *self);
const SfwSampleHumidity      *sfwreading_humidity        (const SfwReading *self);
const SfwSampleMagnetometer  *sfwreading_magnetometer    (const SfwReading *self);
const SfwSamplePressure      *sfwreading_pressure        (const SfwReading *self);
const SfwSampleRotation      *sfwreading_rotation        (const SfwReading *self);
const SfwSampleStepcounter   *sfwreading_stepcounter     (const SfwReading *self);
const SfwSampleTap           *sfwreading_tap             (const SfwReading *self);
const SfwSampleTemperature   *sfwreading_temperature     (const SfwReading *self);

/* ------------------------------------------------------------------------- *
 * SFWSAMPLE
 * ------------------------------------------------------------------------- */

const char *sfwsampleals_repr          (const SfwSampleAls *self);
const char *sfwsampleproximity_repr    (const SfwSampleProximity *self);
const char *sfwsampleorientation_repr  (const SfwSampleOrientation *self);
const char *sfwsampleaccelerometer_repr(const SfwSampleAccelerometer *self);
const char *sfwsamplecompass_repr      (const SfwSampleCompass *self);
const char *sfwsamplegyroscope_repr    (const SfwSampleGyroscope *self);
const char *sfwsamplelid_repr          (const SfwSampleLid *self);
const char *sfwsamplehumidity_repr     (const SfwSampleHumidity *self);
const char *sfwsamplemagnetometer_repr (const SfwSampleMagnetometer *self);
const char *sfwsamplepressure_repr     (const SfwSamplePressure *self);
const char *sfwsamplerotation_repr     (const SfwSampleRotation *self);
const char *sfwsamplestepcounter_repr  (const SfwSampleStepcounter *self);
const char *sfwsampletap_repr          (const SfwSampleTap *self);
const char *sfwsampletemperature_repr  (const SfwSampleTemperature *self);

/* ------------------------------------------------------------------------- *
 * SFWORIENTATIONSTATE
 * ------------------------------------------------------------------------- */

const char *sfworientationstate_repr(SfwOrientationState state);

/* ------------------------------------------------------------------------- *
 * SFWLIDTYPE
 * ------------------------------------------------------------------------- */

const char *sfwlidtype_repr(SfwLidType type);

/* ------------------------------------------------------------------------- *
 * SFWTAPDIRECTION
 * ------------------------------------------------------------------------- */

const char *sfwtapdirection_repr(SfwTapDirection direction);

/* ------------------------------------------------------------------------- *
 * SFWTAPTYPE
 * ------------------------------------------------------------------------- */

const char *sfwtaptype_repr(SfwTapType type);

/* ========================================================================= *
 * SFWSENSORID
 * ========================================================================= */

#define SAMPLE_REPR(fn) ((SfwSampleReprFunc)(fn))

#define DEFAULT_OBJECT(name) SFWDBUS_MANAGER_OBJECT  "/" name

static const SfwSensorInfo typeinfo_lut[SFW_SENSOR_ID_COUNT] = {
    [SFW_SENSOR_ID_INVALID] = {
    },

    [SFW_SENSOR_ID_PROXIMITY] = {
        .sti_sensor_name      = SFWDBUS_SENSOR_NAME_PROXIMITY,
        .sti_sensor_object    = SFWDBUS_SENSOR_OBJECT_PROXIMITY,
        .sti_sensor_interface = SFWDBUS_SENSOR_INTERFACE_PROXIMITY,
        .sti_value_method     = SFWDBUS_SENSOR_METHOD_GET_PROXIMITY,
        .sti_sample_size      = sizeof(SfwSampleProximity),
        .sti_sample_repr_cb   = SAMPLE_REPR(sfwsampleproximity_repr),
        .sti_normalize_cb     = NULL, // if needed: reading_proximity_cb,
    },
    [SFW_SENSOR_ID_ALS] = {
        .sti_sensor_name      = SFWDBUS_SENSOR_NAME_ALS,
        .sti_sensor_object    = SFWDBUS_SENSOR_OBJECT_ALS,
        .sti_sensor_interface = SFWDBUS_SENSOR_INTERFACE_ALS,
        .sti_value_method     = SFWDBUS_SENSOR_METHOD_GET_ALS,
        .sti_sample_size      = sizeof(SfwSampleAls),
        .sti_sample_repr_cb   = SAMPLE_REPR(sfwsampleals_repr),
        .sti_normalize_cb     = NULL, // if needed: reading_als_cb,
    },
    [SFW_SENSOR_ID_ORIENTATION] = {
        .sti_sensor_name      = SFWDBUS_SENSOR_NAME_ORIENTATION,
        .sti_sensor_object    = SFWDBUS_SENSOR_OBJECT_ORIENTATION,
        .sti_sensor_interface = SFWDBUS_SENSOR_INTERFACE_ORIENTATION,
        .sti_value_method     = SFWDBUS_SENSOR_METHOD_GET_ORIENTATION,
        .sti_sample_size      = sizeof(SfwSampleOrientation),
        .sti_sample_repr_cb   = SAMPLE_REPR(sfwsampleorientation_repr),
        .sti_normalize_cb     = NULL, // if needed: reading_orientation_cb,
    },
    [SFW_SENSOR_ID_ACCELEROMETER] = {
        .sti_sensor_name      = SFWDBUS_SENSOR_NAME_ACCELEROMETER,
        .sti_sensor_object    = SFWDBUS_SENSOR_OBJECT_ACCELEROMETER,
        .sti_sensor_interface = SFWDBUS_SENSOR_INTERFACE_ACCELEROMETER,
        .sti_value_method     = SFWDBUS_SENSOR_METHOD_GET_ACCELEROMETER,
        .sti_sample_size      = sizeof(SfwSampleAccelerometer),
        .sti_sample_repr_cb   = SAMPLE_REPR(sfwsampleaccelerometer_repr),
        .sti_normalize_cb     = sfwreading_accelerometer_cb,
    },
    [SFW_SENSOR_ID_COMPASS] = {
        .sti_sensor_name      = SFWDBUS_SENSOR_NAME_COMPASS,
        .sti_sensor_object    = SFWDBUS_SENSOR_OBJECT_COMPASS,
        .sti_sensor_interface = SFWDBUS_SENSOR_INTERFACE_COMPASS,
        .sti_value_method     = SFWDBUS_SENSOR_METHOD_GET_COMPASS,
        .sti_sample_size      = sizeof(SfwSampleCompass),
        .sti_sample_repr_cb   = SAMPLE_REPR(sfwsamplecompass_repr),
        .sti_normalize_cb     = sfwreading_compass_cb,
    },
    [SFW_SENSOR_ID_GYROSCOPE] = {
        .sti_sensor_name      = SFWDBUS_SENSOR_NAME_GYROSCOPE,
        .sti_sensor_object    = SFWDBUS_SENSOR_OBJECT_GYROSCOPE,
        .sti_sensor_interface = SFWDBUS_SENSOR_INTERFACE_GYROSCOPE,
        .sti_value_method     = SFWDBUS_SENSOR_METHOD_GET_GYROSCOPE,
        .sti_sample_size      = sizeof(SfwSampleGyroscope),
        .sti_sample_repr_cb   = SAMPLE_REPR(sfwsamplegyroscope_repr),
        .sti_normalize_cb     = sfwreading_gyroscope_cb,
    },
    [SFW_SENSOR_ID_LID] = {
        .sti_sensor_name      = SFWDBUS_SENSOR_NAME_LID,
        .sti_sensor_object    = SFWDBUS_SENSOR_OBJECT_LID,
        .sti_sensor_interface = SFWDBUS_SENSOR_INTERFACE_LID,
        .sti_value_method     = SFWDBUS_SENSOR_METHOD_GET_LID,
        .sti_sample_size      = sizeof(SfwSampleLid),
        .sti_sample_repr_cb   = SAMPLE_REPR(sfwsamplelid_repr),
        .sti_normalize_cb     = NULL, // if needed: reading_lid_cb,
    },
    [SFW_SENSOR_ID_HUMIDITY] = {
        .sti_sensor_name      = SFWDBUS_SENSOR_NAME_HUMIDITY,
        .sti_sensor_object    = SFWDBUS_SENSOR_OBJECT_HUMIDITY,
        .sti_sensor_interface = SFWDBUS_SENSOR_INTERFACE_HUMIDITY,
        .sti_value_method     = SFWDBUS_SENSOR_METHOD_GET_HUMIDITY,
        .sti_sample_size      = sizeof(SfwSampleHumidity),
        .sti_sample_repr_cb   = SAMPLE_REPR(sfwsamplehumidity_repr),
        .sti_normalize_cb     = NULL, // if needed: reading_humidity_cb,
    },
    [SFW_SENSOR_ID_MAGNETOMETER] = {
        .sti_sensor_name      = SFWDBUS_SENSOR_NAME_MAGNETOMETER,
        .sti_sensor_object    = SFWDBUS_SENSOR_OBJECT_MAGNETOMETER,
        .sti_sensor_interface = SFWDBUS_SENSOR_INTERFACE_MAGNETOMETER,
        .sti_value_method     = SFWDBUS_SENSOR_METHOD_GET_MAGNETOMETER,
        .sti_sample_size      = sizeof(SfwSampleMagnetometer),
        .sti_sample_repr_cb   = SAMPLE_REPR(sfwsamplemagnetometer_repr),
        .sti_normalize_cb     = sfwreading_magnetometer_cb,
    },
    [SFW_SENSOR_ID_PRESSURE] = {
        .sti_sensor_name      = SFWDBUS_SENSOR_NAME_PRESSURE,
        .sti_sensor_object    = SFWDBUS_SENSOR_OBJECT_PRESSURE,
        .sti_sensor_interface = SFWDBUS_SENSOR_INTERFACE_PRESSURE,
        .sti_value_method     = SFWDBUS_SENSOR_METHOD_GET_PRESSURE,
        .sti_sample_size      = sizeof(SfwSamplePressure),
        .sti_sample_repr_cb   = SAMPLE_REPR(sfwsamplepressure_repr),
        .sti_normalize_cb     = NULL, // if needed: reading_pressure_cb,
    },
    [SFW_SENSOR_ID_ROTATION] = {
        .sti_sensor_name      = SFWDBUS_SENSOR_NAME_ROTATION,
        .sti_sensor_object    = SFWDBUS_SENSOR_OBJECT_ROTATION,
        .sti_sensor_interface = SFWDBUS_SENSOR_INTERFACE_ROTATION,
        .sti_value_method     = SFWDBUS_SENSOR_METHOD_GET_ROTATION,
        .sti_sample_size      = sizeof(SfwSampleRotation),
        .sti_sample_repr_cb   = SAMPLE_REPR(sfwsamplerotation_repr),
        .sti_normalize_cb     = NULL, // if needed: reading_rotation_cb,
    },
    [SFW_SENSOR_ID_STEPCOUNTER] = {
        .sti_sensor_name      = SFWDBUS_SENSOR_NAME_STEPCOUNTER,
        .sti_sensor_object    = SFWDBUS_SENSOR_OBJECT_STEPCOUNTER,
        .sti_sensor_interface = SFWDBUS_SENSOR_INTERFACE_STEPCOUNTER,
        .sti_value_method     = SFWDBUS_SENSOR_METHOD_GET_STEPCOUNTER,
        .sti_sample_size      = sizeof(SfwSampleStepcounter),
        .sti_sample_repr_cb   = SAMPLE_REPR(sfwsamplestepcounter_repr),
        .sti_normalize_cb     = NULL, // if needed: reading_stepcounter_cb,
    },
    [SFW_SENSOR_ID_TAP] = {
        .sti_sensor_name      = SFWDBUS_SENSOR_NAME_TAP,
        .sti_sensor_object    = SFWDBUS_SENSOR_OBJECT_TAP,
        .sti_sensor_interface = SFWDBUS_SENSOR_INTERFACE_TAP,
        .sti_value_method     = SFWDBUS_SENSOR_METHOD_GET_TAP,
        .sti_sample_size      = sizeof(SfwSampleTap),
        .sti_sample_repr_cb   = SAMPLE_REPR(sfwsampletap_repr),
        .sti_normalize_cb     = NULL, // if needed: reading_tap_cb,
    },
    [SFW_SENSOR_ID_TEMPERATURE] = {
        .sti_sensor_name      = SFWDBUS_SENSOR_NAME_TEMPERATURE,
        .sti_sensor_object    = SFWDBUS_SENSOR_OBJECT_TEMPERATURE,
        .sti_sensor_interface = SFWDBUS_SENSOR_INTERFACE_TEMPERATURE,
        .sti_value_method     = SFWDBUS_SENSOR_METHOD_GET_TEMPERATURE,
        .sti_sample_size      = sizeof(SfwSampleTemperature),
        .sti_sample_repr_cb   = SAMPLE_REPR(sfwsampletemperature_repr),
        .sti_normalize_cb     = NULL, // if needed: reading_temperature_cb,
    },
};

bool
sfwsensorid_is_valid(SfwSensorId id)
{
    return SFW_SENSOR_ID_INVALID < id && id < SFW_SENSOR_ID_COUNT;
}

static const SfwSensorInfo *
sfwsensorid_info(SfwSensorId id)
{
    return sfwsensorid_is_valid(id) ? &typeinfo_lut[id] : NULL;
}

const char *
sfwsensorid_name(SfwSensorId id)
{
    const SfwSensorInfo *info = sfwsensorid_info(id);
    return info ? info->sti_sensor_name : NULL;
}

size_t
sfwsensorid_sample_size(SfwSensorId id)
{
    const SfwSensorInfo *info = sfwsensorid_info(id);
    return info ? info->sti_sample_size : 0;
}

const char *
sfwsensorid_interface(SfwSensorId id)
{
    const SfwSensorInfo *info = sfwsensorid_info(id);
    return info ? info->sti_sensor_interface : NULL;
}

const char *
sfwsensorid_object(SfwSensorId id)
{
    const SfwSensorInfo *info = sfwsensorid_info(id);
    return info ? info->sti_sensor_object : NULL;
}

/* ========================================================================= *
 * SFWREADING
 * ========================================================================= */

static inline void sfwreading_normalize_level(int32_t *plevel)
{
    int32_t level = *plevel;

    *plevel = (level <= 0) ? 0 : (level >= 3) ? 100 : (level * 100 / 3);
}

SfwSensorId
sfwreading_sensor_id(const SfwReading *self)
{
    return self ? self->sensor_id : SFW_SENSOR_ID_INVALID;
}

const char *
sfwreading_repr(const SfwReading *self)
{
    const SfwSensorInfo *info = sfwsensorid_info(sfwreading_sensor_id(self));
    if( info ) {
        static char buf[128];
        snprintf(buf, sizeof buf, "%s(%s)",
                 info->sti_sensor_name,
                 info->sti_sample_repr_cb(&self->sample));
        return buf;
    }
    return "???";
}

void
sfwreading_normalize(SfwReading *self)
{
    /* The idea here is to perform similar scaling and unit conversion
     * operations as what qtsensors is already doing.
     */
    const SfwSensorInfo *info = sfwsensorid_info(sfwreading_sensor_id(self));
    if( info ) {
        if( info->sti_normalize_cb )
            info->sti_normalize_cb(self);
    }
}

static void
sfwreading_accelerometer_cb(SfwReading *reading)
{
    reading->sample.accelerometer.x *= GRAVITY_EARTH_THOUSANDTH;
    reading->sample.accelerometer.y *= GRAVITY_EARTH_THOUSANDTH;
    reading->sample.accelerometer.z *= GRAVITY_EARTH_THOUSANDTH;
}

static void
sfwreading_gyroscope_cb(SfwReading *reading)
{
    reading->sample.gyroscope.x *= MILLI;
    reading->sample.gyroscope.y *= MILLI;
    reading->sample.gyroscope.z *= MILLI;
}

static void
sfwreading_magnetometer_cb(SfwReading *reading)
{
#if 0 // FIXME check datatype - int or float?
    reading->sample.magnetometer.x *= NANO;
    reading->sample.magnetometer.y *= NANO;
    reading->sample.magnetometer.z *= NANO;
#endif
    sfwreading_normalize_level(&reading->sample.magnetometer.level);
}

static void
sfwreading_compass_cb(SfwReading *reading)
{
    sfwreading_normalize_level(&reading->sample.compass.level);
}

const SfwSampleXyz *
sfwreading_xyz(const SfwReading *self)
{
    const SfwSampleXyz *xyz = NULL;
    switch( self->sensor_id ) {
    case SFW_SENSOR_ID_ACCELEROMETER:
    case SFW_SENSOR_ID_GYROSCOPE:
    case SFW_SENSOR_ID_ROTATION:
        xyz = &self->sample.xyz;
        break;
    default:
        sfwlog_warning("%s does not have xyz data",
                       sfwsensorid_name(self->sensor_id));
        break;
    }
    return xyz;
}

const SfwSampleAls *
sfwreading_als(const SfwReading *self)
{
    const SfwSampleAls *als = NULL;
    if( self->sensor_id != SFW_SENSOR_ID_ALS )
        sfwlog_warning("%s does not have als data",
                       sfwsensorid_name(self->sensor_id));
    else
        als = &self->sample.als;
    return als;
}

const SfwSampleProximity *
sfwreading_proximity(const SfwReading *self)
{
    const SfwSampleProximity *proximity = NULL;
    if( self->sensor_id != SFW_SENSOR_ID_PROXIMITY )
        sfwlog_warning("%s does not have proximity data",
                       sfwsensorid_name(self->sensor_id));
    else
        proximity = &self->sample.proximity;
    return proximity;
}

const SfwSampleOrientation *
sfwreading_orientation(const SfwReading *self)
{
    const SfwSampleOrientation *orientation = NULL;
    if( self->sensor_id != SFW_SENSOR_ID_ORIENTATION )
        sfwlog_warning("%s does not have orientation data",
                       sfwsensorid_name(self->sensor_id));
    else
        orientation = &self->sample.orientation;
    return orientation;
}

const SfwSampleAccelerometer *
sfwreading_accelerometer(const SfwReading *self)
{
    const SfwSampleAccelerometer *accelerometer = NULL;
    if( self->sensor_id != SFW_SENSOR_ID_ACCELEROMETER )
        sfwlog_warning("%s does not have accelerometer data",
                       sfwsensorid_name(self->sensor_id));
    else
        accelerometer = &self->sample.accelerometer;
    return accelerometer;
}

const SfwSampleCompass *
sfwreading_compass(const SfwReading *self)
{
    const SfwSampleCompass *compass = NULL;
    if( self->sensor_id != SFW_SENSOR_ID_COMPASS )
        sfwlog_warning("%s does not have compass data",
                       sfwsensorid_name(self->sensor_id));
    else
        compass = &self->sample.compass;
    return compass;
}

const SfwSampleGyroscope *
sfwreading_gyroscope(const SfwReading *self)
{
    const SfwSampleGyroscope *gyroscope = NULL;
    if( self->sensor_id != SFW_SENSOR_ID_GYROSCOPE )
        sfwlog_warning("%s does not have gyroscope data",
                       sfwsensorid_name(self->sensor_id));
    else
        gyroscope = &self->sample.gyroscope;
    return gyroscope;
}

const SfwSampleLid *
sfwreading_lid(const SfwReading *self)
{
    const SfwSampleLid *lid = NULL;
    if( self->sensor_id != SFW_SENSOR_ID_LID )
        sfwlog_warning("%s does not have lid data",
                       sfwsensorid_name(self->sensor_id));
    else
        lid = &self->sample.lid;
    return lid;
}

const SfwSampleHumidity *
sfwreading_humidity(const SfwReading *self)
{
    const SfwSampleHumidity *humidity = NULL;
    if( self->sensor_id != SFW_SENSOR_ID_HUMIDITY )
        sfwlog_warning("%s does not have humidity data",
                       sfwsensorid_name(self->sensor_id));
    else
        humidity = &self->sample.humidity;
    return humidity;
}

const SfwSampleMagnetometer *
sfwreading_magnetometer(const SfwReading *self)
{
    const SfwSampleMagnetometer *magnetometer = NULL;
    if( self->sensor_id != SFW_SENSOR_ID_MAGNETOMETER )
        sfwlog_warning("%s does not have magnetometer data",
                       sfwsensorid_name(self->sensor_id));
    else
        magnetometer = &self->sample.magnetometer;
    return magnetometer;
}

const SfwSamplePressure *
sfwreading_pressure(const SfwReading *self)
{
    const SfwSamplePressure *pressure = NULL;
    if( self->sensor_id != SFW_SENSOR_ID_PRESSURE )
        sfwlog_warning("%s does not have pressure data",
                       sfwsensorid_name(self->sensor_id));
    else
        pressure = &self->sample.pressure;
    return pressure;
}

const SfwSampleRotation *
sfwreading_rotation(const SfwReading *self)
{
    const SfwSampleRotation *rotation = NULL;
    if( self->sensor_id != SFW_SENSOR_ID_ROTATION )
        sfwlog_warning("%s does not have rotation data",
                       sfwsensorid_name(self->sensor_id));
    else
        rotation = &self->sample.rotation;
    return rotation;
}

const SfwSampleStepcounter *
sfwreading_stepcounter(const SfwReading *self)
{
    const SfwSampleStepcounter *stepcounter = NULL;
    if( self->sensor_id != SFW_SENSOR_ID_STEPCOUNTER )
        sfwlog_warning("%s does not have stepcounter data",
                       sfwsensorid_name(self->sensor_id));
    else
        stepcounter = &self->sample.stepcounter;
    return stepcounter;
}

const SfwSampleTap *
sfwreading_tap(const SfwReading *self)
{
    const SfwSampleTap *tap = NULL;
    if( self->sensor_id != SFW_SENSOR_ID_TAP )
        sfwlog_warning("%s does not have tap data",
                       sfwsensorid_name(self->sensor_id));
    else
        tap = &self->sample.tap;
    return tap;
}

const SfwSampleTemperature *
sfwreading_temperature(const SfwReading *self)
{
    const SfwSampleTemperature *temperature = NULL;
    if( self->sensor_id != SFW_SENSOR_ID_TEMPERATURE )
        sfwlog_warning("%s does not have temperature data",
                       sfwsensorid_name(self->sensor_id));
    else
        temperature = &self->sample.temperature;
    return temperature;
}

/* ========================================================================= *
 * SFWSAMPLE
 * ========================================================================= */

const char *
sfwsampleals_repr(const SfwSampleAls *self)
{
    static char buf[80];
    snprintf(buf, sizeof buf,
             "time=%"PRIu64" lux=%"PRIu32,
             self->timestamp,
             self->value);
    return buf;
}

const char *
sfwsampleproximity_repr(const SfwSampleProximity *self)
{
    static char buf[80];
    snprintf(buf, sizeof buf,
             "time=%"PRIu64" distance=%"PRIu32" proximity=%s",
             self->timestamp,
             self->distance,
             self->proximity ? "true" : "false");
    return buf;
}

const char *
sfwsampleorientation_repr(const SfwSampleOrientation *self)
{
    static char buf[80];
    snprintf(buf, sizeof buf,
             "time=%"PRIu64" state=%s",
             self->timestamp,
             sfworientationstate_repr(self->state));
    return buf;
}

const char *
sfwsampleaccelerometer_repr(const SfwSampleAccelerometer *self)
{
    static char buf[80];
    snprintf(buf, sizeof buf,
             "time=%"PRIu64" x=%g y=%g z=%g",
             self->timestamp,
             self->x,
             self->y,
             self->z);
    return buf;
}

const char *
sfwsamplecompass_repr(const SfwSampleCompass *self)
{
    static char buf[80];
    snprintf(buf, sizeof buf,
             "time=%"PRIu64" deg=%"PRId32" raw=%"PRId32" cor=%"PRId32" lev=%"PRId32,
             self->timestamp,
             self->degrees,
             self->raw_degrees,
             self->corrected_degrees,
             self->level);
    return buf;
}

const char *
sfwsamplegyroscope_repr(const SfwSampleGyroscope *self)
{
    static char buf[80];
    snprintf(buf, sizeof buf,
             "time=%"PRIu64" x=%g y=%g z=%g",
             self->timestamp,
             self->x,
             self->y,
             self->z);
    return buf;
}

const char *
sfwsamplelid_repr(const SfwSampleLid *self)
{
    static char buf[80];
    snprintf(buf, sizeof buf,
             "time=%"PRIu64" type=%s value=%"PRIu32,
             self->timestamp,
             sfwlidtype_repr(self->type),
             self->value);
    return buf;
}

const char *
sfwsamplehumidity_repr(const SfwSampleHumidity *self)
{
    static char buf[80];
    snprintf(buf, sizeof buf,
             "time=%"PRIu64" humidity=%"PRIu32,
             self->timestamp,
             self->value);
    return buf;
}

const char *
sfwsamplemagnetometer_repr(const SfwSampleMagnetometer *self)
{
    static char buf[80];
    snprintf(buf, sizeof buf,
             "time=%"PRIu64
             " x=%"PRId32" y=%"PRId32" z=%"PRId32
             " rx=%"PRId32" ry=%"PRId32" rz=%"PRId32
             " level=%"PRId32,
             self->timestamp,
             self->x,
             self->y,
             self->z,
             self->rx,
             self->ry,
             self->rz,
             self->level);
    return buf;
}

const char *
sfwsamplepressure_repr(const SfwSamplePressure *self)
{
    static char buf[80];
    snprintf(buf, sizeof buf,
             "time=%"PRIu64" pressure=%"PRIu32,
             self->timestamp,
             self->value);
    return buf;
}

const char *
sfwsamplerotation_repr(const SfwSampleRotation *self)
{
    static char buf[80];
    snprintf(buf, sizeof buf,
             "time=%"PRIu64" x=%g y=%g z=%g",
             self->timestamp,
             self->x,
             self->y,
             self->z);
    return buf;
}

const char *
sfwsamplestepcounter_repr(const SfwSampleStepcounter *self)
{
    static char buf[80];
    snprintf(buf, sizeof buf,
             "time=%"PRIu64" stepcount=%"PRIu32,
             self->timestamp,
             self->value);
    return buf;
}

const char *
sfwsampletap_repr(const SfwSampleTap *self)
{
    static char buf[80];
    snprintf(buf, sizeof buf, "time=%"PRIu64" direction=%s type=%s",
             self->timestamp,
             sfwtapdirection_repr(self->direction),
             sfwtaptype_repr(self->type));
    return buf;
}

const char *
sfwsampletemperature_repr(const SfwSampleTemperature *self)
{
    static char buf[80];
    snprintf(buf, sizeof buf,
             "time=%"PRIu64" temperature=%"PRIu32,
             self->temperature_timestamp,
             self->temperature_value);
    return buf;
}

/* ========================================================================= *
 * SFWORIENTATIONSTATE
 * ========================================================================= */

const char *
sfworientationstate_repr(SfwOrientationState state)
{
    const char *name = "UNKNOWN";
    switch( state ) {
    case SFW_ORIENTATION_UNDEFINED:   name = "UNDEFINED";   break;
    case SFW_ORIENTATION_LEFT_UP:     name = "LEFT_UP";     break;
    case SFW_ORIENTATION_RIGHT_UP:    name = "RIGHT_UP";    break;
    case SFW_ORIENTATION_BOTTOM_UP:   name = "BOTTOM_UP";   break;
    case SFW_ORIENTATION_BOTTOM_DOWN: name = "BOTTOM_DOWN"; break;
    case SFW_ORIENTATION_FACE_DOWN:   name = "FACE_DOWN";   break;
    case SFW_ORIENTATION_FACE_UP:     name = "FACE_UP";     break;
    default: break;
    }
    return name;
}

/* ========================================================================= *
 * SFWLIDTYPE
 * ========================================================================= */

/** Translate orientation state to human readable form */
const char *
sfwlidtype_repr(SfwLidType type)
{
    const char *repr = "INVALID";
    switch( type ) {
    case SFW_LID_TYPE_UNKNOWN: repr = "UNKNOWN"; break;
    case SFW_LID_TYPE_FRONT:   repr = "FRONT";   break;
    case SFW_LID_TYPE_BACK:    repr = "BACK";    break;
    default: break;
    }
    return repr;
}

/* ========================================================================= *
 * SFWTAPDIRECTION
 * ========================================================================= */

const char *
sfwtapdirection_repr(SfwTapDirection direction)
{
    const char *repr = "INVALID";
    switch( direction ) {
    case SFW_TAP_DIRECTION_X:           repr = "X";          break;
    case SFW_TAP_DIRECTION_Y:           repr = "Y";          break;
    case SFW_TAP_DIRECTION_Z:           repr = "Z";          break;
    case SFW_TAP_DIRECTION_LEFT_RIGHT:  repr = "LEFT_RIGHT"; break;
    case SFW_TAP_DIRECTION_RIGHT_LEFT:  repr = "RIGHT_LEFT"; break;
    case SFW_TAP_DIRECTION_TOP_BOTTOM:  repr = "TOP_BOTTOM"; break;
    case SFW_TAP_DIRECTION_BOTTOM_TOP:  repr = "BOTTOM_TOP"; break;
    case SFW_TAP_DIRECTION_FACE_BACK:   repr = "FACE_BACK";  break;
    case SFW_TAP_DIRECTION_BACK_FACE:   repr = "BACK_FACE";  break;
    default: break;
    }
    return repr;
}

/* ========================================================================= *
 * SFWTAPTYPE
 * ========================================================================= */

const char *
sfwtaptype_repr(SfwTapType type)
{
    const char *repr = "INVALID";
    switch( type ) {
    case SFW_TAP_TYPE_NONE:       repr = "NONE";       break;
    case SFW_TAP_TYPE_DOUBLE_TAP: repr = "DOUBLE_TAP"; break;
    case SFW_TAP_TYPE_SINGLE_TAP: repr = "SINGLE_TAP"; break;
    default: break;
    }
    return repr;
}
