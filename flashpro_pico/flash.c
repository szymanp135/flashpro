/*
 * Raspberry Pi Pico based double chip SST39SF020 automated flash memory
 * programmer.
 * Paweł Szymański 06.2026
 *
 * This software consists of following files:
 * - main.c - main file
 * - flash.c - library of flash functions
 * - command.c - library for reading commands
 *
 * This program is capable of:
 * Automated and convinient flash programming from PC with 'flashpro'
 * software.
 */

#include "flash.h"

#define LED 25

#define SLEEP_TIME_US 5
#define BUFFER_SIZE 2 * 4096
#define MEMORY_ADDRESS_SPACE 0x0007FFFF

#define D0 0
#define A0 0
#define A8 8
#define DATA_PIN_MASK 0xff << D0
#define ADDR_LOW_PIN_MASK 0xff << A0
#define ADDR_HIGH_PIN_MASK 0x3ff << A8

#define WE 18
#define OE 19
#define CE_HIGH 20
#define CE_LOW 21
#define BUFFER_CLK 22

#define WE_PIN_MASK 1 << WE
#define OE_PIN_MASK 1 << OE
#define CE_HIGH_PIN_MASK 1 << CE_HIGH
#define CE_LOW_PIN_MASK 1 << CE_LOW
#define BUFFER_CLK_PIN_MASK 1 << BUFFER_CLK

int ENDIAN = 0;

/* Initialize gpios, turn on pull-ups on control pins, put appropriate
 * levels on gpios, set gpios directions
 *
 * Returns nothing
 *
 * No arguments
 */
void flash_gpio_init(void) {

	/* Initialize LED */
	gpio_init(LED);
	gpio_put(LED, 0);
	gpio_set_dir(LED, GPIO_OUT);

	/* Initialize gpios */
	gpio_init_mask(BUFFER_CLK_PIN_MASK | WE_PIN_MASK | OE_PIN_MASK | CE_HIGH_PIN_MASK | CE_LOW_PIN_MASK
		| ADDR_HIGH_PIN_MASK | ADDR_LOW_PIN_MASK | DATA_PIN_MASK); 

	/* Turn pull-up resistors on control pins */
	gpio_pull_up(WE);
	gpio_pull_up(OE);
	gpio_pull_up(CE_HIGH);
	gpio_pull_up(CE_LOW);
	gpio_pull_down(BUFFER_CLK);

	/* Set data/address lines low while control lines high
	 * Bit set to 0 is low; set to 1 i high */
	gpio_put_masked(BUFFER_CLK_PIN_MASK | WE_PIN_MASK | OE_PIN_MASK | CE_HIGH_PIN_MASK |
		CE_LOW_PIN_MASK | ADDR_HIGH_PIN_MASK | ADDR_LOW_PIN_MASK | DATA_PIN_MASK,
		WE_PIN_MASK | OE_PIN_MASK | CE_HIGH_PIN_MASK | CE_LOW_PIN_MASK);

	/* Set gpios directories; gpios 0-7 are set as input to set them into
	 * high-impedance mode to prevent shorting circuit in case memory turns
	 * its pins on
	 * Bit set to 0 means input; set to 1 means output */
	gpio_set_dir_masked(BUFFER_CLK_PIN_MASK | WE_PIN_MASK | OE_PIN_MASK | CE_HIGH_PIN_MASK | CE_LOW_PIN_MASK
		| ADDR_HIGH_PIN_MASK | ADDR_LOW_PIN_MASK | DATA_PIN_MASK,
		BUFFER_CLK_PIN_MASK | WE_PIN_MASK | OE_PIN_MASK | CE_HIGH_PIN_MASK | CE_LOW_PIN_MASK
		| ADDR_HIGH_PIN_MASK | ADDR_LOW_PIN_MASK);
}

/* Prepare pico to work 
 *
 * Returns nothing
 *
 * No arguments
 */
void flash_init(void) {

	/* Initialize gpios */
	flash_gpio_init();

	/* Set endianness to default value (little-endianness) */
	ENDIAN = 0;
}

/* Write 'value' to address low buffer 
 *
 * Returns nothing.
 *
 * Argument:
 * value	: value to be writen to buffer
 */
void flash_write_address_buffer(uint8_t value) {

	/* Put 0 to clock pin */
	gpio_put(BUFFER_CLK, 0);

	/* Put data onto gpios */
	gpio_put_masked(ADDR_LOW_PIN_MASK, value << A0);

	/* Set address low pins as output */
	gpio_set_dir_masked(ADDR_LOW_PIN_MASK, ADDR_LOW_PIN_MASK);

	/* Drive clock pin high, wait a little and drive back low */
	sleep_us(1);
	gpio_put(BUFFER_CLK, 1);
	sleep_us(1);
	gpio_put(BUFFER_CLK, 0);
}

/* Outputs address onto address pins and drives appropriate CE control pin
 * low depending on given address
 *
 * REMEMBER TO DISABLE ADDRESS AFTER USAGE
 *
 * Returns nothing.
 *
 * Argument:
 * address	: Address to be enabled
 */
void flash_enable_address(uint32_t address) {

	uint which = ((address & 0x1) ^ ENDIAN) ? CE_HIGH : CE_LOW;
	address >>= 1;

	/* Set address low part */
	flash_write_address_buffer(address & 0xFF);

	/* Set address high part */
	gpio_put_masked(ADDR_HIGH_PIN_MASK, (address >> 8) << A8);

	/* Set CE pin low */
	//sleep_us(1);
	gpio_put(which, 0);

	/* Turn on LED to indicate CE is low */
	gpio_put(LED, 1);
}

/* Disables memory by driving CE pins high
 *
 * Returns nothing
 *
 * No arguments
 */
void flash_disable_address(void) {

	gpio_put_masked(CE_HIGH_PIN_MASK | CE_LOW_PIN_MASK,
		CE_HIGH_PIN_MASK | CE_LOW_PIN_MASK);

	/* Turn off LED to indicate CE is back high */
	gpio_put(LED, 0);
}

/* Put address and data byte onto pins and set WE to low for a period of
 * time.
 * This does not program any byte. It's used for commands.
 *
 * Returns nothing.
 *
 * Arguments:
 * address	: Address to be put onto address pins
 * byte		: Byte to be put onto data pins
 */
void flash_put_byte(uint32_t address, uint8_t byte) {

	/* Output address and enable memory */
	flash_enable_address(address);

	/* Set data direction as output and put data */
	gpio_set_dir_masked(DATA_PIN_MASK, DATA_PIN_MASK);
	gpio_put_masked(DATA_PIN_MASK, ((int)byte) << D0);

	/* Enable memory input (write data) */
	gpio_put(WE, 0);
	sleep_us(SLEEP_TIME_US);
	gpio_put(WE, 1);

	/* Set data direstion as input to prevent any shorts if present */
	gpio_set_dir_masked(DATA_PIN_MASK, 0);

	flash_disable_address();
	sleep_us(SLEEP_TIME_US);
}

/* Erase whole chip
 * Erasing prepares memory for writing
 *
 * Returns nothing
 *
 * No arguments
 */
void flash_chip_erase(void) {

	/* Perform Chip-Erase command */
	/* Erase low memory */
	flash_put_byte((0x5555 << 1) | 0x0, 0xAA);
	flash_put_byte((0x2AAA << 1) | 0x0, 0x55);
	flash_put_byte((0x5555 << 1) | 0x0, 0x80);
	flash_put_byte((0x5555 << 1) | 0x0, 0xAA);
	flash_put_byte((0x2AAA << 1) | 0x0, 0x55);
	flash_put_byte((0x5555 << 1) | 0x0, 0x10);

	/* Erase high memory */
	flash_put_byte((0x5555 << 1) | 0x1, 0xAA);
	flash_put_byte((0x2AAA << 1) | 0x1, 0x55);
	flash_put_byte((0x5555 << 1) | 0x1, 0x80);
	flash_put_byte((0x5555 << 1) | 0x1, 0xAA);
	flash_put_byte((0x2AAA << 1) | 0x1, 0x55);
	flash_put_byte((0x5555 << 1) | 0x1, 0x10);

	/* Wait 100ms for command to finish */
	sleep_ms(100);
}

/* Erases sector containing given address
 *
 * Returns nothing
 *
 * Arguments:
 * address	: Address contained by sector to be erased
 */
void flash_sector_erase(uint32_t address) {

	/* Clear bits not needed for sector address */
	uint32_t sector = address & 0xfffff000;

	/* Perform Sector-Erase command */
	/* Erase low memory sector */
	flash_put_byte((0x5555 << 1) | 0x0, 0xAA);
	flash_put_byte((0x2AAA << 1) | 0x0, 0x55);
	flash_put_byte((0x5555 << 1) | 0x0, 0x80);
	flash_put_byte((0x5555 << 1) | 0x0, 0xAA);
	flash_put_byte((0x2AAA << 1) | 0x0, 0x55);
	flash_put_byte((sector << 1) | 0x0, 0x30);

	/* Erase high memory sector */
	flash_put_byte((0x5555 << 1) | 0x1, 0xAA);
	flash_put_byte((0x2AAA << 1) | 0x1, 0x55);
	flash_put_byte((0x5555 << 1) | 0x1, 0x80);
	flash_put_byte((0x5555 << 1) | 0x1, 0xAA);
	flash_put_byte((0x2AAA << 1) | 0x1, 0x55);
	flash_put_byte((sector << 1) | 0x1, 0x30);

	/* Wait 25ms for command to finish */
	sleep_ms(25);
}

/* Write byte to memory at given address
 *
 * Returns nothing.
 *
 * Arguments:
 * address	: Addresss where data is to be saved
 * byte		: Data to be saved
 */
void flash_write_byte(uint32_t address, uint8_t byte) {

	int which = address & 0x1;
	/* Perform Byte-Program command */
	flash_put_byte((0x5555 << 1) | which, 0xAA);
	flash_put_byte((0x2AAA << 1) | which, 0x55);
	flash_put_byte((0x5555 << 1) | which, 0xA0);
	flash_put_byte(address, byte); /* Write byte itself at given address */

	/* Sleep 20µs for command to finish internally */
	sleep_us(20);
}

/* Read byte from memory at given address
 *
 * Returns read character.
 *
 * Arguemnt:
 * address	: Addresss from which data is read
 */
uint8_t flash_read_byte(uint32_t address) {

	char result = 0;

	/* Output address and enable memory */
	flash_enable_address(address);

	/* Set data direction as input */
	gpio_set_dir_masked(DATA_PIN_MASK, 0);

	/* Enable memory output (set OE to low) */
	gpio_put(OE, 0);

	/* Read result */
	sleep_us(SLEEP_TIME_US);
	result = (gpio_get_all() & DATA_PIN_MASK) >> D0;

	/* Disable memory (set OE and CE to high) */
	gpio_put(OE, 1);
	flash_disable_address();
	sleep_us(SLEEP_TIME_US);

	return result;
}

/* Write data buffer into sector.
 * data buffer has to match its size with BUFFER_SIZE
 *
 * SECTOR HAS TO BE ERASED BEFORE WRITING
 *
 * Returns nothing
 *
 * Arguments:
 * address	: address contained in sector to be written to
 * data		: buffer with data to be written to the sector
 */
void flash_write_sector(uint32_t address, uint8_t* data) {
	
	uint32_t i;
	uint32_t sector = address & 0xfffff000;

	for (i = 0; i < BUFFER_SIZE; ++i) {
		flash_write_byte(sector | i, data[i]);
	}
}

/* Read sector data into data buffer
 * data buffer has to match its size with BUFFER_SIZE
 *
 * Returns nothing
 *
 * Arguments:
 * address	: address contained in sector to be written to
 * data		: buffer to be filled with sector's data
 */
void flash_read_sector(uint32_t address, uint8_t* data) {

	uint32_t i;
	uint32_t sector = address & 0xfffff000;

	for (i = 0; i < BUFFER_SIZE; ++i) {
		data[i] = flash_read_byte(sector | i);
	}
}

/* Print data buffer of given length
 *
 * Returns nothing
 *
 * Arguments:
 * data		: buffer with data to be printed
 * length	: data buffer's length
 */
void flash_print_buffer(uint8_t* data, int length) {
	
	int i;

	for (i = 0; i < length; ++i) {
		if (!(i & 0x0f))
			printf("\n%05X:", i);

		printf(" %02X", data[i]);
	}
}

