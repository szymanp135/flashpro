#ifndef STATE_H
#define STATE_H

#include <stdint.h>

/* Enum describing all program states */
enum state_program {
    /* Initial states */
    s_init                  = 0x00,
    s_data_frame            = 0x10,

    /* Chip-erase command */
    s_erase_chip            = 0x20,

    /* Sector-erase command */
    s_erase_sector          = 0x30,
    s_erase_sector_address  = 0x31,

    /* Write sector command */
    s_write_sector          = 0x40,
    s_write_sector_address  = 0x41,
    s_write_sector_data     = 0x42,
    s_write_sector_readout  = 0x43,

    /* Read sector command */
    s_read_sector           = 0x50,
    s_read_sector_address   = 0x51,

    /* Set endianness command */
    s_set_endianness        = 0x80,
    s_set_endianness_type   = 0x81,
    
    /* Syntax error state */
    s_error                 = 0xff
};

/* Function declarations */
int is_whitespace(char);
int char_to_digit(int);
int parse_int(int (*)(void));
int parse_data(uint8_t*);
void state_erase_sector(char, enum state_program*);
void state_write_sector(char, enum state_program*);
void state_read_sector(char, enum state_program*);
void state_set_endianness(char, enum state_program*);
void state_handle_char(char, enum state_program*);

#endif
