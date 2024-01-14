# Directory where Jansson library is located
JANSSON_DIR = /opt/homebrew/opt/jansson

# Compiler options for server
SERVER_CFLAGS = -I$(JANSSON_DIR)/include -Wall

# Linker options for server
SERVER_LDFLAGS = -L$(JANSSON_DIR)/lib -ljansson -w

# Compiler options for client
CLIENT_CFLAGS = $(shell pkg-config --cflags gtk+-3.0) -Wall

# Linker options for client
CLIENT_LDFLAGS = $(shell pkg-config --libs gtk+-3.0) -lncurses -w

# Targets
all: server client

server: server.c
	gcc $(SERVER_CFLAGS) $^ -o $@ $(SERVER_LDFLAGS)

client: client.c
	gcc $(CLIENT_CFLAGS) $^ -o $@ $(CLIENT_LDFLAGS)

clean:
	rm -f server client
