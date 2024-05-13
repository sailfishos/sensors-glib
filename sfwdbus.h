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

#ifndef SFWDBUS_H_
# define SFWDBUS_H_

/* ========================================================================= *
 * Sensorfwd D-Bus Service
 * ========================================================================= */

/** D-Bus name of the sensord service */
# define SFWDBUS_SERVICE                          "com.nokia.SensorService"

/* ========================================================================= *
 * Sensorfwd Sensor Manager Interface
 * ========================================================================= */

/** D-Bus object path for sensord sensor manager
 *
 * Note: Manager object is always awailable
 */
# define SFWDBUS_MANAGER_OBJECT                   "/SensorManager"

/** D-Bus interface used by sensor manager */
# define SFWDBUS_MANAGER_INTEFCACE                "local.SensorManager"

/** D-Bus method for loading sensor plugin */
# define SFWDBUS_MANAGER_METHOD_LOAD_PLUGIN       "loadPlugin"

/** D-Bus method for starting sensor session */
# define SFWDBUS_MANAGER_METHOD_START_SESSION     "requestSensor"

/** D-Bus method for stopping sensor session */
# define SFWDBUS_MANAGER_METHOD_STOP_SESSION      "releaseSensor"

/** D-Bus method for querying available sensors */
# define SFWDBUS_MANAGER_METHOD_AVAILABLE_PLUGINS "availableSensorPlugins"

/* ========================================================================= *
 * Sensorfwd Sensor Interface
 * ========================================================================= */

/* Sensor / plugin names used by sensorfwd
 */
# define SFWDBUS_SENSOR_NAME_PROXIMITY           "proximitysensor"
# define SFWDBUS_SENSOR_NAME_ALS                 "alssensor"
# define SFWDBUS_SENSOR_NAME_ORIENTATION         "orientationsensor"
# define SFWDBUS_SENSOR_NAME_ACCELEROMETER       "accelerometersensor"
# define SFWDBUS_SENSOR_NAME_COMPASS             "compasssensor"
# define SFWDBUS_SENSOR_NAME_GYROSCOPE           "gyroscopesensor"
# define SFWDBUS_SENSOR_NAME_LID                 "lidsensor"
# define SFWDBUS_SENSOR_NAME_HUMIDITY            "humiditysensor"
# define SFWDBUS_SENSOR_NAME_MAGNETOMETER        "magnetometersensor"
# define SFWDBUS_SENSOR_NAME_PRESSURE            "pressuresensor"
# define SFWDBUS_SENSOR_NAME_ROTATION            "rotationsensor"
# define SFWDBUS_SENSOR_NAME_STEPCOUNTER         "stepcountersensor"
# define SFWDBUS_SENSOR_NAME_TAP                 "tapsensor"
# define SFWDBUS_SENSOR_NAME_TEMPERATURE         "temperaturesensor"

/* Sensor D-Bus object paths used by sensorfwd
 *
 * Object paths are derived from sensor/plugin names.
 *
 * Objects are made available on demand i.e. after (1st) client calls
 * 1) SFWDBUS_MANAGER_METHOD_LOAD_PLUGIN and
 * 2) SFWDBUS_MANAGER_METHOD_START_SESSION
 */
# define SFWDBUS_SENSOR_OBJECT(name)             SFWDBUS_MANAGER_OBJECT "/" name

# define SFWDBUS_SENSOR_OBJECT_PROXIMITY         SFWDBUS_SENSOR_OBJECT(SFWDBUS_SENSOR_NAME_PROXIMITY)
# define SFWDBUS_SENSOR_OBJECT_ALS               SFWDBUS_SENSOR_OBJECT(SFWDBUS_SENSOR_NAME_ALS)
# define SFWDBUS_SENSOR_OBJECT_ORIENTATION       SFWDBUS_SENSOR_OBJECT(SFWDBUS_SENSOR_NAME_ORIENTATION)
# define SFWDBUS_SENSOR_OBJECT_ACCELEROMETER     SFWDBUS_SENSOR_OBJECT(SFWDBUS_SENSOR_NAME_ACCELEROMETER)
# define SFWDBUS_SENSOR_OBJECT_COMPASS           SFWDBUS_SENSOR_OBJECT(SFWDBUS_SENSOR_NAME_COMPASS)
# define SFWDBUS_SENSOR_OBJECT_GYROSCOPE         SFWDBUS_SENSOR_OBJECT(SFWDBUS_SENSOR_NAME_GYROSCOPE)
# define SFWDBUS_SENSOR_OBJECT_LID               SFWDBUS_SENSOR_OBJECT(SFWDBUS_SENSOR_NAME_LID)
# define SFWDBUS_SENSOR_OBJECT_HUMIDITY          SFWDBUS_SENSOR_OBJECT(SFWDBUS_SENSOR_NAME_HUMIDITY)
# define SFWDBUS_SENSOR_OBJECT_MAGNETOMETER      SFWDBUS_SENSOR_OBJECT(SFWDBUS_SENSOR_NAME_MAGNETOMETER)
# define SFWDBUS_SENSOR_OBJECT_PRESSURE          SFWDBUS_SENSOR_OBJECT(SFWDBUS_SENSOR_NAME_PRESSURE)
# define SFWDBUS_SENSOR_OBJECT_ROTATION          SFWDBUS_SENSOR_OBJECT(SFWDBUS_SENSOR_NAME_ROTATION)
# define SFWDBUS_SENSOR_OBJECT_STEPCOUNTER       SFWDBUS_SENSOR_OBJECT(SFWDBUS_SENSOR_NAME_STEPCOUNTER)
# define SFWDBUS_SENSOR_OBJECT_TAP               SFWDBUS_SENSOR_OBJECT(SFWDBUS_SENSOR_NAME_TAP)
# define SFWDBUS_SENSOR_OBJECT_TEMPERATURE       SFWDBUS_SENSOR_OBJECT(SFWDBUS_SENSOR_NAME_TEMPERATURE)

/* D-Bus interfaces for accessing sensor specific data
 *
 * Sensorfwd uses "copy-paste" interfaces for sensors. The only
 * difference between these interfaces is the name of "get current
 * value" method call and the type of data it returns.
 */
# define SFWDBUS_SENSOR_INTERFACE_PROXIMITY      "local.ProximitySensor"
# define SFWDBUS_SENSOR_INTERFACE_ALS            "local.ALSSensor"
# define SFWDBUS_SENSOR_INTERFACE_ORIENTATION    "local.OrientationSensor"
# define SFWDBUS_SENSOR_INTERFACE_ACCELEROMETER  "local.AccelerometerSensor"
# define SFWDBUS_SENSOR_INTERFACE_COMPASS        "local.CompassSensor"
# define SFWDBUS_SENSOR_INTERFACE_GYROSCOPE      "local.GyroscopeSensor"
# define SFWDBUS_SENSOR_INTERFACE_LID            "local.LidSensor"
# define SFWDBUS_SENSOR_INTERFACE_HUMIDITY       "local.HumiditySensor"
# define SFWDBUS_SENSOR_INTERFACE_MAGNETOMETER   "local.MagnetometerSensor"
# define SFWDBUS_SENSOR_INTERFACE_PRESSURE       "local.PressureSensor"
# define SFWDBUS_SENSOR_INTERFACE_ROTATION       "local.RotationSensor"
# define SFWDBUS_SENSOR_INTERFACE_STEPCOUNTER    "local.StepcounterSensor"
# define SFWDBUS_SENSOR_INTERFACE_TAP            "local.TapSensor"
# define SFWDBUS_SENSOR_INTERFACE_TEMPERATURE    "local.TemperatureSensor"

/** D-Bus method for enabling sensor
 *
 * @Note Common to all sensors.
 */
# define SFWDBUS_SENSOR_METHOD_START             "start"

/** D-Bus method for disabling sensor
 *
 * @Note Common to all sensors.
 */
# define SFWDBUS_SENSOR_METHOD_STOP              "stop"

/** D-Bus method for changing sensor standby override
 *
 * @Note All sensor specific interfaces do have have this method,
 *       but unless it is actually explicitly implemented, a virtual
 *       dummy method call handler gets invoked and it will return
 *       "false" -> caveat client side code.
 */
# define SFWDBUS_SENSOR_METHOD_SET_OVERRIDE      "setStandbyOverride"

/** D-Bus method for changing sensor data rate
 *
 * @Note Common to all sensors.
 */
# define SFWDBUS_SENSOR_METHOD_SET_DATARATE      "setDataRate"

/* D-Bus methods for getting current sensor value
 *
 * The method name and returned data varies from one sensor to
 * another, and in some cases there is no "current state" to
 * query (such as tap detection sensor).
 */
# define SFWDBUS_SENSOR_METHOD_GET_PROXIMITY     "proximity"
# define SFWDBUS_SENSOR_METHOD_GET_ALS           "lux"
# define SFWDBUS_SENSOR_METHOD_GET_ORIENTATION   "orientation"
# define SFWDBUS_SENSOR_METHOD_GET_ACCELEROMETER "xyz"
# define SFWDBUS_SENSOR_METHOD_GET_COMPASS       "value" /* or "declinationvalue" */
# define SFWDBUS_SENSOR_METHOD_GET_GYROSCOPE     "value"
# define SFWDBUS_SENSOR_METHOD_GET_LID           "closed"
# define SFWDBUS_SENSOR_METHOD_GET_HUMIDITY      "relativeHumidity"
# define SFWDBUS_SENSOR_METHOD_GET_MAGNETOMETER  "magneticField"
# define SFWDBUS_SENSOR_METHOD_GET_PRESSURE      "pressure"
# define SFWDBUS_SENSOR_METHOD_GET_ROTATION      "rotation"
# define SFWDBUS_SENSOR_METHOD_GET_STEPCOUNTER   "steps"
# define SFWDBUS_SENSOR_METHOD_GET_TAP           NULL /* has no state. just events */
# define SFWDBUS_SENSOR_METHOD_GET_TEMPERATURE   "temperature"

#endif /* SFWDBUS_H_ */
