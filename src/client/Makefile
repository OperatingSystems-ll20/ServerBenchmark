INCLUDE_DIR = ../../include/client
BIN_DIR = ../../bin

EXEC_NAME = client
CC = gcc

all: directory client 
LINK_LIBS = -lpthread

directory:
		@mkdir -p $(BIN_DIR)

client:
		$(CC) -I $(INCLUDE_DIR) $(EXEC_NAME).c -o $(BIN_DIR)/$(EXEC_NAME) $(LINK_LIBS)