#include <stdio.h>
#include <stdlib.h>

char* hash(FILE *f) {

	// To travel to the end of the file.
	fseek(f, 0, SEEK_END);
	// To get size of file.
	int size = ftell(f); 
	// To travel back to start of the file for fread.
	fseek(f, 0, SEEK_SET);

	// To allocate space for the contents of the file and use fread to read.
	char* contents = malloc(size);
	fread(contents, sizeof(char), size, f);

	// We generate an 8-bit with malloc (so it stays when function disappears).
	char* hash_val = malloc(8);
	hash_val[0] = '\0';
	hash_val[1] = '\0';
	hash_val[2] = '\0';
	hash_val[3] = '\0';
	hash_val[4] = '\0';
	hash_val[5] = '\0';
	hash_val[6] = '\0';
	hash_val[7] = '\0';

	// Index for contents.
	int i = 0;
	// Index for hash_val.
	int j = 0; 

	// To iterate over size and hash our hash_val.
	while (i < size) {
		hash_val[j] = hash_val[j] ^ contents[i];

		// Increment both indices.
		i++;
		j++;

		// Reset hash_val index whenever it hit 8 (out of bounds).
		if (j == 8) {
			j = 0;
		} 
	}

	return hash_val;
}