/*
 * flashpro - double SST39F020 flash memory programmer
 * Paweł Szymański 06.2026
 *
 * flashpro is a double SST39F020 flash memory programmer; memories are
 * configured to be used in 16-bit systems, meaning both chips would be
 * used at the same time during their use (one for lower 8 bits and other
 * for upper 8 bits).
 *
 * This software is meant to be used with flashpro hardware.
 */

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>

#define GETOPT_STRING "c:d:e:f:ghr:"
#define DEFAULT_DEVICE "/dev/ttyUSB0"
#define STRING_BUFFER_SIZE 4096

#define MEMORY_SIZE 0x080000
#define SECTOR_SIZE 0x002000
#define SECTOR_COUNT MEMORY_SIZE / SECTOR_SIZE

/* Simple dynamic string implementation struct */
struct string {
	/* string's text */
	char* string;
	/* string's text allocated size */
	int size;
};

/* This enum is used to determine what command given structure represents.
 * It is a part of polimorphism simulation.
 */
enum command_type {
	command_type_dummy,
	command_type_erase,
	command_type_read,
	command_type_write,
	command_type_endianness
};

/* Following stucts are definitions of flashpro commands executed on
 * programmer device. They are made simulating polimorphism - abstract
 * type of 'command' is used to determine command type based on 'type'
 * variable before cast to target struct type.
 * 
 * Erase command erases whole memory.
 * Read and write commands are executed on sector and not on whole memory.
 */
struct command_node {
	struct command_node *next_command;
	enum command_type type;
};

struct command_node_erase {
	struct command_node *next_command;
	enum command_type type;
};

struct command_node_read {
	struct command_node *next_command;
	enum command_type type;
	uint32_t sector_address;
};

struct command_node_write {
	struct command_node *next_command;
	enum command_type type;
	uint32_t sector_address;
	int readout;
	uint8_t* sector_data;
};

struct command_node_endianness {
	struct command_node *next_command;
	enum command_type type;
	int endianness;
};

/* Print usage information
 *
 * Returns nothing
 *
 * Arguments:
 * name     : program's name string
 */
void usage(char *name) {
	printf("\nusage: %s <options>\n", name);
	printf(" - c: command to be executed; possible commands are:\n");
	printf("   - e: erase memory\n");
	printf("   - r: read memory\n");
	printf("   - w: write memory\n");
	printf(" - d: programmer device path; default is %s\n",
		DEFAULT_DEVICE);
	printf(" - e: set endianness to big ('b') or little ('l')\n");
	printf(" - f: file path serving as input/output of write/read "
		"commands\n");
	printf(" - h: print help message\n");
	printf(" - r: turn on readout after memory programming ('y') "
		"or turn off ('n')\n");
}

/* Print usage and help information
 *
 * Returns nothing
 *
 * Arguemnts:
 * name     : program's name string
 */
void help(char *name) {
	usage(name);
	printf("\n");
	printf("flashpro is a memory programmer software made to program SST39F020 flash memory units.\n");
	/* TODO: write meaningful help */
}

#define ERROR_ALLOC 1
#define ERROR_ENDIANNESS_STR 10
#define ERROR_NO_ENDIANNESS_STR 11
#define ERROR_READOUT_STR 20
#define ERROR_NO_READOUT_STR 21
#define ERROR_UNKNOWN_COMMAND 30
#define ERROR_FILE_OPEN 100
#define ERROR_NO_FILE 101

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
void error(int code, char* name) {
	
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
		default:
			fprintf(stderr, "Unknown error code: %d\n", code);
	}

	usage(name);
	exit(code);
}

/* Append string with appendage to create an appendaged string
 *
 * Returns 0 if succeeded and error value otherwise.
 *
 * Arguments:
 * string       : pointer to string to be appended to
 * appendage    : string to be appended by
 */
int string_append(struct string *string, char *appendage) {
	
	int string_len;
	int appendage_len;

	/* Compute lengths of strings */
	appendage_len = strlen(appendage);
	if (string->string)
		string_len = strlen(string->string);
	else
		string_len = 0;

	/* If string would not fit concatenated string, expand its size to
	 * make it fit.
	 * Additional 1 in condition equation is for null terminating byte.
	 */
	while (string_len + appendage_len + 1 > string->size) {
		string->size += STRING_BUFFER_SIZE;
		string->string = realloc(string->string, string->size);
		if (!string->string)
			return ERROR_ALLOC;
	}

	/* Append string with appendage */
	strcat(string->string, appendage);

	return 0;
}

/* Convert readout string to readout value
 *
 * Returns readout value if succeeded and negative value otherwise
 *
 * Arguments:
 * readout_str  : string with readout symbol to be converted
 */
int str_to_readout(char *readout_str) {
	if (!readout_str)
		return ERROR_NO_READOUT_STR;
	else if (*readout_str == 'y')
		return 1;
	else if (*readout_str == 'n')
		return 0;
	else
		return ERROR_READOUT_STR;
}

/* Convert endianness string to endianness value
 *
 * Returns endianness value if succeeded and negative value otherwise
 *
 * Arguments:
 * readout_str  : string with endianness symbol to be converted
 */
int str_to_endianness(char *endianness_str) {
	if (!endianness_str)
		return ERROR_NO_ENDIANNESS_STR;
	else if (*endianness_str == 'b')
		return 1;
	else if (*endianness_str == 'l')
		return 0;
	else
		return ERROR_ENDIANNESS_STR;
}

/* Read data from opened file to 'data' buffer of size 'size'
 *
 * Returns 0 if succeeded; otherwise:
 * - positive value indicating read bytes if EOF was encountered
 * - negative for error
 *
 * Arguments:
 * fd       : file descriptor from which data will be read
 * data     : pointer to data buffer
 * size     : size of data to be read from file in bytes
 */
int read_data(int fd, uint8_t *data, int size) {

	int i, res;
	int read_bytes = 0;
	int bytes_to_read = size;

	while (bytes_to_read) {
		/* Read data */
		res = read(fd, data + read_bytes, bytes_to_read);
		/* If error occured other than signal interrupt then return */
		if (res < 0 && errno != EINTR)
			return res;
		/* If EOF encountered then return number of read bytes */
		if (res == 0) {
			/* Fill remaining of data buffer with 0 */
			for (i = read_bytes; i < size; ++i)
				data[i] = 0;
			return read_bytes;
		}
		/* If no error ther update read bytes and bytes to be read */
		else if (res > 0) {
			read_bytes += res;
			bytes_to_read -= res;
		}
	}

	return 0;
}

/* Print nodes int command queue
 *
 * Returns nothing
 *
 * Arguments:
 * command_queue    : command queue to be printed
 */
void print_command_queue(struct command_node *command_queue) {

	while (command_queue) {
		printf("================================\n");
		switch(command_queue->type) {
			case command_type_dummy:
				printf("  node:       dummy\n");
				break;
			case command_type_erase:
				printf("  node:       erase\n");
				break;
			case command_type_read:
				printf("  node:       read\n");
				printf("  address:    0x%08X\n",
					((struct command_node_read*)command_queue)->sector_address);
				break;
			case command_type_write:
				printf("  node:       write\n");
				printf("  address:    0x%08X\n",
					((struct command_node_write*)command_queue)->sector_address);
				printf("  data:       buffer\n");
				printf("  readout:    %d\n",
					((struct command_node_write*)command_queue)->readout);
				break;
			case command_type_endianness:
				printf("  node:       endianness\n");
				printf("  endianness: %d\n",
					((struct command_node_endianness*)command_queue)->endianness);
				break;
		}
		printf("================================\n");
		printf("                ||\n");
		printf("                ||\n");
		printf("                \\/\n");

		command_queue = command_queue->next_command;
	}

	printf("               NULL\n");
}

/* Free allocated memory for command nodes in command queue
 *
 * Returns nothing
 *
 * Arguments:
 * command_queue    : command queue to be freed
 */
void free_command_queue(struct command_node *command_queue) {
	
	struct command_node *next;

	/* Iterate until the end of queue */
	while(command_queue) {
		/* If node is a write command then free its data buffer */
		if (command_queue->type == command_type_write)
			free(
				((struct command_node_write*)command_queue)->sector_data
			);
		/* Save next command node */
		next = command_queue->next_command;
		/* Free current node's memory */
		free(command_queue);
		/* Switch current node to next one */
		command_queue = next;
	}
}

/* Add set endianned command node to command queue
 *
 * Returns 0 if succeeded and error value otherwise.
 * Error value is compliant with error function codes.
 *
 * Arguments:
 * command_queue    : pointer to command queue
 * endianness_str   : string with endianness value
 */
int add_command_endianness(
	struct command_node **command_queue,
	char* endianness_str
	) {
	
	int endianness;
	struct command_node_endianness *node;

	/* Convert endianness string to value */
	if((endianness = str_to_endianness(endianness_str)) < 0)
		return endianness;

	node = malloc(sizeof(struct command_node_endianness));
	if (!node)
		return ERROR_ALLOC;
	
	node->next_command = NULL;
	node->type = command_type_endianness;
	node->endianness = endianness;
	*command_queue = (struct command_node*)node;

	return 0;
}

/* Add erase memory command node to command queue
 *
 * Returns 0 if succeeded and error value otherwise.
 * Error value is compliant with error function codes.
 *
 * Arguments:
 * command_queue    : pointer to command queue
 */
int add_command_erase(struct command_node **command_queue) {
	
	struct command_node_erase *node;

	node = malloc(sizeof(struct command_node_erase));
	if (!node)
		return ERROR_ALLOC;

	node->next_command = NULL;
	node->type = command_type_erase;
	if (*command_queue)
		(*command_queue)->next_command = (struct command_node*)node;
	else
		*command_queue = (struct command_node*)node;
	
	return 0;
}

/* Add read memory command nodes to command queue
 *
 * Returns 0 if succeeded and error value otherwise.
 * Error value is compliant with error function codes.
 *
 * Arguments:
 * command_queue    : pointer to command queue
 */
int add_command_read(struct command_node **command_queue) {

	int i = 0;
	struct command_node_read *node, *new_node;

	/* If command queue is empty then add first dummy node.
	 * Dummy node is used to simplify code at small memory cost.
	 */
	if (!*command_queue) {
		node = malloc(sizeof(struct command_node));
		if (!node)
			return ERROR_ALLOC;
		node->type = command_type_dummy;
		*command_queue = (struct command_node*)node;
	}

	/* Further function operation relies solely on 'node' variable, thus
	 * it has command_queue assigned to it before the loop
	 */
	node = (struct command_node_read*)(*command_queue);

	/* Create for every sector new read command node */
	for (i = 0; i < SECTOR_COUNT; ++i) {
		new_node = malloc(sizeof(struct command_node_read));
		if (!node)
			return ERROR_ALLOC;
		new_node->type = command_type_read;
		new_node->sector_address = i * SECTOR_SIZE;
		node->next_command = (struct command_node*)new_node;
		node = new_node;
	}

	/* Null last node's next node pointer for safety */
	node->next_command = NULL;

	return 0;
}

/* Read file and add write memory command nodes to command queue
 *
 * Returns 0 if succeeded and error value otherwise.
 * Error value is compliant with error function codes.
 *
 * Arguments:
 * command_queue    : pointer to command queue
 */
int add_command_write(
	struct command_node **command_queue,
	char* filepath,
	char* readout_str
	) {

	int fd, res, readout = 1;
	int sector = 0;
	int keep_reading = 1;
	struct command_node_write *node = NULL;
	struct command_node_write *new_node;
	uint8_t* data;

	/* Convert readout string to integer value */
	readout = str_to_readout(readout_str);

	/* Check if file was even given */
	if (!filepath)
		return ERROR_NO_FILE;

	/* Open data file */
	fd = open(filepath, O_RDONLY);
	if (fd < 0)
		return ERROR_FILE_OPEN;

	/* If command queue is empty then add first dummy node.
	 * Dummy node is used to simplify code at small memory cost.
	 */
	if (!*command_queue) {
		node = malloc(sizeof(struct command_node_write));
		if (!node)
			return ERROR_ALLOC;
		node->type = command_type_dummy;
		*command_queue = (struct command_node*)node;
	}

	/* Further function operation relies solely on 'node' variable, thus
	 * it has command_queue assigned to it before the loop
	 */
	node = (struct command_node_write*)(*command_queue);

	/* Divide file into sectors and make nodes for sectors */
	while (keep_reading) {
		/* Allocate memory for node and its data */
		new_node = malloc(sizeof(struct command_node_write));
		data = malloc(sizeof(uint8_t) * SECTOR_SIZE);
		if (!new_node || !data)
			return ERROR_ALLOC;
		
		/* Read data for buffer from file and watch for any error*/
		if ((res = read_data(fd, data, SECTOR_SIZE))) {
			/* If error occured while reading then return */
			if (res < 0)
				return res;
			/* If end of file encountered then do not loop again */
			else {
				keep_reading = 0;
			}
		}

		/* Set up new node */
		new_node->type = command_type_write;
		new_node->sector_address = sector * SECTOR_SIZE;
		new_node->sector_data = data;
		new_node->readout = readout;
		/* Assign new node as next node of current node */
		node->next_command = (struct command_node*)new_node;
		/* Change node */
		node = new_node;
		/* Update sector number */
		sector++;
	}

	/* Null last node's next node pointer for safety */
	node->next_command = NULL;

	/* Close file */
	close(fd);

	return 0;
}

/* Generates formatted string with message with instructions for
 * programmer device ready to be sent to programmer device
 *
 * Returns 0 if succeeded and error value otherwise.
 *
 * Arguments:
 * node     : pointer to first node in command queue (head of queue)
 * message	: pointer to string struct to which message will be saved
 */
int generate_message(
	struct command_node *node,
	struct string *message
	) {
	
	int i, res;
	char formatted[256];

	/* Write command set opening sign */
	if((res = string_append(message, "{\n")))
		return res;

	while(node) {
		switch (node->type) {
			/* Write command node */
			case command_type_write:
				sprintf(formatted,
					"\tw\n\t\ta %x$;\n\t\tr %x$;\n\t\td [",
					((struct command_node_write*)node)->sector_address,
					((struct command_node_write*)node)->readout);
				if ((res = string_append(message, formatted)))
					return res;
				for (i = 0; i < SECTOR_SIZE; ++i) {
					sprintf(formatted, "%x",
						((struct command_node_write*)node)->sector_data[i]
					);
					if ((res = string_append(message, formatted)))
						return res;
				}
				if ((res = string_append(message, "];\n\t;\n")))
					return res;
				break;
			/* Read command node */
			case command_type_read:
				sprintf(formatted, "\tr\n\t\ta %x$;\n\t;\n",
					((struct command_node_read*)node)->sector_address);
				if ((res = string_append(message, formatted)))
					return res;
				break;
			/* Erase command node */
			case command_type_erase:
				if ((res = string_append(message, "\te;\n")))
					return res;
				break;
			/* Endianness command node */
			case command_type_endianness:
				sprintf(formatted, "\tn\n\t\te %x$;\n\t;\n",
					((struct command_node_endianness*)node)->endianness);
				if ((res = string_append(message, formatted)))
					return res;
				break;
			/* Dummy command node */
			case command_type_dummy:
				break;
			/* Unknown command */
			default:
				return ERROR_UNKNOWN_COMMAND;
				break;
		}

		/* Change node to next one in queue */
		node = node->next_command;
	}

	/* Write command set closing sign */
	if((res = string_append(message, "}\n")))
		return res;

	return 0;
}

/*
 * Main program function
 */
int main(int argc, char** argv) {
	
	int c, res;
	char* command_str = NULL;
	char *device = DEFAULT_DEVICE;
	char* endianness_str = NULL;
	char* filepath = NULL;
	char* readout_str = NULL;

	struct command_node *command_queue;
	struct string message = { 0 };

	/* Parse program arguments */
	while((c = getopt(argc, argv, GETOPT_STRING)) != -1) {
		switch (c) {
			case 'c':
				command_str = optarg;
				break;
			case 'd':
				device = optarg;
				break;
			case 'e':
				endianness_str = optarg;
				break;
			case 'f':
				filepath = optarg;
				break;
			case 'g':
				printf("I'm a gummybear!\n");
				exit(0x67756D);
				break;
			case 'r':
				readout_str = optarg;
				break;

			case 'h':
				help(argv[0]);
				return 0;
			case ':':
				usage(argv[0]);
				return 1;
			case '?':
				usage(argv[0]);
				return 2;
			default:
				usage(argv[0]);
				return 3;
		}
	}

	/* Check if endianness was given */
	if (endianness_str) {
		/* Add endianness command node to command queue */
		if((res = add_command_endianness(&command_queue, endianness_str)))
			/* If error occured then print info and exit */
			error(res, argv[0]);
	}

	/* Handle command */
	if (command_str) {
		switch (*command_str) {
			/* Erase memory command */
			case 'e':
				if((res = add_command_erase(&command_queue)))
					error(res, argv[0]);
				break;
			/* Read memory command */
			case 'r':
				if ((res = add_command_read(&command_queue)))
					error(res, argv[0]);
				break;
			/* Write memory command */
			case 'w':
				if ((res = add_command_write(
						&command_queue,
						filepath,
						readout_str
					)))
					error(res, argv[0]);
				break;
			default:
				printf("Invalid command: %c\n", *command_str);
		}
	}
	else {
		fprintf(stderr, "No command provided\n");
		usage(argv[0]);
		return 5;
	}

	print_command_queue(command_queue);
	if ((res = generate_message(command_queue, &message)))
		error(res, argv[0]);

	printf("%s\nsize: %d\n", message.string, message.size);

	free_command_queue(command_queue);
	return EXIT_SUCCESS;
}
