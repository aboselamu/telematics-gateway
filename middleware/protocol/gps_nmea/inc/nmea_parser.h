#ifndef NMEA_PARSER_H
#define NMEA_PARSER_H

typedef enum
{
    NMEA_UNKNOWN = 0,
    NMEA_GPRMC,
    NMEA_GPGGA,
    NMEA_GPGSV,
    NMEA_GPGSA,
    NMEA_GPVTG,
    NMEA_GPGLL

} nmea_sentence_t;

nmea_sentence_t nmea_parse(const char *frame);

#endif