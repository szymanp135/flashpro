#ifndef UTIL_H
#define UTIL_H

#include <stdint.h>
#include <unistd.h>
#include <time.h>
#include <errno.h>
#include "constants.h"
#include "error.h"
#include "string.h"

#define FIRST_READ_RETRY_COUNT 30000
#define FOLLOWING_READ_RETRY_COUNT 100

int sleep_ns(long int);

int str_to_readout(char *);
int str_to_endianness(char *);

int read_data(int, uint8_t *, int);

int send_message(struct string *, int);
int receive_message(struct string *, int);

#endif
