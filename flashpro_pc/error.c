#include "error.h"

/* Print error information with usage and exit program
 * This function defines its own error code scheme which is used by other
 * functions throughout the file
 *
 * Returns nothing
 *
 * Arguments:
 * code     : error code number
 * name     : program name string
 */
void error(int code, char* name, void (*usage)(char *)) {
	
	switch (code) {
		case ERROR_ALLOC:
			fprintf(stderr, "Couldn't allocate new memory.\n");
			break;
		case ERROR_ENDIANNESS_STR:
			fprintf(stderr, "Endianness string is invalid.\n");
			break;
		case ERROR_NO_ENDIANNESS_STR:
			fprintf(stderr, "No endianness provided.\n");
			break;
		case ERROR_READOUT_STR:
			fprintf(stderr, "Readout string is invalid.\n");
			break;
		case ERROR_NO_READOUT_STR:
			fprintf(stderr, "No readout provided.\n");
			break;
		case ERROR_UNKNOWN_COMMAND:
			fprintf(stderr, "Unknown command.\n");
			break;
		case ERROR_FILE_OPEN:
			fprintf(stderr, "Couldn't open file.\n");
			break;
		case ERROR_NO_FILE:
			fprintf(stderr, "No file provided.\n");
			break;
		case ERROR_FILE_WRITE:
			fprintf(stderr, "Couldn't write to file.\n");
			break;
		case ERROR_FILE_READ:
			fprintf(stderr, "Couldn't read from file.\n");
			break;
		case ERROR_DEVICE_OPEN:
			fprintf(stderr, "Couldn't open device.\n");
			break;
		case ERROR_FORK:
			fprintf(stderr, "Couldn't fork.\n");
			break;
		default:
			fprintf(stderr, "Error of unknown code: %d\n", code);
	}

	usage(name);
	exit(code);
}

