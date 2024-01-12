# Directorul unde se află biblioteca Jansson
JANSSON_DIR = /opt/homebrew/opt/jansson

# Opțiuni pentru compilator
CFLAGS = -I$(JANSSON_DIR)/include 
# Opțiuni pentru linker
LDFLAGS = -L$(JANSSON_DIR)/lib -ljansson

# Ținte implicite
all: server client

server: server.c
	gcc $(CFLAGS) server.c -o server $(LDFLAGS)

client: client.c
	gcc client.c -o client

clean:
	rm -f server client
