/* Just to test the s4 lib */

#include "s4.h"
#include "strstore.h"
#include <stdio.h>
#include <string.h>


int getline (char *buffer, int size)
{
	if (fgets (buffer, size, stdin) == NULL)
		return 0;
	if (!strncmp (buffer, "DONE", 4))
		return 0;

	buffer[strlen (buffer) - 1] = '\0';

	return 1;
}

int main(int argc, char *argv[])
{
	char buffer[1024];
	s4_t *s4 = s4_open("s4.db");
	int ret;

	printf("Testing patricia trie string insertion\n\n");

	while (getline (buffer, 1024)) {
		strstore_ref_str (s4, buffer);
	}
	while (getline (buffer, 1024)) {
		ret = strstore_unref_str (s4, buffer);
		printf("u: %d\n", ret);
	}
	while (getline (buffer, 1024)) {
		ret = strstore_str_to_int (s4, buffer);
		printf("%i\n", ret);
	}

	s4_close (s4);

	return 0;
}
