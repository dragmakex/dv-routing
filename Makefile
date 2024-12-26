CC      = gcc
CFLAGS  = -Wall -Wextra -pthread
TARGET  = dv_routing

OBJS    = neighbor.o distance.o main.o

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS)

neighbor.o: neighbor.c neighbor.h
	$(CC) $(CFLAGS) -c neighbor.c

distance.o: distance.c distance.h
	$(CC) $(CFLAGS) -c distance.c

main.o: main.c neighbor.h distance.h
	$(CC) $(CFLAGS) -c main.c

clean:
	rm -f $(OBJS) $(TARGET)

