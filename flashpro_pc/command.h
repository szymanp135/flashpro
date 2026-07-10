#ifndef COMMAND_H
#define COMMAND_H

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "constants.h"
#include "util.h"
#include "error.h"

/* This enum is used to determine what command given structure represents.
 * It is a part of polimorphism simulation.
 */
enum command_type {
	command_type_dummy = 0,
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

/* Function declarations */
int add_command_endianness(struct command_node **, char *);
int add_command_erase(struct command_node **);
int add_command_read(struct command_node **);
int add_command_write(struct command_node **, char *, char *);
void free_command_queue(struct command_node *);
void print_command_queue(struct command_node *);

#endif

