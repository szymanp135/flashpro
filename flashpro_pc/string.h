#ifndef STRING_H
#define STRING_H

#include <stdlib.h>
#include <string.h>
#include "constants.h"
#include "error.h"

/* Simple dynamic string implementation struct */
struct string {
	/* string's text */
	char* string;
	/* string's text allocated size */
	int size;
};

int string_append(struct string *, char *);
void string_empty(struct string *);

#endif
