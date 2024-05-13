#include <stdio.h>
#include <inttypes.h>
#include <sfwsensor.h>

static void reading_cb(SfwSensor *sensor, gpointer user_data)
{
    SfwReading *reading = sfwsensor_reading(sensor);
    const SfwSampleProximity *sample = sfwreading_proximity(reading);
    printf("%s: time=%"PRIu64" distance=%"PRIu32" proximity=%s\n",
           sfwsensor_name(sensor),
           sample->timestamp,
           sample->distance,
           sample->proximity ? "true" : "false");
}

int main(int argc, char **argv)
{
    printf("Initialize\n");
    SfwSensor *sensor = sfwsensor_new(SFW_SENSOR_ID_PROXIMITY);
    gulong reading_id = sfwsensor_add_reading_changed_handler(sensor, reading_cb, NULL);
    sfwsensor_start(sensor);

    printf("Mainloop\n");
    GMainLoop *mainloop = g_main_loop_new(NULL, FALSE);
    g_main_loop_run(mainloop);
    g_main_loop_unref(mainloop);

    printf("Cleanup\n");
    sfwsensor_remove_handler(sensor, reading_id);
    sfwsensor_unref(sensor);
    return 0;
}
