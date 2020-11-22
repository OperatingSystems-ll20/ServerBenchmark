BIN_DIR = "./bin"
SRC_DIR = "./src"

MAKE_FLAGS = --no-print-directory

all: client heavy preHeavy 

client:
		$(MAKE) $(MAKE_FLAGS) -C $(SRC_DIR)/client

heavy:
		$(MAKE) $(MAKE_FLAGS) -C $(SRC_DIR)/heavy

preHeavy:
		@mkdir -p $(BIN_DIR)
		$(MAKE) $(MAKE_FLAGS) -C $(SRC_DIR)/preHeavy  

clean:
		@rm -r -f $(BIN_DIR)