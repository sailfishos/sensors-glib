#include <stdio.h>
#include <sfwsensor.h>

static void reading_cb(SfwSensor *sensor, gpointer user_data)
{
    SfwReading *reading = sfwsensor_reading(sensor);
    printf("%s\n", sfwreading_repr(reading));
}

int main(int argc, char **argv)
{
    auto bool selected_p(SfwSensorId id)
    {
        if( argc < 2 )
            return true;
        for( int i = 1; i < argc; ++i )
            if( g_str_has_prefix(sfwsensorid_name(id), argv[i]) )
                return true;
        return false;
    }

    SfwSensor *sensor[SFW_SENSOR_ID_COUNT] = {};
    gulong reading_id[SFW_SENSOR_ID_COUNT] = {};

    printf("Initialize\n");
    for( SfwSensorId id = SFW_SENSOR_ID_FIRST; id < SFW_SENSOR_ID_LAST; ++id ) {
        bool selected = selected_p(id);
        printf("%s %s\n", selected ? "starting" : "ignoring", sfwsensorid_name(id));
        if( !selected )
            continue;
        sensor[id] = sfwsensor_new(id);
        reading_id[id] = sfwsensor_add_reading_changed_handler(sensor[id], reading_cb, NULL);
        sfwsensor_set_datarate(sensor[id], 5);
        sfwsensor_start(sensor[id]);
    }

    printf("Mainloop\n");
    GMainLoop *mainloop = g_main_loop_new(NULL, FALSE);
    g_main_loop_run(mainloop);
    g_main_loop_unref(mainloop);

    printf("Cleanup\n");
    for( SfwSensorId id = SFW_SENSOR_ID_FIRST; id < SFW_SENSOR_ID_LAST; ++id ) {
        sfwsensor_remove_handler(sensor[id], reading_id[id]);
        sfwsensor_unref(sensor[id]);
    }
    return 0;
}
