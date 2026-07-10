#include "util.h"

/* Hang program execution for some time in nano seconds
 *
 * Returns 0 if succeeded and error value otherwise.
 *
 * Arguments:
 * ns       : nano seconds to sleep
 */
int sleep_ns(long int ns) {
	
	int res;
	struct timespec to_sleep  = { 0 };
	struct timespec remaining = { 0 };

	to_sleep.tv_sec = ns / 1000000000;
	to_sleep.tv_nsec = ns % 1000000000;

	/* Loop till program has slept enough */
	while(2137) {
		res = nanosleep(&to_sleep, &remaining);
		/* If sleep was interrupted then try again with remaining time */
		if (res && errno == EINTR) {
			to_sleep.tv_sec = remaining.tv_sec;
			to_sleep.tv_nsec = remaining.tv_nsec;
		}
		/* Error */
		else if (res)
			return res;
		/* Program has slept well for desired amount of time */
		else
			return 0;
	}
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

/* Send message with commands to the device
 *
 * Returns 0 if succeeded and error value otherwise
 *
 * Arguments:
 * message  : message with commands to be sent
 * fd       : file descriptor of device file
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

/* Receive message from the device.
 * Tries 'retry' times to read data from device and waits around 1µs
 * between retries. This retry count is reset every time data is read,
 * though only initial value is great enough to account for device
 * computations. After initial data receive following data should arrive
 * quickly enough.
 *
 *
 * Returns 0 if succeeded and error value otherwise
 *
 * Arguments:
 * received_message : message where received message will be saved
 * fd               : non-blocking file descriptor of device file
 */
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

