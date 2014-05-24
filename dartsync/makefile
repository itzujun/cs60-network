
CC=gcc
CFLAGS=-Wall -g -pthread
TARGET1=mypeer
TARGET2=mytracker
OBJ1=peer/peer.o common/pkt.o common/peertable.o common/filetable.o inotify/inotify_watch.o inotify/inotify_utils.o inotify/event_queue.o
OBJ2=tracker/tracker.o common/pkt.o common/peertable.o common/filetable.o

..c.o:
	        $(CC) $(CFLAGS) -c $<

all: $(TARGET1) $(TARGET2)

$(TARGET1): $(OBJ1)
	        $(CC) $(CFLAGS) $(OBJ1) -o $(TARGET1)
$(TARGET2): $(OBJ2)
	        $(CC) $(CFLAGS) $(OBJ2) -o $(TARGET2)

clean:
	rm -rf $(TARGET1)
	rm -rf $(TARGET2)
	rm -rf $(OBJ1)
	rm -rf $(OBJ2)
	rm -rf core.*
