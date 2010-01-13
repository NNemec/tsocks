/* tsocks.h - Structures used by tsocks to form SOCKS requests */

#ifndef _TSOCKS_H

#define _TSOCKS_H	1

/* Structure representing a socks connection request */
struct sockreq {
        int8_t version;
        int8_t command;
        int16_t dstport;
        int32_t dstip;
        /* A null terminated username goes here */
};

/* Structure representing a socks connection request response */
struct sockrep {
        int8_t version;
        int8_t result;
        int16_t ignore1;
        int32_t ignore2;
};

#endif
