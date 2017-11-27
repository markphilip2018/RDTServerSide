#ifndef PACKET_STRUCT_H_INCLUDED
#define PACKET_STRUCT_H_INCLUDED


struct packet
{
    /* Header */
    uint16_t cksum; /* Optional bonus part */
    uint16_t len;
    uint32_t seqno;
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
