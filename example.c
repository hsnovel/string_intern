#include <stdio.h>
#include <string.h>

#include "string_intern.h"

int main()
{
	intern_table table;
	if (intern_init(&table)) {
		fprintf(stderr, "Unable to initialize intern table\n");
		return 0;
	}

	/* ptr3 have collusion with ptr1 and 2 so I checked for it spesifically*/
	char *ptr1 = intern_str(&table, "test1", strlen("test1"));
	char *ptr2 = intern_str(&table, "test1", strlen("test1"));
	char *ptr3 = intern_str(&table, "123111144412411", strlen("123111144412411"));

	if (ptr2 == ptr3) {
		printf("strings are equal\n");
	}

	if (ptr1 == ptr2) {
		printf("strings are equal\n");
	}


	return 0;
}
