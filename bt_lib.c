#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <arpa/inet.h>	//internet address library
#include <netdb.h>  // for struct hostent
#include <sys/socket.h>
#include <sys/types.h>

#include <sys/stat.h>
#include <arpa/inet.h>

#include <openssl/sha.h>	// for using SHA1() function for hashing

#include "bt_lib.h"
#include "bt_setup.h"

#define BUF_LEN 1024

/**
 * calc_id() clubs the IP address and port number of peer into a single string. Then, 
 * fills the peer 'id' with a SHA1 digest computed on the clubbed string
 */
void calc_id(char *ip, unsigned short port, char *id) {
    char data[256];		// data for which SHA1 digest is to be made
    int len;			// length of data
    
    /* format print to store up to 256 bytes (characters) including the terminating '\0' (null) character in 'data'.
     * snprintf() returns the index of the char that it places the '\0' at; basically returns the length of the string copied
     */
    len = snprintf(data, 256, "%s%u", ip, port);    // example data = localhost7000

    /* id is just the SHA1 of the ip and port string
     * SHA1()'s signature:
     * unsigned char *SHA1(const unsigned char *dataString, unsigned long dataLength, unsigned char *SHA1digest);
     */
    SHA1( (unsigned char *) data, len, (unsigned char *) id );

    return;
}

/**
 * init_peer(peer_t *peer, int id, char *ip, unsigned short port) -> int
 *
 * initialize the peer_t structure peer with an id, ip address, and a
 * port. Further, it will set up the sockaddr such that a socket
 * connection can be more easily established.
 *
 * Return: 0 on success, negative values on failure. Will exit on bad
 * ip address.
 *     
 **/
int init_peer(peer_t *peer, char *id, char *ip, unsigned short port) {
        
    struct hostent *hostinfo;	// instantiate hostent struct that contains information like IP address, host name, etc.
	
    // set the host id and port for reference
    memcpy(peer->id, id, ID_SIZE);  // SHA1 hash of peer IP & port is stored as peer struct's 'id'
    peer->port = port;
        
    // get the host by name
    if( (hostinfo = gethostbyname(ip)) == NULL ) {
        perror("gethostbyname failure, no such host?");
        herror("gethostbyname");	// prints error message associated with the current host
        exit(1);
    }
    
    // zero out the peer's sock address structure before filling in its details
    bzero(&(peer->sockaddr), sizeof(peer->sockaddr));
            
    // set the family to AF_INET, i.e., Internet Addressing
    peer->sockaddr.sin_family = AF_INET;
	
    // copy the address from h_addr field of 'hostinfo' to the peer's socket address
    bcopy( (char *) (hostinfo->h_addr), 
                (char *) &(peer->sockaddr.sin_addr.s_addr),
                hostinfo->h_length );
        
    // encode the port to network-byte order to store in sockaddr_in struct
    peer->sockaddr.sin_port = htons(port);
    
    return 0;	// success
}

/**
 * print_peer(peer_t *peer) -> void
 *
 * print out debug info of a peer
 *
 **/
void print_peer(peer_t *peer) {
    int i;	// loop iterator

    if (peer) {
		printf("peer: %s:%u ", 
				inet_ntoa(peer->sockaddr.sin_addr),		// converts a network address to a dots-and-numbers format string
				peer->port);
        printf("id: ");
        
		for (i = 0; i < ID_SIZE; i++) {
            printf("%02x", peer->id[i]);	// convert to 40-byte hex string
        }
		
        printf("\n");
    }
}

/*
 * NOTE: example of how one can return address of a local variable by 
 * changing its scope to "static"
 */
unsigned char * get_hashhex(unsigned char str[]) {

    int i;
    static unsigned char ret_hash_hex[40];
    for (i = 0; i < ID_SIZE; i++) {
            sprintf( (char *) &ret_hash_hex[2 * i], "%02x", str[i]);    // convert to 40-byte hex string
    }
    return ret_hash_hex;
}

/**
 * init_seeder() documentation TO DO
 **/
void init_seeder(bt_args_t *bt_args) {
    char *parse_bind_str;   // temporary string
    char *token;
    char delim[] = ":"; // delimiter
    char *ip;   // to store IP addr
    char id[20];    // to store SHA1 hash into
    unsigned short port;    // to store port
    int i;  // loop iterator variable

    // copy bt_args.bind_info to parse_bind_str to avoid losing original bt_args.bind_info
    parse_bind_str = malloc(strlen(bt_args->bind_info) + 1);
    memset( parse_bind_str, 0x00, (strlen(bt_args->bind_info) + 1) );   // zero-out parse_bind_str
    strncpy(parse_bind_str, bt_args->bind_info, strlen(bt_args->bind_info));
    // printf("testing, parse_bind_str: '%s'\n", parse_bind_str);

    for ( token = strtok(parse_bind_str, delim), i = 0; token; token = strtok(NULL, delim), i++ ) {
        switch(i) {
            case 0:
                ip = token;
                break;
            case 1:
                port = atoi(token);
                break;
            default:
                break;
        }
    }

    if (i < 2) {    // too few arguments after '-b' flag
        fprintf(stderr, "ERROR: Not enough values in '%s'\n", bt_args->bind_info);
        usage(stderr);
        exit(1);
    }

    if (token) {    // token should be NULL by this time
        fprintf(stderr, "ERROR: Too many values in '%s'\n", bt_args->bind_info);
        usage(stderr);
        exit(1);
    }

    // calculate SHA1 hash of the concatenation of binding machine's IPaddr and port
    calc_id(ip, port, id);    // calculate bt client's ID
    // printf("testing, inside init_seeder, before putting in bt_args, id without hex: '%s'\n", id);
    memcpy(bt_args->id, id, 20);

    make_seeder_listen(ip, port, bt_args);    /* make seeder listen to incoming leecher connections */
}

/**
 * make_seeder_listen() documentation TO DO
 *
--------------------------sockaddr structures--------------------------------------------
struct sockaddr {
    unsigned short    sa_family;    // address family, AF_xxx
    char              sa_data[14];  // 14 bytes of protocol address
};

// IPv4 AF_INET sockets:

struct sockaddr_in {
    short            sin_family;   // e.g. AF_INET, AF_INET6
    unsigned short   sin_port;     // e.g. htons(3490)
    struct in_addr   sin_addr;     // see struct in_addr, below
    char             sin_zero[8];  // zero this if you want to
};

struct in_addr {
    unsigned long s_addr;          // load with inet_pton()
};
----------------------------------------------------------------------------------------
 * 
 **/
void make_seeder_listen(char *ip, unsigned short port, bt_args_t *bt_args) {

    if (bt_args->verbose) {
        printf("Instantiating seeder...\n");
    }

    struct hostent *hostinfo;   // store network-related information of host machine

    if( !(hostinfo = gethostbyname(ip)) ) { // gethostbyname() returns the 'hostent' structure or NULL on failure
        fprintf(stderr,"ERROR: Invalid host name '%s' specified\n", ip);
        usage(stderr);
        exit(1);
    }

    struct sockaddr_in seeder_addr; // structure containing all network-related seeder information

    // populate seeder_addr structure
    seeder_addr.sin_family = hostinfo->h_addrtype;
    seeder_addr.sin_port = htons(port);
    memmove( (char *) &(seeder_addr.sin_addr.s_addr), (char *) hostinfo->h_addr, hostinfo->h_length );

    int seeder_sock;    // seeder's connection-welcoming socket to its leechers
    int new_seeder_sock;    // new seeder allocated socket to exchange data with leechers
    int seeder_listen_status;  // check whether seeder is listening on its socket or no
    unsigned char buffer[BUF_LEN];   // a buffer of size 1024 bytes at max to read or write data
    ssize_t bytes_read, total_bytes_read = 0;  // bytes read; total number of bytes read by seeder; ssize_t defined in <unistd.h>
    int i;  // loop iterator variable
    int drop_conn = 0;  // flag to indicate peer connection needs to be dropped

    // create seeder's listening TCP stream socket
    if ( (seeder_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0 ) {
        fprintf(stderr, "ERROR: Seeder was unable to set up a listening socket.\n");
        exit(1);
    }

    // bind the seeder's connection-welcoming socket to the specific port number specified in 'peer'
    if ( bind(seeder_sock, (struct sockaddr *) &seeder_addr, sizeof(seeder_addr))  < 0 ) {    // NOTE: operator '->' has precedence over '->' operator
        fprintf(stderr, "ERROR: Seeder encountered error in binding its listening socket.\n");
        exit(1);
    }

    // on successful seeder socket binding, seeder should listen to incoming leecher connections
    if ( (seeder_listen_status = listen(seeder_sock, MAX_CONNECTIONS)) < 0 ) { // set maximum number of incoming leecher connections to 5
        fprintf(stderr, "ERROR: Seeder encountered error while trying to listen to incoming leecher connections.\n");
        exit(1);
    }

    if (bt_args->verbose) {
        printf("SEEDER successfully allocated a listener socket to listen to incoming connections from leecher/s.\n\n");
    }

    printf("SEEDER LISTENING now on peer: '%s:%u'", inet_ntoa(seeder_addr.sin_addr), port);   // print dots-and-numbers version of host & its listening port
    printf("; peer id: %s\n", get_hashhex(bt_args->id));

     // store connecting leecher's information
    struct sockaddr_in leecher_info;    // to fill in all relevant leecher information
    unsigned int leecher_length = sizeof(leecher_info);
    if ( ( new_seeder_sock = accept(seeder_sock, (struct sockaddr *) &leecher_info, &leecher_length) ) < 0 ) {  // seeder sets up new socket to exchange data with leecher
        fprintf(stderr, "ERROR: Seeder could not set up a new socket to communicate with leecher.\n");
        exit(1);
    }

    memset(buffer, 0x00, BUF_LEN);  // zero-out buffer before using it

    while ( (bytes_read = read(new_seeder_sock, buffer, BUF_LEN)) != 0 ) {  /* read data from new seeder socker into buffer
                                                                            * read until amount to be read is 0 i.e. until no more data is available is to read */
        total_bytes_read += bytes_read;
        char hs_seeder[100];
        memset(hs_seeder, 0, 100);
        memcpy(hs_seeder, buffer, BUF_LEN);
        printf("HANDSHAKE INFO received from leecher at SEEDER: '%s'\n", hs_seeder);

        // tokenize 'handshake' contents
        char *token;
        char delim[] = ":";
        unsigned char leecher_peer_id[100];
        memset(leecher_peer_id, 0, 100);
        for ( token = strtok(hs_seeder, delim), i = 0; 
            (token && i < 4); token = strtok(NULL, delim), i++ ) {
            switch (i) {
                case 0: case 1: case 2:
                    break;
                case 3:
                    memcpy(leecher_peer_id, token, 20);
                    break;
            }
        }

        if (bt_args->verbose) {
            printf("\tHASH of CONNECTING LEECHER's peer id: '%s'\n", leecher_peer_id);
        }
        // printf("testing, inside seeder while, length of leecher_peer_id: %ld\n", strlen( (const char *) leecher_peer_id));
        unsigned char hex_peer_id[100];
        memset(hex_peer_id, 0, 100);
        for (i = 0; i < 20; i++ ) {
            sprintf( (char *) &hex_peer_id[2 * i], "%02x", leecher_peer_id[i] );
        }
        if (bt_args->verbose) {
            printf("\tHEX value of CONNECTING LEECHER's peer id: '%s'\n", hex_peer_id);
        }
        unsigned char *hex_btclient_id;
        hex_btclient_id = get_hashhex(bt_args->id);
        if (bt_args->verbose) {
            printf("\tHEX value of BT Client's id: '%s'\n", hex_btclient_id);
        }

        // compare received peer id from leecher with bt client's id for equality
        if ( strcmp( (const char *) hex_peer_id, (const char *) hex_btclient_id) == 0) {
            printf("HANDSHAKE SUCCESS peer: %s port: %u id: %s\n", inet_ntoa(seeder_addr.sin_addr), port, hex_btclient_id);
        } else
            drop_conn = 1;

        memset(buffer, 0x00, BUF_LEN);  // flush-out buffer everytime a cycle of reading from socket is completed

    }

    if (bytes_read < 0) {
        fprintf(stderr, "ERROR: Could not read from seeder socket.\n");
        exit(1);
    }

    if (drop_conn == 1) {   // if connecting leecher's peer id & bt client ID do not match, drop connection
        printf("\tConnecting leecher's peer id & bt client's id do not match, connection dropped.\n");
        goto END;
    }

    // proceed with standard closing seeder sockets
    printf("CLOSING SEEDER SOCKET as no more data available to read.\n");
    END:
        close(new_seeder_sock);
        close(seeder_sock);
}

/**
 * init_leecher() documentation TO DO
 **/
int init_leecher(peer_t *peer) {

    int leecher_sock;   // to create a leecher socket to communicate with seeder
    /*char buffer[BUF_LEN];    // read/write buffer
    ssize_t bytesWritten = 0;    // track number of bytes read or written

    memset(buffer, 0x00, BUF_LEN); // zero-out buffer*/

    // create the TCP stream socket
    if ( ( leecher_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP) ) < 0 ) {   // non-negative socket() return value indicates failure in creating the socket
        fprintf(stderr, "ERROR: TCP leecher socket creation failed.\n");
        exit(1);
    }
    
    // establish connection with leecher by calling connect on the seeder's TCP socket, sockfd
    if ( connect( leecher_sock, (struct sockaddr *) &peer->sockaddr, sizeof(peer->sockaddr) ) < 0 ) {
        fprintf(stderr, "ERROR: Connection could not be established to seeder id: %s\n", get_hashhex(peer->id));
        exit(1);
    }

    printf("CONNECTION ESTABLISHED to PEER: '%s:%u'; peer id: %s\n", inet_ntoa(peer->sockaddr.sin_addr), peer->port, get_hashhex(peer->id));

    return leecher_sock;

/*    // send all communication from leecher to seeder henceforth
    while ( ( bytesWritten = write(leecher_sock, buffer, strlen(buffer)) ) ) {
        printf("testing, you are here!");
    }

    close(leecher_sock);
    printf("CONNECTION CLOSED to PEER: '%s:%u'; peer id: ", inet_ntoa(peer->sockaddr.sin_addr), peer->port);
    get_hashhex(peer->id);
    printf("\n");   // line feed*/
    
}

/**
 * init_handshake(peer_t *) documentation TO DO
 **/
void init_handshake(peer_t *peer, unsigned char *hs, bt_info_t *bt_info) {
    
    printf("HANDSHAKE INIT to peer: %s port: %u; peer id: %s\n", inet_ntoa(peer->sockaddr.sin_addr), peer->port, get_hashhex(peer->id));

    // null handshake array initially
    memset(hs, 0, 100);

    // add 'protocol' information to 'handshake'
    sprintf( (char *) hs, "%c", 19);  // store decimal '19' as first byte
    strncat( (char *) hs, "BitTorrent Protocol", 19);
    strncat( (char *) hs, ":", 1);   // add delimiter
    strcat( (char *) hs, "00000000:"); // next 8 'reserved' bytes set as string containing 8 zeros

    /* need to calculate hash of bt_info->name that was extracted from .torrent file
     * fill that hash string into 'handshake' */
    char info_hash[20]; // to store hash of bt_info->name
    memset(info_hash, 0, 20); // null hash string initially
    SHA1( (unsigned char *) bt_info->name, strlen(bt_info->name), (unsigned char *) info_hash );   // copy hash to 'handshake'
    // printf("testing, inside init_handshake, info_hash: '%s'\n", info_hash);
    // printf("testing, inside init_handshake, length of info_hash: '%ld'\n", strlen(info_hash));
    memcpy( (hs + 30), info_hash, 20);
    strncat( (char *) hs, ":", 1);
    // printf("testing, inside init_handshake, strlen(info_hash): %ld\n", strlen(info_hash));
    // printf("testing, inside init_handshake, hs: '%s'\n", hs);

    // store connecting peer's id (hash of 20-bytes) into handshake
    /*printf("testing, inside init_handshake, peer->id without hex: '%s'\n", peer->id);
    printf("testing, inside init_handshake, peer->id in hex: ");
    int i;
    for (i = 0; i < 20; i++ ) {
        printf("%02x", peer->id[i]);
    }
    printf("\n");*/
    memcpy( (hs + 51), peer->id, 20);
    // printf("testing, inside init_handshake, hs: '%s'\n", hs);

    /************** handshake structure construction completed *****************/

}

void create_bitfield(bt_args_t *bt_args, bt_info_t *bt_info) {

    int i;  // loop iterator variable

    FILE *fp = fopen(bt_info->name, "rb"); // say open file 'download.mp3'
    if (!fp) {
        fprintf(stderr, "ERROR: Could not open target torrent file: %s\n", bt_info->name);
        exit(1);
    }

    long int file_offset = bt_info->piece_length;
    fseek(fp, 0, SEEK_END); // move file pointer to the end
    long int f_end = ftell(fp);
    rewind(fp); // move file pointer back to start
    unsigned char *file_buffer = malloc( bt_info->piece_length * sizeof(char) + 1); // 1 extra byte for null-character
    unsigned char piece_hash[bt_info->num_pieces][20];
    unsigned char *piece_hex_hash = malloc(40);

    // null all variables initially
    memset(file_buffer, 0x00, bt_info->piece_length);
    memset(piece_hex_hash, 0x00, 40);

    // set bitfield size only as much as the number of pieces the file is divided into
    bt_args->bitfield->size = bt_info->num_pieces;
    // printf("testing, bt_args->bitfield->size: '%ld\n'", bt_args->bitfield->size);
    bt_args->bitfield->bits = malloc(bt_info->num_pieces * sizeof(char));

    if (bt_args->verbose) {
        printf("Comparing hex values of pieces on record from '%s' with those calculated by splitting actual file...\n", bt_args->torrent_file);
    }
    for (i = 0; i < (int) bt_args->bitfield->size; i++) { // set each bitfield one by one
        /* break file into known number of pieces;
         * read each file piece and create hash of the piece */
        fread(file_buffer, sizeof(char), file_offset, fp);
        SHA1( file_buffer, file_offset, piece_hash[i]);
        piece_hex_hash = get_hashhex(piece_hash[i]);

        if (bt_args->verbose) {
            printf("Hex of piece_hash[%d]: '%s'\n", i, piece_hex_hash);
            printf("Hex of bt_info->piece_hashes[%d]: '%s'\n", i, bt_info->piece_hashes[i]);
        }

        if ( memcmp(piece_hex_hash, bt_info->piece_hashes[i], 40) == 0 ) {
            bt_args->bitfield->bits[i] = '1';
        } else {
            bt_args->bitfield->bits[i] = '0';
        }

        if ( i == ((int) bt_args->bitfield->size - 2) ) {
            file_offset = f_end - ftell(fp);
        }
    }
    bt_args->bitfield->bits[i] = '\0';  // null-termination
    // printf("testing, bt_args->bitfield->bits: '%s'\n", bt_args->bitfield->bits);
}