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

#ifndef SFWTYPES_H_
# define SFWTYPES_H_

# include <stdint.h>
# include <stdbool.h>

# include <glib.h>

G_BEGIN_DECLS

# pragma GCC visibility push(default)

/* ========================================================================= *
 * Types
 * ========================================================================= */

/** Sensorfwd service availability on D-Bus (shared instance)
 */
typedef struct SfwService             SfwService;

/** Sensor specific plugin and D-Bus object (shared instance / sensor type)
 */
typedef struct SfwPlugin              SfwPlugin;

/** Sensor specific session and data connection
 */
typedef struct SfwSensor              SfwSensor;

/** Sensor setup and control: start/stop/datarate/etc
 */
typedef struct SfwReporting           SfwReporting;

/** Catch-all data structure used for sensor data reporting
 */
typedef struct SfwReading             SfwReading;

/** Catch-all data structure that can hold a sample for any sensor
 */
typedef union  SfwSample              SfwSample;

typedef struct SfwSampleXyz           SfwSampleXyz;
typedef struct SfwSampleXyz           SfwSampleAccelerometer;
typedef struct SfwSampleXyz           SfwSampleGyroscope;
typedef struct SfwSampleXyz           SfwSampleRotation;
typedef struct SfwSampleAls           SfwSampleAls;
typedef struct SfwSampleCompass       SfwSampleCompass;
typedef struct SfwSampleHumidity      SfwSampleHumidity;
typedef struct SfwSampleLid           SfwSampleLid;
typedef struct SfwSampleMagnetometer  SfwSampleMagnetometer;
typedef struct SfwSampleOrientation   SfwSampleOrientation;
typedef struct SfwSamplePressure      SfwSamplePressure;
typedef struct SfwSampleProximity     SfwSampleProximity;
typedef struct SfwSampleStepcounter   SfwSampleStepcounter;
typedef struct SfwSampleTap           SfwSampleTap;
typedef struct SfwSampleTemperature   SfwSampleTemperature;

/** Supported / known sensor types
 */
typedef enum SfwSensorId
{
    SFW_SENSOR_ID_INVALID,
    SFW_SENSOR_ID_PROXIMITY,
    SFW_SENSOR_ID_ALS,
    SFW_SENSOR_ID_ORIENTATION,
    SFW_SENSOR_ID_ACCELEROMETER,
    SFW_SENSOR_ID_COMPASS,
    SFW_SENSOR_ID_GYROSCOPE,
    SFW_SENSOR_ID_LID,
    SFW_SENSOR_ID_HUMIDITY,
    SFW_SENSOR_ID_MAGNETOMETER,
    SFW_SENSOR_ID_PRESSURE,
    SFW_SENSOR_ID_ROTATION,
    SFW_SENSOR_ID_STEPCOUNTER,
    SFW_SENSOR_ID_TAP,
    SFW_SENSOR_ID_TEMPERATURE,
    SFW_SENSOR_ID_COUNT,
    SFW_SENSOR_ID_FIRST = SFW_SENSOR_ID_PROXIMITY,
    SFW_SENSOR_ID_LAST  = SFW_SENSOR_ID_TEMPERATURE,
} SfwSensorId;

/** Orientation sensor states
 *
 * These must match with what sensorfw uses internally
 */
typedef enum SfwOrientationState
{
    SFW_ORIENTATION_UNDEFINED   = 0,  /**< Orientation is unknown. */
    SFW_ORIENTATION_LEFT_UP     = 1,  /**< Device left side is up */
    SFW_ORIENTATION_RIGHT_UP    = 2,  /**< Device right side is up */
    SFW_ORIENTATION_BOTTOM_UP   = 3,  /**< Device bottom is up */
    SFW_ORIENTATION_BOTTOM_DOWN = 4,  /**< Device bottom is down */
    SFW_ORIENTATION_FACE_DOWN   = 5,  /**< Device face is down */
    SFW_ORIENTATION_FACE_UP     = 6,  /**< Device face is up */
} SfwOrientationState;

/** Lid sensor types
 *
 * These must match with what sensorfw uses internally
 */
typedef enum SfwLidType
{
    SFW_LID_TYPE_UNKNOWN = -1, // UnknownLid
    SFW_LID_TYPE_FRONT   =  0, // FrontLid
    SFW_LID_TYPE_BACK    =  1, // BackLid
} SfwLidType;

/** Tap sensor directions
 *
 * These must match with what sensorfw uses internally
 */
typedef enum SfwTapDirection
{
    /** Left or right side tapped */
    SFW_TAP_DIRECTION_X          = 0,

    /** Top or down side tapped */
    SFW_TAP_DIRECTION_Y          = 1,

    /** Face or bottom tapped */
    SFW_TAP_DIRECTION_Z          = 2,

    /** Tapped from left to right */
    SFW_TAP_DIRECTION_LEFT_RIGHT = 3,

    /** Tapped from right to left */
    SFW_TAP_DIRECTION_RIGHT_LEFT = 4,

    /** Tapped from top to bottom */
    SFW_TAP_DIRECTION_TOP_BOTTOM = 5,

    /** Tapped from bottom to top */
    SFW_TAP_DIRECTION_BOTTOM_TOP = 6,

    /** Tapped from face to back */
    SFW_TAP_DIRECTION_FACE_BACK  = 7,

    /** Tapped from back to face */
    SFW_TAP_DIRECTION_BACK_FACE  = 8,
} SfwTapDirection;

/** Tap sensor tap types
 *
 * These must match with what sensorfw uses internally
 */
typedef enum SfwTapType
{
    /** placeholder */
    SFW_TAP_TYPE_NONE       = -1,
    /** Double tap. */
    SFW_TAP_TYPE_DOUBLE_TAP = 0,
    /**< Single tap. */
    SFW_TAP_TYPE_SINGLE_TAP = 1,
} SfwTapType;

/** Common XYZ data block used by sensord for several sensors
 */
struct SfwSampleXyz
{
    /** Microseconds, monotonic */
    uint64_t timestamp;

    float x,y,z;
};

/** ALS data block as sensord sends them
 */
struct SfwSampleAls
{
    /** Microseconds, monotonic */
    uint64_t timestamp;

    /** amount of light [lux] */
    uint32_t value;
};

/** Proximity sensor data block as sensord sends them
 */
struct SfwSampleProximity
{
    /** Microseconds, monotonic */
    uint64_t timestamp;

    /** distance of blocking object [cm] */
    uint32_t distance;

    /** sensor covered [bool]
     *
     * This should be the size of a C++ bool on the same platform.
     * Unfortunately there's no way to find out in a C program
     */
    uint8_t  proximity;
};

/** Orientation sensor data block as sensord sends them
 */
struct SfwSampleOrientation
{
    /* microseconds, monotonic */
    uint64_t timestamp;

    /* orientation [enum SfwOrientationState] */
    int32_t  state;
};

/** Compass sensor data block as sensord sends them
 */
struct SfwSampleCompass
{
    /** Microseconds, monotonic */
    uint64_t timestamp;

    /** Angle to north which may be declination corrected or not. This is the value apps should use */
    int32_t degrees;
    /** Angle to north without declination correction */
    int32_t raw_degrees;
    /** Declination corrected angle to north */
    int32_t corrected_degrees;
    /** Magnetometer calibration level. Higher value means better calibration. */
    int32_t level;
};

/** Lid sensor data block as sensord sends them
 */
struct SfwSampleLid
{
    /** Microseconds, monotonic */
    uint64_t timestamp;

    /** FIXME not documented in sensorfwd */
    int32_t  type; // SfwLidType
    uint32_t value;
};

/** Humidity sensor data block as sensord sends them
 */
struct SfwSampleHumidity
{
    /** Microseconds, monotonic */
    uint64_t timestamp;

    /** FIXME not documented in sensorfwd */
    uint32_t value;
};

/** Magnetometer sensor data block as sensord sends them
 */
struct SfwSampleMagnetometer
{
    /** Microseconds, monotonic */
    uint64_t timestamp;

    /** X coordinate value */
    int32_t x;

    /** Y coordinate value */
    int32_t y;

    /** Z coordinate value */
    int32_t z;

    /** Raw X coordinate value */
    int32_t rx;

    /** Raw Y coordinate value */
    int32_t ry;

    /** Raw Z coordinate value */
    int32_t rz;

    /** Magnetometer calibration level. Higher value means better calibration. */
    int32_t level;
};

/** Pressure sensor data block as sensord sends them
 */
struct SfwSamplePressure
{
    /** Microseconds, monotonic */
    uint64_t timestamp;

    /** FIXME not documented in sensorfwd */
    uint32_t value;
};

/** Step count sensor data block as sensord sends them
 */
struct SfwSampleStepcounter
{
    /** Microseconds, monotonic */
    uint64_t timestamp;

    /** FIXME not documented in sensorfwd */
    uint32_t value;
};

/** Tap sensor data block as sensord sends them
 */
struct SfwSampleTap
{
    /** Microseconds, monotonic */
    uint64_t timestamp;

    /** FIXME not documented in sensorfwd */
    uint32_t direction; // SfwTapDirection
    int32_t  type;      // SfwTapType
};

/** Temperature sensor data block as sensord sends them
 */
struct SfwSampleTemperature
{
    /** Microseconds, monotonic */
    uint64_t temperature_timestamp;

    /** FIXME not documented in sensorfwd */
    uint32_t temperature_value;
};

union SfwSample
{
    /** Microseconds, monotonic */
    uint64_t               timestamp;

    SfwSampleXyz           xyz;
    SfwSampleAls           als;
    SfwSampleProximity     proximity;
    SfwSampleOrientation   orientation;
    SfwSampleAccelerometer accelerometer;
    SfwSampleCompass       compass;
    SfwSampleGyroscope     gyroscope;
    SfwSampleLid           lid;
    SfwSampleHumidity      humidity;
    SfwSampleMagnetometer  magnetometer;
    SfwSamplePressure      pressure;
    SfwSampleRotation      rotation;
    SfwSampleStepcounter   stepcounter;
    SfwSampleTap           tap;
    SfwSampleTemperature   temperature;
};

struct SfwReading
{
    /** Sensor type identification */
    SfwSensorId sensor_id;

    /* Union covering all sensor types */
    SfwSample sample;
};

/* ========================================================================= *
 * Prototypes
 * ========================================================================= */

/* ------------------------------------------------------------------------- *
 * SFWSENSORID
 * ------------------------------------------------------------------------- */

bool        sfwsensorid_is_valid   (SfwSensorId id);
const char *sfwsensorid_name       (SfwSensorId id);
size_t      sfwsensorid_sample_size(SfwSensorId id);
const char *sfwsensorid_interface  (SfwSensorId id);
const char *sfwsensorid_object     (SfwSensorId id);

/* ------------------------------------------------------------------------- *
 * SFWREADING
 * ------------------------------------------------------------------------- */

SfwSensorId                   sfwreading_sensor_id    (const SfwReading *self);
const char                   *sfwreading_repr         (const SfwReading *self);
void                          sfwreading_normalize    (SfwReading *self);
const SfwSampleXyz           *sfwreading_xyz          (const SfwReading *self);
const SfwSampleAls           *sfwreading_als          (const SfwReading *self);
const SfwSampleProximity     *sfwreading_proximity    (const SfwReading *self);
const SfwSampleOrientation   *sfwreading_orientation  (const SfwReading *self);
const SfwSampleAccelerometer *sfwreading_accelerometer(const SfwReading *self);
const SfwSampleCompass       *sfwreading_compass      (const SfwReading *self);
const SfwSampleGyroscope     *sfwreading_gyroscope    (const SfwReading *self);
const SfwSampleLid           *sfwreading_lid          (const SfwReading *self);
const SfwSampleHumidity      *sfwreading_humidity     (const SfwReading *self);
const SfwSampleMagnetometer  *sfwreading_magnetometer (const SfwReading *self);
const SfwSamplePressure      *sfwreading_pressure     (const SfwReading *self);
const SfwSampleRotation      *sfwreading_rotation     (const SfwReading *self);
const SfwSampleStepcounter   *sfwreading_stepcounter  (const SfwReading *self);
const SfwSampleTap           *sfwreading_tap          (const SfwReading *self);
const SfwSampleTemperature   *sfwreading_temperature  (const SfwReading *self);

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

# pragma GCC visibility pop

G_END_DECLS

#endif /* SFWTYPES_H_ */
