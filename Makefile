CC      = gcc
CFLAGS  = -Wall -Wextra -pthread
TARGET  = test_neighbor
OBJS    = neighbor.o test_neighbor.o

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS)

neighbor.o: neighbor.c neighbor.h
	$(CC) $(CFLAGS) -c neighbor.c

test_neighbor.o: test_neighbor.c neighbor.h
	$(CC) $(CFLAGS) -c test_neighbor.c

clean:
	rm -f $(OBJS) $(TARGET)

