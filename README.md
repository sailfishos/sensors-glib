What sensors-glib is
====================

It is a C language client library for communicating with [Sailfish OS
Sensor Framework Daemon](https://github.com/sailfishos/sensorfw).

It is implemented as a collection of glib object classes.

It is (eventually) meant to be feature compatible with
[QtSensors](https://github.com/sailfishos/qtsensors) API.

Objects available in sensors-glib
==================================

SfwSensor
---------

- The class applications need to directly work with
- One object / sensor / specific need, not shared by default
- Objects have methods for starting / stopping sensor, controlling
  sensor datarate, stand-by override, and accessing the latest seen
  sensor value / subscribing to value change notifications

SfwReading
----------

- Handles abstraction of sensor type specific values
- Usually instances are owned by and made available to
  applications via SfwSensor objects

SfwService
----------

- Tracks sensor daemon availability on D-Bus SystemBus
- Shared singleton instance
- Usually applications can ignore this object - unless there is an
  explicit need to react to availability of sensor service

SfwPlugin
---------

- Handles loading of sensor specific plugins at daemon side
- Shared singleton instance / sensor type
- Usually applications can ignore these objects - unless there is an
  explicit need to react to availability of sensor backends

SfwReporting
------------

- Handles sensor property synchronization behind the scene
- Usually applications have no need to touch these objects

Caveats
=======

This is work in progress and the API cannot be considered stable.

Some features that are available with QtSensors have not been
implemented yet.

Also the currently used one glib signal / one sensor reading way of
reporting can cause problems by choking the glib mainloop when high
data acquisition rates (hundreds of samples per second) are used - and
addressing this could lead to problems with backwards compatibility.

Examples
========

* Minimalistic [proximity sensor](examples/proximity.c) example
* Example subscribing to [all sensors](examples/allsensors.c)

-----------------------------------------------------------------------
