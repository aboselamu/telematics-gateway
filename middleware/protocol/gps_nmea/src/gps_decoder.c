#include "gps_decoder.h"
#include <string.h>
#include <stdint.h>  

/*
 * Decode:
 * $GPRMC,time,status,lat,N/S,lon,E/W,speed,...
 */

bool gps_decode_rmc(const char *frame, gps_data_t *gps)
{
    if ((frame == NULL) || (gps == NULL))
    {
        return false;
    }

    uint8_t field = 0;
    uint8_t index = 0;

    char token[20];

    memset(gps, 0, sizeof(gps_data_t));

    while (*frame)
    {
        if ((*frame == ',') || (*frame == '\0'))
        {
            token[index] = '\0';

            switch (field)
            {
                case 1:
                    strcpy(gps->utc_time, token);
                    break;

                case 2:
                    gps->valid_fix = (token[0] == 'A');
                    break;

                case 3:
                    strcpy(gps->latitude, token);
                    break;

                case 4:
                    gps->latitude_dir = token[0];
                    break;

                case 5:
                    strcpy(gps->longitude, token);
                    break;

                case 6:
                    gps->longitude_dir = token[0];
                    break;

                case 7:
                    strcpy(gps->speed, token);
                    break;

                default:
                    break;
            }

            field++;
            index = 0;

            if (*frame == '\0')
            {
                break;
            }
        }
        else
        {
            if (index < (sizeof(token) - 1))
            {
                token[index++] = *frame;
            }
        }

        frame++;
    }

    return true;
}