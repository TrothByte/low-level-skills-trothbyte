#ifndef BAD_ENUM_H
#define BAD_ENUM_H

/* C believes the enum is int-sized with exactly these values. */
enum status {
    STATUS_OK = 0,
    STATUS_BUSY = 1,
    STATUS_ERROR = 2,
};

#endif
