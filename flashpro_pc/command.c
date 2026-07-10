#include "command.h"

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
 * filepath         : path to file to be written
 * readout_str      : string indicating whether to perform readout after
 *                    write
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
	if (readout < 0)
		return ERROR_READOUT_STR;

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

