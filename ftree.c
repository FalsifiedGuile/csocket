#include <stdio.h>
//
// Basic Library Includes
#include <stdlib.h>
#include <string.h>
// For dirent, lstat, socket, directory calls & fileinfo struct.
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>    /* Internet domain header */
#include <arpa/inet.h>
 #include <libgen.h>

// Files Includes
#include "ftree.h"
#include "hash.h"

// Constants.
#define MAX_BACKLOG 1

// Function Headers
int recieve_client(int sd_client);
int client_copy(const char *src, const char *dest, int sock_fd, int index);
char* concatenate_string(char* s1, char* s2);

void fcopy_server(int port) {

    // To create a Socket File Descriptor.
    int sock_fd = socket(AF_INET, SOCK_STREAM, 0);
	
    // To return an error if the Socket call fails.
    if (sock_fd < 0) {
        perror("server: socket");
        exit(1); 
    }

    // To Bind socket to an address
    struct sockaddr_in server;
    server.sin_family = AF_INET;
    server.sin_port = htons(port);  // Note use of htons here
    server.sin_addr.s_addr = INADDR_ANY;
    memset(&server.sin_zero, 0, 8);  // Initialize sin_zero to 0
	
	// To bind the server to a unique socket. 
    if (bind(sock_fd, (struct sockaddr *)&server, sizeof(struct sockaddr_in)) < 0) {
        perror("server: bind");
        close(sock_fd);
        exit(1);
    }

    // To create queue in kernel for new connection requests
    if (listen(sock_fd, MAX_BACKLOG) < 0) {
        perror("server: listen");
        close(sock_fd);
        exit(1);
    }

    // Variable Declaration(s) for server connections.
    int client_fd;
	int recievedAction;

    // While loop to continously await new connections.
    while(1) {
		recievedAction = 0;
		client_fd = accept(sock_fd, NULL, NULL);
        // Error checking for if accept call fails.
		if (client_fd < 0) {
		    perror("server: accept");
		    close(sock_fd);
		    exit(1);
		}
    	// To recieve the client
		while(recievedAction != 2){
			recievedAction = recieve_client(client_fd);
            // Error checking for if the client exited with failure.
			if (recievedAction != 0 && recievedAction != 2){
				fprintf(stderr, "error: %d when recieving client", recievedAction);
			}
		}
	} 
    // To close the connection & socket.    
    close(sock_fd);
}


int recieve_client(int sd_client){
        
    // Struct Declaration(s).
    struct fileinfo *filestruct = malloc(sizeof(struct fileinfo));
    
    // Variable Declaration(s).
    int match;
    int write_val;
    int read_val;
    int transmit;

    // To read data from the client.
    read_val = read(sd_client, filestruct, sizeof(struct fileinfo)); 
	
    // Error checking for if read system call failed.
    if (read_val < 0){
        perror("read");
        return 2;
    }
	// Check if we are finished with the client by seeing if there is a keyword.
	if (strcmp(filestruct->path, "nickerino") == 0){
		return 2;
	}
	// Struct Declarations.
    struct stat my_stat; 
	char* path_dest;

    // If the passed in file is a regular file.
    if (S_ISREG(filestruct->mode) == 1) {
        
        // To try to open the file (to check existence).
        FILE *checkFile = fopen(filestruct->path, "rb");
		
		// String literal(s) for comparisons.
		path_dest = strdup(filestruct->path);

		// To find information about the file.
		lstat(path_dest, &my_stat); 
		// To check failure (src is not a file or directory).
		DIR *myDir = opendir(filestruct->path);
		
		if (((S_ISREG(my_stat.st_mode) != 1) && (checkFile != NULL)) || ((S_ISDIR(my_stat.st_mode) == 1) && myDir != NULL)){
			if (myDir != NULL){
				closedir(myDir);
			}
			match = 3;
			fprintf(stderr, "MATCH_ERROR file found! at %s\n", filestruct->path);
			write_val = write(sd_client, &match, sizeof(int));
			if (write_val < 0) {
				perror("write");
				return 2;
			}
			return 2;
		}
		if (myDir != NULL){
				closedir(myDir);
		}
        // If the file doesn't exist.
        if (checkFile == NULL) {
            match = 1;
            if(filestruct->size == 0) {
                FILE *fp;
                // To try to open the file to append.
                fp = fopen(filestruct->path, "wb");
                fclose(fp);
                match = 2;
            }
		
        }
        // If the file does exist.
        else {

            // To get size of the file.
            fseek(checkFile, 0, SEEK_END); // To travel to the end of the file.
            int size = ftell(checkFile); // To get size of file.
            fseek(checkFile, 0, SEEK_SET); // To travel back to start of the file for fread.

            // To read the file contents.
            char* contents = malloc(size);
            fread(contents, sizeof(char), size, checkFile);

            // To get a hash of the file on the directory side.
            char* new_hash = hash(checkFile);
			
			// To close the checkFile.
			if (checkFile != NULL){
				fclose(checkFile);
			}
            // If the hashes or size are different:
            if ((strcmp(new_hash, filestruct->hash) != 0) || (filestruct->size != size)) {
                match = 1;
                FILE *fp;
                // To try to open the file to append.
                fp = fopen(filestruct->path, "rb");
                if (fp != NULL){
                    int removeAction = remove(filestruct->path);
                    if (removeAction < 0){
                        perror("remove");
						return 2;
                    }
					fclose(fp);
                }
                
            } 
            // Otherwise, the hashes are the same:
            else {
                match = 2;
            }

            if(filestruct->size == 0) {
                FILE *fp;
                // To try to open the file to append.
                fp = fopen(filestruct->path, "wb");
				if (fp != NULL){
					fclose(fp);
				}
                match = 2;
            }
        }

        // To pass the match code to the client.
        write_val = write(sd_client, &match, sizeof(int));
        
        // Error checking for if write system call failed.
        if (write_val < 0) {
            perror("write");
            return 2;
        }
        
        // If match == 1, we have to receive incoming data from the client (fcopy).
        if (match == 1){

            // Variables we need:
            unsigned char buf[8192] = {0};
            memset(buf, '0', sizeof(buf));
            unsigned long transferByte = 0;
            FILE *fp;

            // To try to open the file to append.
            fp = fopen(filestruct->path, "ab"); 
            
            // Error checking for if fopen system call fails.
            if (NULL == fp) {
                perror("open");
                match = 2;
            }
			// To see how many bytes we have transfered.
			unsigned long transferedByte = 0;
            // To write incoming data.
            while (1) {
				
				transferByte = read(sd_client, buf, 8192);
				if (transferByte < 0){
					perror("read");
					match = 2;
				}
                int wrote = fwrite(buf, sizeof(char), transferByte, fp);
				transferedByte += wrote;
                if (wrote != transferByte){
					transmit = 5;
					exit(1);
				} else{
					transmit = 4;
				}
				
				write_val = write(sd_client, &transmit, sizeof(int));
				if (write_val < 0) {
					perror("write");
					match = 2;
				}
				if (transferedByte == (unsigned long) filestruct->size){
					fclose(fp);
					break;
				}
            }
        }
		
		// change the permission of the file to the new permission
		int old_permission = filestruct->mode & 0777;
		int chmodAction = chmod(filestruct->path, old_permission);
		if (chmodAction < 0){
			perror("chmod");
			transmit = 5;
			write_val = write(sd_client, &transmit, sizeof(int));
			return 2;
		}
		
    }

    else if (S_ISDIR(filestruct->mode) == 1) {
		
        int old_permission;
        // To open directory.
        DIR *myDir = opendir(filestruct->path);
		// String literal(s) for comparisons.
		int i = strlen(filestruct->path);
		//create a string to check if the regular file already exists.
		char *stringname = malloc(i);
		strcpy(stringname, filestruct->path);
		stringname[i] = '\0';
		path_dest = strdup(stringname);
		// To check if reg file exists.
		FILE *filecheck = fopen(stringname, "rb");
		// To find information about the file.
		lstat(path_dest, &my_stat); 
		// To check failure (src is not a file or directory).
		if ((S_ISREG(my_stat.st_mode) == 1) && (filecheck != NULL)){
			match = 3;
			fprintf(stderr, "MATCH_ERROR found! at %s\n", filestruct->path);
			write_val = write(sd_client, &match, sizeof(int));
			if (write_val < 0) {
				perror("write");
				match = 2;
			}
			return 2;
		}
		
		if (filecheck != NULL){
			fclose(filecheck);
		}
		
		// To pass the match code to the client.
		match = 1;
        write_val = write(sd_client, &match, sizeof(int));
        
        // Error checking for if write system call failed.
        if (write_val < 0) {
            perror("write");
            match = 2;
        }
        // An error message to stderr if src is not a directory and return null.
        if (myDir == NULL) {
            old_permission = filestruct->mode & 0777;
            int makeAction = mkdir(filestruct->path, old_permission);
			if (makeAction < 0){
				transmit = 5;
				write_val = write(sd_client, &transmit, sizeof(int));
				perror("mkdir");
				return 2;
				
			}
        } else{
			old_permission = filestruct->mode & 0777;
			int chmodAction = chmod(filestruct->path, old_permission);
			if (chmodAction < 0){
				perror("chmod");
				transmit = 5;
				write_val = write(sd_client, &transmit, sizeof(int));
				if (write_val < 0) {
					perror("write");
					match = 2;
				}
				return 2;
			}
		}
        // Set transmit value to 4
        transmit = 4;
        write_val = write(sd_client, &transmit, sizeof(int));
		
        if (write_val < 0) {
            perror("write");
        }

		closedir(myDir);
    }

    return 0;
}


int fcopy_client(char *src_path, char *dest_path, char *host, int port) {

    // Setting up the Socket File Descriptor.
    int sock_fd = socket(AF_INET, SOCK_STREAM, 0);
	struct fileinfo *filestruct = malloc(sizeof(struct fileinfo));
    // Setting up the sockaddr_in to connect to.
    struct sockaddr_in server;
    server.sin_family = AF_INET;
    server.sin_port = htons(PORT);

    // The server runs on the IP Address of host (for reference local machine: 127.0.0.1).
    if (inet_pton(AF_INET, host, &server.sin_addr) < 0) {
        perror("client: inet_pton");
        close(sock_fd);
        exit(1);
    }
	
	char *backslash;
	int index;
	backslash = strrchr(src_path, '/');
	if (backslash != NULL){
		index = (int)(backslash - src_path);
	} else {
		index = 0;
	}
	
    // To connect to the server
    if (connect(sock_fd, (struct sockaddr *) &server, sizeof(server)) == -1) {
        perror("client: connect");
        close(sock_fd);
        exit(1);
    }
	

	int clientAction = client_copy(src_path, dest_path, sock_fd, index);
	strcpy(filestruct->path, "nickerino");
	write(sock_fd, filestruct, sizeof(struct fileinfo));
    close(sock_fd); 
    return clientAction;
    
}


int client_copy(const char *src, const char *dest, int sock_fd, int index) {

    // Struct Declarations.
    struct dirent *my_dirent;
    struct stat my_stat; 
    struct fileinfo *filestruct = malloc(sizeof(struct fileinfo)); 

    // String literal(s) for comparisons.
    char* new_src = strdup(src);
    char* new_dest = strdup(dest);

    // Variable Declarations:
    int match;
    int write_val;
    int read_val;
	int transmit;

    // To find information about the file.
    int find_src = lstat(new_src, &my_stat); 

    // To check failure (src is not a file or directory).
    if (find_src < 0) {
        perror("lstat");
        exit(1);
    }

    // If the file is a regular file.
    if (S_ISREG(my_stat.st_mode) == 1) {

        // To open the file.
        FILE *my_file = fopen(new_src, "r");

        // Error-checking for if the file is not-openable.
        if (my_file == NULL) {
            perror("fopen");
            exit(1);
        }

        // To get size of the file.
        fseek(my_file, 0, SEEK_END); // To travel to the end of the file.
        int size = ftell(my_file); // To get size of file.
        fseek(my_file, 0, SEEK_SET); // To travel back to start of the file for fread.

        // To read the file contents.
        char* contents = malloc(size);
        fread(contents, sizeof(char), size, my_file);
		char* new_path = concatenate_string(new_dest, "/");
        // To populate the fileinfo struct. 
		char *new_index_src;
		char* new_path2;
		if (index > 0){
			new_index_src = &new_src[index+1];
			new_path2 = concatenate_string(new_path, new_index_src);
		} else {
			new_path2 = concatenate_string(new_path, new_src);
		}
		
        
		strcpy(filestruct->hash, hash(my_file));
        strcpy(filestruct->path, new_path2);
        filestruct->size = size;
        filestruct->mode = my_stat.st_mode;
        

        // To write/pass filestruct to the server.
        write_val = write(sock_fd, filestruct, sizeof(struct fileinfo));

        // Error checking for if write system call failed.
        if (write_val < 0) {
            perror("write");
            exit(1);
        }

        // To read our match value back from the server.
        read_val = read(sock_fd, &match, sizeof(match));
		if (match == 3){
			fprintf(stderr, "MATCH_ERROR found!\n");
			exit(1);
		}

        // Error checking for if read system call failed.
        if (read_val < 0) {
            perror("read");
            exit(1);
        }

        // To close file after use.
		fclose(my_file);

        // If match == 1, we must copy the file.
		if (match == 1){

            // To read the file (transfer).
			FILE *fp = fopen(new_src, "rb");

            // Error checking for if read fails.
			if (fp == NULL){
				perror("read");
			}
            // Read loop.
			while (1) {
				
                // To read from the file in chunks of 8192 bytes
				unsigned char buf[8192] = {0};
				
				int fileread = fread(buf, sizeof(char), 8192, fp);
     
                if (fileread > 0) {
					
                    write_val = write(sock_fd, buf, fileread);
					if (write_val < 0) {
						perror("write");
						exit(1);
					}
					// To receive incoming transmit value.
					read_val = read(sock_fd, &transmit, sizeof(int));
					
					// Error checking for if the read system call failed.
					if (read_val < 0) {
						perror("read");
						exit(1);
					}
					if (transmit == 5){
						fprintf(stderr, "transmit to server failed\n");
						exit(1);
					}
                }
				
                if (fileread < 8192) {
                    if (fileread == 0) {
                        break;
                    }

                    if (ferror(fp)) {
                        perror("read");
                    }
                }
				usleep(10000);
			}
			fclose(fp);
		}
    }

    else if (S_ISDIR(my_stat.st_mode) == 1) {

        // To open directory.
        DIR *myDir = opendir(new_src);

        // An error message to stderr if src directory cannot be openned.
        if (myDir == NULL) {
            fprintf(stderr, "'%s' cannot be opened.\n", new_src);
            exit(1);
        }

		
        char* new_dest_path = concatenate_string(new_dest, "/");
		char *new_index_src;
		char* new_dest_path2;
		if (index > 0){
			new_index_src = &new_src[index+1];
			new_dest_path2 = concatenate_string(new_dest_path, new_index_src);
		} else {
			new_dest_path2 = concatenate_string(new_dest_path, new_src);
		}
        strcpy(filestruct->path, new_dest_path2);
        filestruct->mode = my_stat.st_mode;

        write_val = write(sock_fd, filestruct, sizeof(struct fileinfo));

        if (write_val < 0){
            fprintf(stderr, "writing filestruct to server failed\n");
            exit(1);
        }
        read_val = read(sock_fd, &match, sizeof(match));
		
		if (match == 3){
			fprintf(stderr, "MATCH_ERROR found!\n");
			exit(1);
		}

        // Error checking for if read system call failed.
        if (read_val < 0) {
            perror("read");
            exit(1);
        }
		
        // To receive incoming transmit value.
        read_val = read(sock_fd, &transmit, sizeof(int));

        // Error checking for if the read system call failed.
        if (read_val < 0) {
            perror("read");
            exit(1);
        }
		if (transmit == 5){
			fprintf(stderr, "transmit to server failed\n");
			exit(1);
		}
        
        // To iterate through all the files in src.
        while (((my_dirent = readdir(myDir)) != NULL)) {
            // To copy/check if the all the files in src are dest (ignore Symbolic Links).
            if  (my_dirent->d_name[0] != '.') {

                // To create a path for each file in src.
                char* new_src_path = concatenate_string(new_src, "/");
                char* new_src_path2 = concatenate_string(new_src_path, my_dirent->d_name);
				
				char* new_dest_path = concatenate_string(new_dest, "/");
				
				char *new_index_src;
				char* new_dest_path2;
				if (index > 0){
					new_index_src = &new_src_path2[index+1];
					new_dest_path2 = concatenate_string(new_dest_path, new_index_src);
				} else {
					new_dest_path2 = concatenate_string(new_dest_path, new_src_path2);
				}
                
                lstat(new_src_path2, &my_stat);

                if (S_ISDIR(my_stat.st_mode) == 1) {
					
                    client_copy(new_src_path2, new_dest, sock_fd, index); // Save the recursive process num in fcopy.

                }

                else if (S_ISREG(my_stat.st_mode) == 1) {
                    
					// Open the file to transfer.
                    FILE *my_file = fopen(new_src_path2, "rb");
                    
                    if (my_file == NULL){
                        fprintf(stderr, "couldn't open\n");
                        exit(1);
                    }
                    // To get size of the file.
                    fseek(my_file, 0, SEEK_END); // To travel to the end of the file.
                    int size = ftell(my_file); // To get size of file.
                    fseek(my_file, 0, SEEK_SET); // To travel back to start of the file for fread.

                    // To read the file contents.
                    char* contents = malloc(size);
                    fread(contents, sizeof(char), size, my_file);
					
                    // To fill the filestruct.
					
					strcpy(filestruct->hash, hash(my_file));
                    strcpy(filestruct->path, new_dest_path2);
                    filestruct->size = size;
                    filestruct->mode = my_stat.st_mode;
					fclose(my_file);
                    
					// To write the fileinfo to the server.
                    write_val = write(sock_fd, filestruct, sizeof(struct fileinfo));

                    if (write_val < 0){
                        fprintf(stderr, "writing filestruct to server failed\n");
                        exit(1);
                    }
                    
					// To read the match value from the server.
                    read_val = read(sock_fd, &match, sizeof(match));
					
					// Match value 3 indicates a match error has occurred.
					if (match == 3){
						fprintf(stderr, "MATCH_ERROR found!\n");
						exit(1);
					}
					
					// Failed to read the match code from server
                    if (read_val < 0){
                        fprintf(stderr, "reading match code from server failed\n");
                        exit(1);
                    }
					
					// Mismatch on server end has been detected, meaning we need to upload our file to the server.
                    if (match == 1){
                        
						// To open the file to pass it to server.
                        FILE *fp = fopen(new_src_path2, "rb");
						int writtenByte = 0;
                        // To continuously read the file then upload to server.

                        while (1) {
                            
                            // To read from the file in chunks of 8192 bytes
                            unsigned char buf[8192] = {0};
                            int fileread = fread(buf, sizeof(char), 8192, fp);
							writtenByte += fileread;
							
							// To write the data to server if we have data to send.
                            if (fileread > 0) {
                                write_val = write(sock_fd, buf, fileread);
								if (write_val < 0) {
									perror("write");
									exit(1);
								}

								// To receive incoming transmit value.
								read_val = read(sock_fd, &transmit, sizeof(int));

								// Error checking for if the read system call failed.
								if (read_val < 0) {
									perror("read");
									exit(1);
								}
								if (transmit == 5){
									fprintf(stderr, "transmit to server failed\n");
									exit(1);
								}
                            }
							
							// File read is under 8192 that means we are finished writing or encountered an error.
                            if (fileread < 8192) {
                                if (writtenByte == filestruct->size) {
                                    fclose(fp);
                                    break;
                                }
                                if (ferror(fp)) {
                                    perror("read");
                                }
                            }
							
							// To prevent desync.
							usleep(10000);
                        }

                    }   
                }
            }
        }
		if (myDir != NULL) {
            closedir(myDir);
        }
	}
    return 0;
}


/*
 * Contatenates char* string s1 in front of char* string s2 and returns result.
 */
char* concatenate_string(char* s1, char* s2) {

	// To dynamically create memory for new string.
	char* new_string = malloc(strlen(s1) + strlen(s2) + 1);
	strcpy(new_string, s1); // We first copy s1 to put it in front.
	strcat(new_string, s2); // Then we concatenate s2 to put in the back.

	return new_string;
}
