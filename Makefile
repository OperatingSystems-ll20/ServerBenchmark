BIN_DIR = "./bin"
SRC_DIR = "./src"

MAKE_FLAGS = --no-print-directory

all: client preHeavy 

client:
		$(MAKE) $(MAKE_FLAGS) -C $(SRC_DIR)/client

preHeavy:
		@mkdir -p $(BIN_DIR)
		$(MAKE) $(MAKE_FLAGS) -C $(SRC_DIR)/preHeavy  


clean:
		@rm -r -f $(BIN_DIR)