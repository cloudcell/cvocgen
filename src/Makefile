CC=gcc
CFLAGS=-I. -O2 -Wall
LDFLAGS=

# Add your C source files here
SOURCES=cvocgen.c

# The name of your executable
EXECUTABLE=cvocgen

all: $(EXECUTABLE)

$(EXECUTABLE): $(SOURCES)
	$(CC) $(CFLAGS) -o $(EXECUTABLE) $(SOURCES) $(LDFLAGS)

clean:
	rm -f $(EXECUTABLE)
