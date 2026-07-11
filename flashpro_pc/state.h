#ifndef STATE_H
#define STATE_H

#include <stdint.h>

/* Enum describing all program states */
enum state_program {
    /* Initial states */
    s_init          = 0x00,
    s_data_frame    = 0x10,

    /* Ok info */
    s_ok            = 0x20,

    /* Data array */
    s_data          = 0x30,
	s_data_address  = 0x31,
    s_data_data     = 0x32,

    /* Syntax error state */
    s_error         = 0xff
};

/* Function declarations */
int is_whitespace(char);
int char_to_digit(int);
int parse_int(char **);
int parse_buffer(char **, uint8_t *);
void state_data(char c, enum state_program *);
void state_handle_char(char, enum state_program*);

#endif
