#ifndef ERROR_H
#define ERROR_H

#include <stdio.h>
#include <stdlib.h>

#define ERROR_ALLOC               -1
#define ERROR_ENDIANNESS_STR     -10
#define ERROR_NO_ENDIANNESS_STR  -11
#define ERROR_READOUT_STR        -20
#define ERROR_NO_READOUT_STR     -21
#define ERROR_UNKNOWN_COMMAND    -30
#define ERROR_FILE_OPEN         -100
#define ERROR_NO_FILE           -101
#define ERROR_FILE_WRITE        -102
#define ERROR_FILE_READ         -103
#define ERROR_DEVICE_OPEN       -200
#define ERROR_FORK              -300

/* Error function */
void error(int, char *, void (*)(char *));

#endif
