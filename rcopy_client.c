// Library Includes
#include <stdio.h>
#include <sys/stat.h>

// File Includes
#include "ftree.h"
#include "hash.h"

int main(int argc, char **argv) {

    // Error checking for correct # of CMD-line arguments.
    if (argc != 4) {
        fprintf(stderr, "Required 4 Arguments, Currently: %d\n", argc);
		exit(1);
    }

    // To fcopy_client to copy files from client to server.
    int i = fcopy_client(argv[1], argv[2], argv[3], PORT);
    
    return i;
}
