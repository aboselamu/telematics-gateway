#include "nmea_parser.h"
#include <string.h>

nmea_sentence_t nmea_parse(const char *frame)
{
    if (strncmp(frame, "$GPRMC", 6) == 0)
        return NMEA_GPRMC;

    if (strncmp(frame, "$GPGGA", 6) == 0)
        return NMEA_GPGGA;

    if (strncmp(frame, "$GPGSV", 6) == 0)
        return NMEA_GPGSV;

    if (strncmp(frame, "$GPGSA", 6) == 0)
        return NMEA_GPGSA;

    if (strncmp(frame, "$GPVTG", 6) == 0)
        return NMEA_GPVTG;

    if (strncmp(frame, "$GPGLL", 6) == 0)
        return NMEA_GPGLL;

    return NMEA_UNKNOWN;
}