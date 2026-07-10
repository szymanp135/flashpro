#include "string.h"

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

/* Empty string if possible.
 * Write null terminating sign in place of first character of string.
 *
 * Returns nothing
 *
 * Arguments:
 * string   : string to be emptied
 */
void string_empty(struct string *string) {
	
	if (string && string->size > 0) {
		string->string[0] = 0;
	}
}

