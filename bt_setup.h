#ifndef _BT_SETUP_H
#define _BT_SETUP_H

// standard stuff
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include "bt_setup.h"
#include "bt_lib.h"

/**
 * __parse_peer(peer_t *peer, char peer_st) -> void
 *
 * parse a peer string, peer_st and store the parsed result in peer
 *
 * ERRORS: Will exit on various errors
 **/
void usage(FILE *file);

/**
 * pars_args(bt_args_t *bt_args, int argc, char *argv[]) -> void
 *
 * parse the command line arguments to bt_client using getopt and
 * store the result in bt_args.
 *
 * ERRORS: Will exit on various errors
 *
 **/
void parse_args(bt_args_t *bt_args, int argc,  char **argv);

/**
 * parse_torrent_file(bt_args_t *bt_args) -> void
 * 
 * parse *.torrent file to populate values related to the 'info' part of of the torrent file
 * 
 */
void parse_torrent_file(bt_args_t *bt_args, bt_info_t *);

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
void fast_forward(char *, FILE *);

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
void store_forward(char *, FILE *, bt_info_t *);

/**
 * This function checks exactly which digits appear in a bencoded string before a ':' and 
 * accordingly keeps passing each digit to the construct_num() function.
 * 
 * @param char* "the character that has the digit"
 * @param FILE* "pointer to the file being read"
 * 
 * @return int "the fully constructed natural number"
 */
int handle_numbers(char *, FILE *);

/**
 * construct_num(int)
 * 	Constructs a natural number by successively appending each digit to the previous one on successive calls and
 * 	forms a non-single digit natural number. When called only once, will return a single-digit natural number.
 * 
 * @param int "each single digit parsed in file that can be appended to the previous one"
 * 
 * @return int "the entire non-single digit or single digit natural number"
 */
int construct_num(int);

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
void handle_info_contents(char *buf, char *chr, FILE *fpr, bt_info_t *);

#endif
