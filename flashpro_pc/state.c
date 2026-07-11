#include "state.h"

#define NIBBLE_SIZE 4

#define OK_RESPONSE_SYMBOL          'k'
#define DATA_RESPONSE_SYMBOL        'd'

#define RESPONSE_ARG_ADDRESS_SYMBOL 'a'
#define RESPONSE_ARG_DATA_SYMBOL    'd'

#define INT_END_SYMBOL              '$'
#define BUFFER_START_SYMBOL         '['
#define BUFFER_END_SYMBOL           ']'

/* Check if c is a whitespace
 *
 * Returns 1 if c is a whitespace, 0 otherwise.
 *
 * Arguments:
 * c        : character to be checked
 */
int is_whitespace(char c) {
	return c == ' ' || c == '\t' || c == '\n' || c == '\r';
}

/* Convert character to hexadecimal digit value.
 *
 * Returns digit value (0-15) if succeeded and -1 otherwise
 *
 * Arguments:
 * c        : character with digit to be converted
 */
int char_to_digit(int c) {

    if (c >= '0' && c <= '9')
        return c - '0';
    if (c >= 'a' && c <= 'f')
        return c - 'a' + 10;
    if (c >= 'A' && c <= 'F')
        return c - 'A' + 10;

    return -1;
}

/* Parse text and read integer value in hexadecimal.
 * Reads characters, skips any whitespaces and constructs an integer
 * number until INT_END_SYMBOL sign is read.
 *
 * Returns parsed integer if succeeded and -1 otherwise.
 * Program does not predict usage of negative integers, thus -1 indicates
 * error.
 *
 * Arguments:
 * text     : text to parse integer from
 */
int parse_int(char **text) {

    int c;
    int value = 0;
    int digit;

    do {
        /* Read character */
        c = *(++(*text));
        /* Skip white spaces */
        if (is_whitespace(c))
            continue;

        /* Convert character to digit */
        digit = char_to_digit(c);
    
        /* Digit is invalid */
        if (digit < 0) {
            /* If it is symbol indicating end of integer then return */
            if (c == INT_END_SYMBOL)
                return value;
            /* Otherwise return rror */
            else
                return -1; 
        }   
        /* Digit is valid - update value */
        else
            value = (value << 4) | digit;

    } while (c != INT_END_SYMBOL);

	return value;
}

/* Parse text and read buffer byte data encoded in hex.
 * Reads BUFFER_START_SYMBOL, then reads characters until
 * BUFFER_END_SYMBOL sign is read.
 *
 * Returns 0 if succeeded and error value otherwise:
 * 1 if buffer start is invalid
 * 2 if invalid digit was read
 *
 * Arguments:
 * text     : text to parse array from
 * data     : pointer to buffer to where to store parsed data
 */
int parse_buffer(char **text, uint8_t *data) {

	int c = 0;
	uint8_t byte_value = 0;
	int digit_num = NIBBLE_SIZE;
	int digit_value;

	/* Skip preceding white spaces and read BUFFER_START_SYMBOL */
	while (02137) {
		/* Read character */
        c = *(++(*text));
		/* Skip whitespaces */
		if (is_whitespace(c))
			continue;
		/* Continue function if start symbol was read */
		if (c == BUFFER_START_SYMBOL)
			break;
		/* Other characters are not welcome */
		return 1;
	}

	/* Read data, convert to buffer byte data and write to buffer */
	while (0x2137) {
		/* Read character */
        c = *(++(*text));
		/* Check if character is not a buffer end symbol */
		if (c == BUFFER_END_SYMBOL)
			break;
		/* Convert character to integer value */
		digit_value = char_to_digit(c);
		/* Check if it was a valid digit */
		if (digit_value < 0)
			return 2;
		/* Update byte value with read digit */
		byte_value |= digit_value << digit_num;

		/* If digit_num is 0 then whole byte has been acquired
		 * and is read to be written to buffer
		 */
		if (!digit_num) {
			/* Write byte to buffer */
			*(data++) = byte_value;
			/* Reset byte value */
			byte_value = 0;
		}

		/* Change digit number */
		digit_num = NIBBLE_SIZE - digit_num;
	}

	return 0;
}

/* Main function for character handling in context of state change.
 * Depending on current program state and given character decides what
 * to do with program's state
 *
 * Returns nothing
 *
 * Arguments:
 * c        : character upon which something about the state should be
 *            done
 * state    : pointer to program's state
 */
void state_handle_char(char c, enum state_program *state) {

	if(is_whitespace(c))
		return;

	switch (*state & 0xf0) {
		/* Init state */
		case s_init: *state = c == '{' ? s_data_frame : s_init; break;
		/* Data frame state */
		case s_data_frame:
			switch (c) {
				case OK_RESPONSE_SYMBOL:
					*state = s_ok; break;
				case DATA_RESPONSE_SYMBOL:
					*state = s_data; break;
				case '}':
					*state = s_init; break;
				default: *state = s_error;
			}
			break;
		/* Ok response state */
		case s_ok:
			if (c == ';') { *state = s_data_frame; break; }
			*state = s_error;
			break;
		/* Data response state */
		case s_data:
			switch(*state) {
				/* Change state to arguments' or leave data state */
				case s_data:
					switch(c) {
						case RESPONSE_ARG_ADDRESS_SYMBOL:
							*state = s_data_address;
							break;
						case RESPONSE_ARG_DATA_SYMBOL:
							*state = s_data_data;
							break;
						case ';':
							*state = s_data_frame;
							break;
						default:
							*state = s_error;
					}
					break;
				/* Argument states */
				case s_data_data:
				case s_data_address:
					if (c == ';') { *state = s_data; break; }
					*state = s_error;
					break;
				/* Other states are forbidden */
				default:
					*state = s_error;
			}
			break;

		/* There are no other state groups to consider */
		default:
			*state = s_error;
	}
}



