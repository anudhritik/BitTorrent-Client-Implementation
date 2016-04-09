
// standard libraries
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>			// for getopt(), optarg, optind
#include <string.h>
#include <openssl/sha.h>

#include "bt_setup.h"
#include "bt_lib.h"

/**
 * a helper variable to the construct_num() function
 */
static int final_num;

/**
 * usage(FILE *file) -> void
 *
 * print the usage of this program to the file stream file
 *
 **/
void usage(FILE *file) {
    if(file == NULL) {
        file = stdout;
    }

    fprintf(file,
                    "bt-client [OPTIONS] file.torrent\n"
                    "    -h 			\t Print this help screen\n"
                    "    -b ip:port 		\t Bind to this ip:port for incoming connections \n"
                    "    -s save_file   	\t Save the torrent in directory save_dir (dflt: .)\n"
                    "    -l log_file    	\t Save logs to log_file (dflt: bt-client.log)\n"
                    "    -p ip:port 		\t Instead of contacing the tracker for a peer list,\n"
                    "                           \t use this peer instead, ip:port (ip or hostname)\n"
                    "                           \t (include multiple -p for more than 1 peer)\n"
                    "    -I id 		\t Set the node identifier to id (dflt: random)\n"
                    "    -v                     \t verbose, print additional verbose info\n");
}

/**
 * __parse_peer(peer_t * peer, char peer_st) -> void
 *
 * parse a peer string, peer_st and store the parsed result in peer
 *
 * ERRORS: Will exit on various errors
 **/
void __parse_peer(peer_t *peer, char *peer_st) {
    char *parse_str;    // string in the form of (IPaddr:port) written as command-line argument after -p
    char *word;     // token grabber variable used with string tokenizer: strtok()
    unsigned short port;    // connection port of peer
    char *ip;   // IP address or hostname of peer
    char id[20];
    char sep[] = ":";   // delimiter separating IP address and port of peer
    int i;  // loop iterator variable

    //need to copy because strtok mangels things
    parse_str = malloc(strlen(peer_st) + 1);
    strncpy( parse_str, peer_st, (strlen(peer_st) + 1) );

    // can only have 2 tokens max, but may have less
    for(word = strtok(parse_str, sep), i = 0; 
            (word && i < 3); 
            word = strtok(NULL, sep), i++) {

        printf("%d:%s\n", i, word);
        switch(i) {
            case 0: // ip or hostname of peer
                ip = word;
                break;
            case 1: // port of peer
                port = atoi(word);
            default:
                break;
        }
    }

    if (i < 2) {    // sanity check for format, "IPaddr:port" OK but "IPaddr" or "" after -p NOT OK
        fprintf(stderr,"ERROR: Parsing Peer: Not enough values in '%s'\n", peer_st);
        usage(stderr);
        exit(1);
    }

    if(word) {
        fprintf(stderr, "ERROR: Parsing Peer: Too many values in '%s'\n", peer_st);
        usage(stderr);
        exit(1);
    }

    // calculate the id (i.e. SHA1 digest), where a 20-byte value is placed in 'id' that denotes each peer id

    calc_id(ip, port, id);

    // build the peer object
    init_peer(peer, id, ip, port);
    
    // free extra memory
    free(parse_str);

    return;
}

/**
 * parse_args(bt_args_t *bt_args, int argc, char *argv[]) -> void
 *
 * parse the command line arguments to bt_client using getopt and
 * store the result in bt_args.
 *
 * ERRORS: Will exit on various errors
 *
 **/
void parse_args(bt_args_t *bt_args, int argc, char *argv[]) {
    int ch;	// ch for each flag
    int n_peers = 0;	// track number of seeders in torrent swarm; peers is synonymous to seeders
    int i;	// loop iterator variable

    /* set the default args */
    bt_args->verbose = 0; // no verbosity
    bt_args->bind = 0;	// NOT in seeder bind mode by default
    
    // null bt_args members: bind_info, save_file, log_file, torrent_file
    memset( bt_args->bind_info, 0x00, sizeof(bt_args->bind_info) );
    memset( bt_args->save_file, 0x00, FILE_NAME_MAX);
    memset( bt_args->torrent_file, 0x00, FILE_NAME_MAX);
    memset( bt_args->log_file, 0x00, FILE_NAME_MAX);

    // null out file pointers
    bt_args->f_save = NULL;

    // null bt_info pointer; should be set once torrent file is read
    bt_args->bt_info = NULL;

    //default log file
    strncpy( bt_args->log_file, "bt_client.log", FILE_NAME_MAX );

    for(i = 0; i < MAX_CONNECTIONS; i++) {
        bt_args->peers[i] = NULL; // set all peers NULL initially
    }

    memset(bt_args->id, 0x00, ID_SIZE);	// set bt_client's id to 0
    
    while ((ch = getopt(argc, argv, "hb:p:s:l:vI:")) != -1) {	// getopt() returns -1 after all command line arguments are parsed
        switch (ch) {
			case 'h':	// help 
				usage(stdout);
				exit(0);
				break;
			case 'v':	// verbose
				bt_args->verbose = 1;
				break;
			case 's':	// the file that seeder has
				strncpy( bt_args->save_file, optarg, FILE_NAME_MAX );
				break;
				if (bt_args->verbose) {
					printf("Peers will download file name: '%s'.\n", bt_args->save_file);
				}
			case 'l':	//log file
				strncpy( bt_args->log_file, optarg, FILE_NAME_MAX );
				break;
			case 'b':	// bt client is in seeder mode; set up seeder
				bt_args->bind = 1;	// seeder flag ON
				snprintf(bt_args->bind_info, 256, "%s", optarg);	// copy "IPaddr:port" string into bt_args struct
				break;
			case 'p':	// bt client is in leecher mode
				n_peers++;	// increment number of peers in torrent swarm

				if ( n_peers > MAX_CONNECTIONS ) {	// cannot have more than MAX_CONNECTIONS number of peers
					fprintf(stderr, "ERROR: Can only support %d seeders.\n", MAX_CONNECTIONS);
					usage(stderr);
					exit(1);
				}

				/* construct peer; add peer to the torrent swarm */
				bt_args->peers[n_peers - 1] = malloc(sizeof(peer_t));
				__parse_peer( bt_args->peers[n_peers - 1], optarg );	// parse seeder information
				if (bt_args->verbose) {
					printf("Peer #%d added to swarm.\n", n_peers);
				}
				break;
			/*case 'I':
				strcpy(bt_args->id, optarg);
				break;*/
			default:
				fprintf(stderr, "ERROR: Unknown option '-%c'\n", ch);
				usage(stdout);
				exit(1);
		}
	}

    argc -= optind;
    argv += optind;

    if(argc == 0){
        fprintf(stderr,"ERROR: Remember we need a torrent file? Please try again.\n");
        usage(stderr);
        exit(1);
    }

    // copy torrent file over
    strncpy( bt_args->torrent_file, argv[0], FILE_NAME_MAX );
    if (bt_args->verbose) {
		printf("Information about file download in torrent file: '%s'\n", bt_args->torrent_file);
	}

    return;
}

/**
 * parse_torrent_file(bt_args_t *bt_args) -> void
 * 
 * parse *.torrent file to populate values related to the 'info' part of of the torrent file
 *
 * @param bt_args_t* "the structure that stores all command line arguments passed by user"
 * 
 * @return void
 */
void parse_torrent_file(bt_args_t *bt_args, bt_info_t *bt_info) {
	/*
	 * bt_args->torrent_file	// .torrent file to read from
	 * bt_args->save_file	// the filename to save to ('name' in .torrent file)
	 * bt_args->bt_info->piece_length	// size of a piece for the file in bytes (power of 2); ('piece length' in .torrent file)
	 * bt_args->bt_info->length	// length of file to be downloaded ('length' in .torrent file)
	 * bt_args->bt_info->num_pieces	// number of pieces file is divided into ()
	 * bt_args->bt_info->piece_hashes	// array of char arrays (20 bytes each) representing each SHA1 hashed piece of file 
						// ('pieces' in torrent file) */
	
	if (bt_args->verbose) {
		printf("PARSING metainfo file: '%s' ...\n", bt_args->torrent_file);
	}
	FILE *fp = fopen( bt_args->torrent_file, "r" );	// open .torrent file specified by user in read only mode
	if (fp == NULL) {
		fprintf(stderr, "ERROR: Could not read file: '%s'\n", bt_args->torrent_file);
		exit(1);
	}
	
	char ch;
	while ( (ch = fgetc(fp)) != EOF ) {	// read file up to the end
	
	    if (ch == 'd') {	/* if 'd' is found in the file, we need to check each 'key' from then on to look for the key: 'info' coz it 
				 * is the 'info' dictionary that contains all data relevant to us.
				 * every dictionary has an 'e' as ending indicator corresponding to its character "begin" indicator 'd' */
	    
		// a fair assumption made here is that a 'dictionary' can have no other but only a 'string' in its 'key' places
	    
			// look for 'key': "info"
			while ( (ch = fgetc(fp)) != 'e') {	// as long as the dictionary does not end

		    	if (ch == 'i') {	// integer value found
		    		while ( (ch = fgetc(fp)) != 'e' )
		    			continue;
					ch = fgetc(fp);	// store character next to where integer ends so as to pass a 'number' value to next store_forward() call
				}

				store_forward(&ch, fp, bt_info);

			}

	    }
	    
	    /* the file could begin with a string (e.g. 4:spam), integer (e.g. i3e) or a list (e.g. l8:pleasant5:smilee i.e. 
	    * ["pleasant", "smile"]) whatever it is, and it will not matter to us coz we should only be alarmed at the sight of another 
	    * 'dictionary' hoping that it is the 'info' dictionary.
	    */
	
	    // CASE: if file finds an "integer"not inside a dictionary, just get past the integer coz it is insignificant to us
	    if (ch == 'i') {
	    // the next 'e' will indicate the end of the integer
		while ( (ch = fgetc(fp)) != 'e') {
		    continue;	// traverse through integer's digits
		}
	    }
		    
	    // CASE: if file finds a "list", just get past that list too (list begins with character 'l')
	    if (ch == 'l') {
	    
		while ( (ch = fgetc(fp)) != 'e') {	// while the list does not end,
		    fast_forward(&ch, fp);		
		}
	    
	    }
	
	    // CASE: if file starts with "string", just get past that string too (string begins with a 'number')
	    fast_forward(&ch, fp);	/* move on in the file
					 * NOTE: will not fast-forward in file if a "number" is not found; 
					 * 'offset' is set to 0 in fast_forward() for such a case
					 */
		}

	rewind(fp);	// set file pointer to beginning of file
	if (bt_args->verbose) {
		printf("\nPARSING of '%s' file complete.\n", bt_args->torrent_file);
	}
	fclose(fp);	// close file after reading from it
}

/**
 * fast_forward(char *, FILE *)
 * 	Used to simply get past the bytes following a ':'. Getting past the bytes or characters means that we do not 
 * 	need to store any information from the file and therefore, are just moving ahead in the file.
 * 
 * @param char* "the character in the file that was last read"
 * @param FILE* "pointer to the file being read"
 * 
 * @return void
 */
void fast_forward(char *c, FILE *fptr) {
    int num = 0;	/* temporary integer holder; 
			 * num = 0 is also useful when offset needs to be 0 (does not fast-forward) */
    
    final_num = 0;	// reset static variable
    
    num = handle_numbers(c, fptr);
    
    fseek(fptr, num, SEEK_CUR);	// move file pointer ahead by length of string (num) after ':'
    
}

/**
 * This function checks exactly which digits appear in a bencoded string before a ':' and 
 * accordingly keeps passing each digit to the construct_num() function.
 * @param char* "the character that has the digit"
 * @param FILE* "pointer to the file being read"
 * 
 * @return int "the fully constructed natural number"
 */
int handle_numbers(char *chr, FILE *fpr) {
    int num = 0;
    switch (*chr) {
	case '0': case '1': case '2': case '3': case '4':
	case '5': case '6': case '7': case '8': case '9':
	    num = atoi(chr);	// store first digit
	    num = construct_num(num);	// construct a natural number
	    while ((*chr = fgetc(fpr)) != ':') {
			num = atoi(chr);	// more than one digit found
			num = construct_num(num);
	    }
	default:
	    break;
    }
    
    return num;
}

/**
 * store_forward(char *, FILE *)
 * 	This function is just like fast_forward() except that instead of moving the file pointer ahead without storing any 
 * file contents, store_forward() stores strings found in the 'info' dictionary into a temporary buffer.
 * 
 * @param char* "the character last read in file"
 * @param FILE* "pointer to the file being read"
 * 
 * @return void
 */
void store_forward(char *c, FILE *fptr, bt_info_t *bt_info) {
    
    int num = 0;
    final_num = 0;	// reset static variable
    char buffer[1024];	// temporary string holder
    switch(*c) {
	case '4':	// if we get a '4', we hope to see an "info" string
	    num = atoi(c);
	    num = construct_num(num);
	    while ((*c = fgetc(fptr)) != ':') {
			num = atoi(c);	// more than one digit found; no chance of "info" being in there
			num = construct_num(num);	// still need to construct that number to fast-forward without storing anythingd
	    }
	    
	    if (num == 4) {	/* if number is indeed the single-digit '4' and not something like '472', then
				 * check if 'key' is equal to "info" */
	    	int i;
			memset(buffer, 0, sizeof(buffer));	// zero-out buffer before anything is read into it
			for (i = 0; i < num; i++) {	// store string of length 4, into a buffer
				*c = fgetc(fptr);
				buffer[i] = *c;
			}
		
			if (strcmp(buffer, "info") == 0) {	// 'buffer' contents equal to "info"?

		    	if ( (*c = fgetc(fptr)) != 'd') {	// but if 'info' is not followed by a dictionary ('d')
					// may be what follows is an integer, a list or a string; handle all such cases
		    		switch(*c) {
			    		case 'l':	// list encountered
			    			while ( (*c = fgetc(fptr) != 'e') )
			    				continue;
			    			break;
			    		case 'i':	// integer encountered
			    			while ( (*c = fgetc(fptr) != 'e') )
			    				continue;
			    			break;
			    		default:
							num = handle_numbers(c, fptr);	// string encountered
							break;
					}
				} else {	// case where a dictionary follows after 'info'

					while ( (*c = fgetc(fptr)) != 'e' ) {	// read through the whole 'info' dictionary until it ends

						final_num = 0;
						num = handle_numbers(c, fptr);
			    		memset(buffer, 0, sizeof(buffer));	// flush buffer
			    
			    		for (i = 0; i < num; i++) {	// store each dictionary 'key'
			    			*c = fgetc(fptr);
			    			buffer[i] = *c;
						}

			    		handle_info_contents(buffer, c, fptr, bt_info);	// check value of each dictionary 'key' to set values in bt_info structure accordingly
					}
			
					num = 0;	// set num to 0, as we do not need a file offset in this case
				}
			}
	    }
	    break;
	case '0': case '1': case '2': case '3': 
	case '5': case '6': case '7': case '8': case '9':	// handle other irrelevant cases
	    num = handle_numbers(c, fptr);
	    break;
	default:
	    break;
    }
    
    fseek(fptr, num, SEEK_CUR);	// offset file pointer ahead
}

/**
 * This function takes a string buffer as argument and checks whether buffer might have data that is of relevance to us as per the 'info' dictionary.
 * It grabs all relevant data and populates the bt_info structure.
 *
 * @param char* buf "the temporary buffer that holds the relevant 'info' keys
 * @param char* chr "last character read from file"
 * @param FILE* fpr "pointer to file"
 *
 * @return void
 */
void handle_info_contents(char *buf, char *chr, FILE *fpr, bt_info_t *bt_info) {

    char holder[1024];	// another temporary string-holder
    int number = 0;
    final_num = 0;	// reset static 'final_num'
    int i;	// loop iterator variable

    if ( strcmp(buf, "length") == 0 ) {	// look to store length of file in bt_args's bt_info structure
	
    	if ( (*chr = fgetc(fpr)) == 'i' ) {	// look for integer value only

    		while ( (*chr = fgetc(fpr)) != 'e' ) {
    			number = atoi(chr);
    			number = construct_num(number);
    		}
    		bt_info->length = number;
    		printf("\tSize or length of file to be downloaded: %d bytes\n", bt_info->length);

    	} else  {
    		fprintf(stderr, "Unexpected value for 'length' of file found in .torrent file");
    		exit(1);
    	}

    } else if ( strcmp(buf, "name") == 0 ) {	// look to store suggested name for storing the torrent file

		*chr = fgetc(fpr);	// push to next character after buffer read
		number = handle_numbers(chr, fpr);

		memset(holder, 0x00, 1024);	// zero-out string-holder
		for (i = 0; i < number; i++) {
			*chr = fgetc(fpr);
			holder[i] = *chr;
		}
		// holder[number] = '\0';	// null-terminate the string; NO NEED TO DO THIS

		memset(bt_info->name, 0x00, FILE_NAME_MAX);	// null 'name' char array initially
		memcpy( bt_info->name, holder, number );	// use memcpy() instead of strncpy() so as to include 'null-terminators'in strings as well
		printf("\tSuggested filename to save torrent as: '%s'\n", bt_info->name);

    } else if ( strcmp(buf, "piece length") == 0 ) {	// look to store size (in bytes) of a piece of the torrent file
	
    	if ( (*chr = fgetc(fpr)) == 'i' ) {	// look for integer only
    		while ( (*chr = fgetc(fpr)) != 'e' ) {
    			number = atoi(chr);
    			number = construct_num(number);
    		}

		    // need to check if 'number' is indeed a power of 2
    		int tempNumber = number;
    		while ( (tempNumber % 2) == 0 ) {
    			tempNumber = tempNumber / 2;
    		}
    		if (tempNumber != 1) {
    			fprintf(stderr, "ERROR: 'Piece length' in .torrent file should be a power of 2");
    			exit(1);
    		}

    		bt_info->piece_length = number;
    		printf("\tSize of a piece of the file: %d bytes\n", bt_info->piece_length);

    	} else  {
    		fprintf(stderr, "Unexpected value for 'piece length' found in .torrent file");
    		exit(1);
    	}
	
    } else if ( strcmp(buf, "pieces") == 0 ) {	// look to store the hash strings corresponding to each piece of file
	
		*chr = fgetc(fpr);	// push to next character after buffer read
		number = handle_numbers(chr, fpr);	// total SHA1 bytes for all pieces together
	
		if ((number % 20) != 0 ) {	// check whether hash length (bytes) is a multiple of 20
			fprintf(stderr, "ERROR: SHA1 hash length is not a multiple of 20.");
			exit(1);
		}
		
		// printf("testing, number: %d\n", number);

		bt_info->num_pieces = (number / 20);	// total number of 'pieces' of file
		printf("\tNumber of pieces the file is to be divided into: %d\n", bt_info->num_pieces);
	
		memset(holder, 0x00, 1024);	// zero-out string-holder
		
		fread(holder, sizeof(char), number, fpr);	// read a chunk of 'number' bytes from .torrent file
		// holder[number] = '\0';	// explicitly null-terminate temporary string holder again

		bt_info->piece_hashes = malloc( bt_info->num_pieces * sizeof(char *) );	// allocate memory to 'pointer to pointer' (array of char arrays)

		unsigned char tempString[20];
		int j;
		printf("\n");	// line-feed
		for (i = 0; i < bt_info->num_pieces; i++) {
			bt_info->piece_hashes[i] = malloc(41);	// 40 + 1 extra byte for null-character
			memset(bt_info->piece_hashes[i], 0x00, 41);	// null each hash piece initially

			memset(tempString, 0x00, 20);
			j = 0;
			while (j < 20) {
				tempString[j] = (unsigned char) holder[j + 20 * i];
				j++;
			}

			j = 0;
			while (j < 20) {
				snprintf( (char *) &(bt_info->piece_hashes[i][j * 2]), 41, "%02x", tempString[j]);
				j++;
			}

			printf("\t40-byte hex for piece #%d, hash_piece[%d]: %s\n", (i + 1), i, bt_info->piece_hashes[i]);
		}
	
    } else {
		fprintf(stderr, "ERROR: Bad .torrent file. Please check it.");
		exit(1);
    }
}

/**
 * construct_num(int)
 * 	Constructs a natural number by successively appending each digit to the previous one on successive calls and
 * 	forms a non-single digit natural number. When called only once, will return a single-digit natural number.
 * 
 * @param int "each single digit parsed in file that can be appended to the previous one"
 * 
 * @return int "the entire non-single digit or single digit natural number"
 */
int construct_num(int n) {
    return ( final_num = (final_num * 10 + n) );
}