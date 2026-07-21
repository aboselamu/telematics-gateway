#ifndef GPS_DECODER_H
#define GPS_DECODER_H

#include <stdbool.h>

typedef struct
{
    char utc_time[16];

    bool valid_fix;

    char latitude[16];
    char latitude_dir;

    char longitude[16];
    char longitude_dir;

    char speed[16];

} gps_data_t;

/* Decode a GPRMC sentence */
bool gps_decode_rmc(const char *frame, gps_data_t *gps);

#endif