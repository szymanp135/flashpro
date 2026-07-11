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
#include <sys/wait.h>
#include <string.h>

#include "constants.h"
#include "command.h"
#include "error.h"
#include "state.h"
#include "string.h"
#include "util.h"

#define GETOPT_STRING "c:d:e:f:ghmpqr:"
#define DEFAULT_DEVICE "/dev/ttyACM0"

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
	printf(" - m: [DEBUG] print generated messages\n");
	printf(" - p: [DEBUG] print command queue\n");
	printf(" - q: quiet output\n");
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

/* Print data array of given length
 *
 * Returns nothing
 *
 * Arguments:
 * data		: buffer with data to be printed
 * length	: data buffer's length
 */
#define LINE_LENGTH 0x10
void print_array(uint8_t* data, int length) {
	
	int i, j;
	int is_repeating_pattern;
	int prev_pattern[LINE_LENGTH] = { 0 };
	int skip_line_print = 0;

	printf("\nMemory contents:");

	/* Reset pattern array */
	memset(prev_pattern, -1, sizeof(prev_pattern));

	for (i = 0; i < length; ++i) {
		/* For each new line to be printed check if every character is
		 * the same as the previous character. If it is then skip the line.
		 * Otherwise print the line address info */
		if (!(i & 0x0f)) {
			/* Reset repeating character info */
			is_repeating_pattern = 1;
			/* Scan line whether is has different characters */
			for (j = 0; j < LINE_LENGTH; ++j)
				if (prev_pattern[j] != data[i + j]) {
					is_repeating_pattern = 0;
					skip_line_print = 0;
					break;
				}
			/* If characters are repeating then skip printing the line */
			if (is_repeating_pattern) {
				i += j - 1;
				if (!skip_line_print) {
					printf("\n       ... (repeating as above)");
					skip_line_print = 1;
				}
				continue;
			}
			/* Print line address */
			printf("\n%05X:", i);
		}

		/* Print array content */
		printf(" %02X", data[i]);
		/* Update previous character */
		prev_pattern[i & 0x0f] = data[i];
	}

	printf("\n");
}


/* Set up tty device to properly format incoming data from pico
 * Removes echo ("-echo") to not to receive sent data repeatedly and
 * removes formatting with additional carrige return symbol ("raw").
 *
 * Returns 0 if succeeded and error value otherwise.
 *
 * Arguments:
 * device   : path to device
 */
int setup_device_tty(char *device) {

	int stat;
	pid_t pid;
	char *argv[] = { "/usr/bin/stty", "-F", "", "raw", "-echo", "115200",
		NULL };

	/* Fork */
	pid = fork();
	if (pid < 0)
		return ERROR_FORK;
	
	/* As a parent wait for child process */
	if (pid) {
		wait(&stat);
		if (stat)
			return ERROR_FORK;
	}
	/* As a child execute setting command */
	else {
		argv[2] = device;
		execve(argv[0], argv, NULL);
		/* program should not execute any code of old program after execv
		 * while successful, thus reaching this section means error.
		 */
		exit(-1);
	}

	return 0;
}

/* Generates formatted string with message with single command ready
 * to be sent to device.
 * Commands have following format:
 * - available defined commands:
 *   - 'e' for erase
 *   - 'n' for endianness
 *   - 'r' for read
 *   - 'w' for write
 * - most of them have arguments, such as eg. sector address
 * - numbers are written in hex and end with '$'
 * - byte data is written in hex
 * - every command and argument has to end with semicolon ';'
 * - whitespaces are irrelevant
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

	/* Depending on node type generate appropriate message */
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

	return 0;
}

/*
 *
 *
 *
 *
 */
int process_received_message(
	struct string *response,
	int *address,
	uint8_t *buffer
	) {
	
	int value = 0;
	char *c;
	enum state_program state = s_init;

	/* Initialize c - current character pointer */
	c = response->string - 1;

	while(*(++c)) {
		/* Skip white spaces */
		if (is_whitespace(*c))
			continue;

		/* Change state upon character under c */
		state_handle_char(*c, &state);

		/* Execute an action upon state if necessary */
		switch(state) {
			/* Idle states */
			case s_init:
			case s_data_frame:
			case s_data:
				break;
			/* Ok reponse value */
			case s_ok:
				value = parse_int(&c);
				break;
			/* Data response address */
			case s_data_address:
				*address = parse_int(&c);
				break;
			/* Data response data array */
			case s_data_data:
				parse_buffer(&c, buffer);
				break;
			/* Other state values indicate error */
			default:
				return ERROR_WRONG_STATE;
		}
	}

	if (value != s_data_frame) {
		return ERROR_WRONG_DEVICE_STATE;
	}

	return 0;
}

/* Handle communication between computer and device.
 * Goes through whole command queue and for each command node prepares
 * a message, sends it and waits to receive response.
 * Message is of following format:
 * - starts with '{' and ends with '}'
 * - inside command is defined:
 *   - 'e' for erase
 *   - 'n' for endianness
 *   - 'r' for read
 *   - 'w' for write
 * - most commands have arguments, such as eg. sector address
 * - numbers are written in hex and end with '$'
 * - byte data is written in hex
 * - every command and argument has to end with semicolon ';'
 * - whitespaces are irrelevant
 * After receiving a response it is processed. Afterwards cycle is
 * repeated for next node.
 *
 * Returns 0 if succeeded and error value otherwise.
 *
 * Arguments:
 * devicepath       : string with device file path
 * node             : command queue first node
 * print_message    : whether to print message sent to device
 * print_progress   : whether to print progress of commands execution
 */
int communicate_with_device(
	char *devicepath,
	struct command_node *node,
	int print_message,
	int print_progress
	) {
	
	int fd, res, address, read_command = 0, i = 0;
	int command_count, current_command = 0;
	char progress_bar[PROGRESS_BAR_LENGTH + 1] = { 0 };
	uint8_t buffer[SECTOR_SIZE];
	uint8_t read_memory[MEMORY_SIZE];
	struct string send_message_string = { 0 };
	struct string receive_message_string = { 0 };
	struct string command_message_string = { 0 };

	/* If progress is to be printed then count up commands in queue
	 * and reset progress bar */
	if (print_progress) {
		command_count = count_commands(node);
		printf("\n");
		memset(progress_bar, ' ', PROGRESS_BAR_LENGTH);
	}

	while (node) {
		/* If progress is to be printed then print the progress */
		if (print_progress) {
			/* Update current command index */
			++current_command;
			/* Go to previous line and clear it */
			printf("\033[1A\r\033[2K\r");
			/* Update progress_bar */
			for (; i < PROGRESS_BAR_LENGTH * current_command / command_count; ++i)
				progress_bar[i] = '#';
			/* Print progress info */
			printf("Progress: [%s] %2d/%d    Executing command: %s\n",
				progress_bar,
				current_command,
				command_count,
				command_type_string[node->type - command_type_dummy]
			);
			fflush(stdout);
		}

		/* Skip dummy nodes */
		if (node->type == command_type_dummy) {
			node = node->next_command;
			continue;
		}

		/* Empty message to be sent */
		string_empty(&send_message_string);
		string_empty(&receive_message_string);
		string_empty(&command_message_string);

		/* Generate command string */
		if ((res = generate_message(node, &command_message_string)))
			return res;

		/* Create whole message stored in send_message string */
		/* Open message */
		if ((res = string_append(&send_message_string, "{\n")))
			return res;
		/* Add command string to the message */
		if ((res = string_append(
				&send_message_string,
				command_message_string.string
			)))
			return res;
		/* Close message */
		if ((res = string_append(&send_message_string, "}\n")))
			return res;

		/* Print message to be sent if such option is set */
		if (print_message)
			printf("message to send:\n%s\n", send_message_string.string);

		/* Send prepared message and receive response from device */
		/* Open device file */
		fd = open(devicepath, O_RDWR | O_NONBLOCK);
		if (fd < 0)
			return ERROR_DEVICE_OPEN;
		/* Send message */
		if ((res = send_message(&send_message_string, fd)))
			return res;
		/* Receive message */
		if ((res = receive_message(&receive_message_string, fd)))
			return res;
		/* Close device file */
		close(fd);

		/* Reset address value */
		address = -1;
		/* Process received response */
		if ((res = process_received_message(
				&receive_message_string,
				&address,
				buffer
			)))
			return res;

		/* If command was a read then save read buffer into memory array */
		if (node->type == command_type_read) {
			/* If address was not read then return error */
			if (address < 0)
				return ERROR_NO_READ_BUFFER_ADDRESS;
			/* Copy buffer contents into memory array */
			memcpy(read_memory + address, buffer, sizeof(buffer));
			/* Set flag to inform about read command presence */
			read_command = 1;
		}

		/* Move to the next node */
		node = node->next_command;
	}

	/* If read command was present then print memory */
	if (read_command) {
		print_array(read_memory, sizeof(read_memory));
	}

	/* Free allocated memory if there was any */
	if (send_message_string.string) free(send_message_string.string);
	if (receive_message_string.string) free(receive_message_string.string);
	if (command_message_string.string) free(command_message_string.string);

	return 0;
}

/*
 * Main program function
 */
int main(int argc, char **argv) {
	
	int c, res;
	int print_queue = 0;
	int print_message = 0;
	int print_progress = 1;

	char *command_str = NULL;
	char *device = DEFAULT_DEVICE;
	char *endianness_str = NULL;
	char *filepath = NULL;
	char *readout_str = NULL;

	struct command_node *command_queue = NULL;

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
			case 'm':
				print_message = 1;
				break;
			case 'p':
				print_queue = 1;
				break;
			case 'q':
				print_progress = 0;
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
			error(res, argv[0], &usage);
	}

	/* Handle command */
	if (command_str) {
		switch (*command_str) {
			/* Erase memory command */
			case 'e':
				if((res = add_command_erase(&command_queue)))
					error(res, argv[0], &usage);
				break;
			/* Read memory command */
			case 'r':
				if ((res = add_command_read(&command_queue)))
					error(res, argv[0], &usage);
				break;
			/* Write memory command */
			case 'w':
				if (!readout_str)
					error(ERROR_NO_READOUT_STR, argv[0], &usage);
				if ((res = add_command_write(
						&command_queue,
						filepath,
						readout_str
					)))
					error(res, argv[0], &usage);
				break;
			default:
				printf("Invalid command: %c\n", *command_str);
		}
	}
	/* Print error if no command provided */
	else {
		fprintf(stderr, "No command provided\n");
		usage(argv[0]);
		return 5;
	}

	/* Print command queue if such option is set */
	if (print_queue)
		print_command_queue(command_queue);
	
	/* Set up device with proper settings */
	if ((res = setup_device_tty(device)))
		error(res, argv[0], &usage);

	/* Communicate with device - send commands, receive responses
	 * and process them */
	if ((res = communicate_with_device(
			device,
			command_queue,
			print_message,
			print_progress
		)))
		error(res, argv[0], &usage);

	/* Free allocated memory for command queue */
	free_command_queue(command_queue);
	return EXIT_SUCCESS;
}
