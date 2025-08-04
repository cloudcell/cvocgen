CC=gcc
CFLAGS=-I. -O2 -Wall
LDFLAGS=

# Add your C source files here
SOURCES=cvocgen.c

# The name of your executable
EXECUTABLE=cvocgen

# Output directory
BIN_DIR=../bin

all: $(BIN_DIR)/$(EXECUTABLE)

$(BIN_DIR)/$(EXECUTABLE): $(SOURCES)
	mkdir -p $(BIN_DIR)
	$(CC) $(CFLAGS) -o $(BIN_DIR)/$(EXECUTABLE) $(SOURCES) $(LDFLAGS)

clean:
	rm -f $(BIN_DIR)/$(EXECUTABLE)
