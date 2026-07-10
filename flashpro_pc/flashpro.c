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
#include "string.h"
#include "util.h"
#include "error.h"

#define GETOPT_STRING "c:d:e:f:ghmqr:"
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
	printf(" - m: [DEBUG] print generated message\n");
	printf(" - q: [DEBUG] print command queue\n");
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

/* Send message with commands to the device
 *
 * Returns 0 if succeeded and error value otherwise
 *
 * Arguments:
 * message  : message with commands to be sent
 * device   : path to device to which send the message
 */
int send_message(struct string *message, int fd) {

	int res;
	int bytes_written = 0;
	int bytes_to_write;

	bytes_to_write = strlen(message->string);

	/* Send complete message to the device */
	while (bytes_to_write) {
		res = write(fd, message->string + bytes_written, bytes_to_write);
		if (res < 0) {
			if (errno != EINTR)
				return ERROR_FILE_WRITE;
		}
		else {
			bytes_written += res;
			bytes_to_write -= res;
		}
	}

	return 0;
}

/*
 *
 *
 */
#define FIRST_READ_RETRY_COUNT 30000
#define FOLLOWING_READ_RETRY_COUNT 100
int receive_message(struct string *received_message, int fd) {

	int res;
	int retry = FIRST_READ_RETRY_COUNT;
	ssize_t n;
	char received[256];

	/* Read until there is no more data */
	while (retry) {
		n = read(fd, received, 255);
		/* If data was read then append it to string */
		if (n >= 0) {
			received[n] = 0;
			if ((res = string_append(received_message, received)))
				return res;
			retry = FOLLOWING_READ_RETRY_COUNT;
		}
		/* If no data read decrement retry count */
		else if (errno == EAGAIN) {
			retry--;
		}
		/* Handle error */
		else
			return ERROR_FILE_READ;

		/* Wait 1 µs for data */
		sleep_ns(1000);
	}

	return 0;
}

int process_received_message(struct string *response) {
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
 * devicepath   : string with device file path
 * node         : command queue first node
 * print_message: whether to print message sent to device
 */
int communicate_with_device(
	char *devicepath,
	struct command_node *node,
	int print_message
	) {
	
	int fd, res;
	struct string send_message_string = { 0 };
	struct string receive_message_string = { 0 };
	struct string command_message_string = { 0 };

	while (node) {
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

		/* Process received response */
		if ((res = process_received_message(&receive_message_string)))
			return res;

		/* Move to the next node */
		node = node->next_command;
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
			case 'q':
				print_queue = 1;
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
			print_message
		)))
		error(res, argv[0], &usage);

	/* Free allocated memory for command queue */
	free_command_queue(command_queue);
	return EXIT_SUCCESS;
}
