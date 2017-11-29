#ifndef PACKET_STRUCT_H_INCLUDED
#define PACKET_STRUCT_H_INCLUDED

#include <time.h>


struct packet
{
    /* Header */
    uint16_t cksum; /* Optional bonus part */
    uint16_t len;
    uint32_t seqno;
    time_t timer;
    /* Data */
    char data[500]; /* Not always 500 bytes, can be less */
};

struct ack_packet
{
    uint16_t cksum; /* Optional bonus part */
    uint16_t len;
    uint32_t ackno;
};


#endif // PACKET_STRUCT_H_INCLUDED
