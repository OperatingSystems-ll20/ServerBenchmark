INCLUDE_DIR = ../../include/client -I../../include/common
BIN_DIR = ../../bin
COMMON_SRC = ../common/pathHelper.c

EXEC_NAME = client
CC = gcc

all: directory client 
LINK_LIBS = -lpthread

directory:
		@mkdir -p $(BIN_DIR)

client:
		$(CC) -I $(INCLUDE_DIR) $(EXEC_NAME).c $(COMMON_SRC) -o $(BIN_DIR)/$(EXEC_NAME) $(LINK_LIBS)