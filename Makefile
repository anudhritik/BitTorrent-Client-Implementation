CC=gcc
CPFLAGS=-g -Wall
LDFLAGS= -lcrypto

SRC= bt_client.c bt_lib.c bt_setup.c
OBJ=$(SRC:.c=.o)
BIN=bt_client

all: $(BIN)

$(BIN): $(OBJ)
	$(CC) $(CPFLAGS) $(LDFLAGS) $(OBJ) -o $(BIN)

# need to find more info about the line below
%.o:%.c
	$(CC) -c $(CPFLAGS) -o $@ $<

$(SRC):

clean:
	rm -rf $(OBJ) $(BIN)