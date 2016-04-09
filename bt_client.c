
// standard libraries
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

// libraries for networking stuff
#include <netinet/in.h>				// for struct sockaddr_in
#include <netinet/ip.h>				// ip header library (must come before ip_icmp.h)
#include <netinet/ip_icmp.h> 		// icmp header
#include <arpa/inet.h>				// internet address library
#include <netdb.h>					// for hostent struct
#include <sys/socket.h>				// for socket operations
#include <sys/types.h>
#include <signal.h>

#include "bt_lib.h"
#include "bt_setup.h"

int main (int argc, char * argv[]) {

    bt_args_t bt_args; // structure to capture command-line arguments
    int i;	// loop iterator
    int leecher_sock;   // leecher's connection socket

    parse_args(&bt_args, argc, argv);

    if (bt_args.verbose) {	// if verbose mode is requested
        printf("Args information from command line:\n");
        printf("\tverbose: %d\n", bt_args.verbose);
        printf("\tsave_file: %s\n", bt_args.save_file);	// display name of file to save to
        printf("\tlog_file: %s\n", bt_args.log_file);		// display name of file to log information to
        printf("\ttorrent_file: %s\n", bt_args.torrent_file);	// metainfo or torrent file being used by bt client

        // print information of all peers
        /*for (i = 0; i < MAX_CONNECTIONS; i++) {
            if(bt_args.peers[i] != NULL)
                print_peer(bt_args.peers[i]);
            else
                break;
        }*/
    }

    // instantiate & allocate memory to bt_info_t struct that's inside bt_args
    bt_info_t *bt_info = (bt_info_t *) malloc(sizeof(bt_info_t));

    // parse the torrent file to fill up contents of the bt_info structure with required information from the 'info' dictionary in .torrent file
    parse_torrent_file(&bt_args, bt_info);

    if (bt_args.bind == 1) {    // bt client runs in seeder mode

        /* separate IPaddr:port from string following '-b'; generate bt client's ID;
         * get a handle on the seeder's data-exchange socket;
         * make seeder listen for incoming leecher connections */
        init_seeder(&bt_args);
        create_bitfield(&bt_args, bt_info);
        printf("BITFIELD at SEEDER: '%s'\n", bt_args.bitfield->bits);
        // printf("testing, bt_args.bitfield->bits: '%s'\n", bt_args.bitfield->bits);
        // printf("testing, inside main, bt_args.bitfield->size: %ld\n", bt_args.bitfield->size);

    } else {    // bt client runs in leecher mode
        for (i = 0; i < MAX_CONNECTIONS; i++) {
            if (bt_args.peers[i] != NULL) {
                // write all client (leecher) code here

                if (bt_args.verbose) {
                    printf("Creating a leecher socket...\n");
                }
                leecher_sock = init_leecher(bt_args.peers[i]); // run a leecher instance for each peer recorded in bt_args->peers[]
                unsigned char *handshake = malloc(100);   // handshake information to be exchanged between peers
                init_handshake(bt_args.peers[i], handshake, bt_info);
                
                // send handshake over to seeder
                ssize_t bytes_written;
                if ( (bytes_written = write(leecher_sock, handshake, 100)) < 0 ) {
                    fprintf(stderr, "ERROR: Could not write to leecher socket.\n");
                    exit(1);
                }

            } else
                break;
        }

    }

    // main client loop
    // printf("Starting Main Loop\n");
    /*
	while(1){

        // try to accept incoming connection from new peer
             
        // poll current peers for incoming traffic
        // write pieces to files
        // update peers' choke or unchoke status
        // responses to have/havenots/interested etc.
        
        // for peers that are not choked
        //     request pieces from outcoming traffic

        // check livelness of peers and replace dead (or useless) peers
        // with new potentially useful peers
        
        // update peers, 

    }
	*/
	
    return 0;
}
