
CC=gcc
CFLAGS=-Wall -g -pthread
TARGET1=myinotify
TARGET2=mypeer
GARGET3=mytracker
OBJ1=inotify_test.o inotify_utils.o event_queue.o
OBJ2=peer.o pkt.o peertable.o filetable.o
OBJ3=tracker.o pkt.o peertable.o filetable.o

..c.o:
	        $(CC) $(CFLAGS) -c $<

$(TARGET1): $(OBJ1)
	        $(CC) $(CFLAGS) $(OBJ1) -o $(TARGET1)
$(TARGET2): $(OBJ2)
	        $(CC) $(CFLAGS) $(OBJ2) -o $(TARGET2)
$(TARGET3): $(OBJ3)
	        $(CC) $(CFLAGS) $(OBJ3) -o $(TARGET3)

clean:
	rm -rf $(TARGET1)
	rm -rf $(TARGET2)
	rm -rf $(TARGET3)
	rm -rf $(OBJ1)
	rm -rf $(OBJ2)
	rm -rf $(OBJ3)
	rm -rf core.*
