struct socksent {
        struct in_addr localip;
        struct in_addr localnet;
	struct socksent *next;
};

struct sockreq {
        int8_t version;
        int8_t command;
        int16_t dstport;
        int32_t dstip;
        /* A null terminated username goes here */
};

struct sockrep {
        int8_t version;
        int8_t result;
        int16_t ignore1;
        int32_t ignore2;
};
