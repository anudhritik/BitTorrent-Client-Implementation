
#ifndef _BT_LIB_H
#define _BT_LIB_H

//standard stuff
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include <poll.h>

//networking stuff
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h> 

#include "bt_lib.h"

/* Maximum file name size, to make things easy */
#define FILE_NAME_MAX 1024

/* Maximum number of connections */
#define MAX_CONNECTIONS 5

/* initial port to try and open a listen socket on */
#define INIT_PORT 6667 

/* max port to try and open a listen socket on */
#define MAX_PORT 6699

/* Different BitTorrent Message Types */
#define BT_CHOKE 0
#define BT_UNCHOKE 1
#define BT_INTERESTED 2
#define BT_NOT_INTERESTED 3
#define BT_HAVE 4
#define BT_BITFILED 5
#define BT_REQUEST 6
#define BT_PIECE 7
#define BT_CANCEL 8

/* size (in bytes) of id field for peers (20-byte SHA1 digest denoting peer ID) */
#define ID_SIZE 20

/**
 * Message structures
 */
typedef struct {
    char *bits; //bitfield where each bit represents a piece that the peer has or doesn't have
    size_t size; //size of the bitfield
} bt_bitfield_t;

typedef struct{
    int index; //which piece index
    int begin; //offset within piece
    int length; //amount wanted, within a power of two
} bt_request_t;

typedef struct{
    int index; //which piece index
    int begin; //offset within piece
    char piece[0]; //pointer to start of the data for a piece
} bt_piece_t;

typedef struct bt_msg {
    int length; // length of remaining message, 0 length message is a keep-alive message
    unsigned int bt_type; // type of bt_mesage

    // payload can be any of these
    union { 
        bt_bitfield_t bitfield; // send a bitfield
        int have; // what piece you have
        bt_piece_t piece; // a piece message
        bt_request_t request; // request messge
        bt_request_t cancel; // cancel message, same type as request
        char data[0]; // pointer to start of payload, just incase   
    } payload;

} bt_msg_t;

// holds information about a peer
typedef struct peer {
    unsigned char id[ID_SIZE];  // the peer id (SHA1 hash of peer IP & port)
    unsigned short port;    // the port to connect
    struct sockaddr_in sockaddr;    // sockaddr for peer
    int peer_sock;  // socket used for connections
    int choked; // peer choked?
    int interested; // peer interested?
} peer_t;

/* holds information about a torrent file
 * parse .torrent file to fill contents of bt_info_t structure
 */
typedef struct {
    char name[FILE_NAME_MAX];   // suggested name for saving the file, grab this from the 'name' field in the 'info' dictionary in .torrent file
    int piece_length;   // number of bytes in each piece
    int length; // length of the file to be downloaded in bytes
    int num_pieces; //number of pieces, computed based on above two values
    unsigned char **piece_hashes;    // pointer to 20 byte data buffers containing the sha1sum of each of the pieces
} bt_info_t;

// holds all the arguments and state information for running the bt client
typedef struct {
    int verbose; // verbose level
    int bind;   // to indicate whether bt_client needs to bind to seeder
    char bind_info[256];    // stores "IPaddr:port" string entered after '-b' flag
    char save_file[FILE_NAME_MAX]; // the file that seeder has
    bt_bitfield_t *bitfield;    // to store bitfield for torrent file in swarm
    FILE *f_save;
    char log_file[FILE_NAME_MAX]; //thise log file
    char torrent_file[FILE_NAME_MAX]; // *.torrent file
    peer_t *peers[MAX_CONNECTIONS]; // array of peer_t pointers (i.e., array of seeders)
    unsigned char id[ID_SIZE];  // this bt_client's id
    int sockets[MAX_CONNECTIONS]; // Array of possible sockets
    struct pollfd poll_sockets[MAX_CONNECTIONS]; /* Array of pollfd for polling for input
                          * struct pollfd {
                          * int fd;         // file descriptor
                          * short events;   // requested events (bitmasks indicating for what events fd must be watched)
                          * short revents;  // returned events (bitmasks)
                          * };
                          */
    /* set once torrent is parsed */
    bt_info_t *bt_info; // the parsed info for this torrent
} bt_args_t;

/**
 * get_hashhex() documentation TO DO
 **/
unsigned char * get_hashhex(unsigned char *);

/**
 * init_seeder() documentation TO DO
 **/
void init_seeder(bt_args_t *);

/**
 * init_leecher(peer_t *, bt_args_t) documentation TO DO
 **/
int init_leecher(peer_t *);


/**
 * make_seeder_listen() documentation TO DO
 **/
void make_seeder_listen(char *, unsigned short, bt_args_t *);

/**
 * init_handshake() documentation TO DO
 **/
void init_handshake(peer_t *, unsigned char *, bt_info_t *);

/* choose a random id for this node */
unsigned int select_id();

/**
 * propogate a peer_t struct and add it to the bt_args structure
 **/
int add_peer(peer_t *peer, bt_args_t *bt_args, char *hostname, unsigned short port);

/* drop an unresponsive or failed peer from the bt_args */
int drop_peer(peer_t *peer, bt_args_t *bt_args);

/* initialize connection with peers */
int init_peer(peer_t *peer, char *id, char *ip, unsigned short port);

/* calc the peer id based on the string representation of the ip and port */
void calc_id(char *ip, unsigned short port, char *id);

/* print info about this peer */
void print_peer(peer_t *peer);

/* check status on peers, maybe they went offline? */
int check_peer(peer_t *peer);

/* check if peers want to send me something */
int poll_peers(bt_args_t *bt_args);

/* send a msg to a peer */
int send_to_peer(peer_t *peer, bt_msg_t *msg);

/* read a msg from a peer and store it in msg */
int read_from_peer(peer_t *peer, bt_msg_t *msg);

/* save a piece of the file */
int save_piece(bt_args_t *bt_args, bt_piece_t *piece);

/* load a piece of the file into piece */
int load_piece(bt_args_t *bt_args, bt_piece_t *piece);

/* peers know which file pieces others have through a bitfield */
void create_bitfield(bt_args_t *, bt_info_t *);

/* load the bitfield into bitfield */
int get_bitfield(bt_args_t *bt_args, bt_bitfield_t *bitfield);

/* compute the sha1sum for a piece, store result in hash */
int sha1_piece(bt_args_t *bt_args, bt_piece_t *piece, unsigned char *hash);

/* Contact the tracker and update bt_args with info learned, such as peer list */
int contact_tracker(bt_args_t *bt_args);

#endif
