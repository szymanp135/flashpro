#ifndef UTIL_H
#define UTIL_H

#include <stdint.h>
#include <unistd.h>
#include <time.h>
#include <errno.h>
#include "constants.h"
#include "error.h"

int sleep_ns(long int);

int str_to_readout(char *);
int str_to_endianness(char *);

int read_data(int, uint8_t *, int);

#endif
