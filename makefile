CC=gcc
CFLAGS=-lrt -g -Wall
CAMERA_MACROS=-D WIDTH=1280 -D HEIGHT=800 -D QSIZE=120 -D FPS=30

all: gpumanager cammanager

gpumanager: gpumanager.o utility.o
	$(CC) -o gpumanager gpumanager.o utility.o $(CFLAGS) -lcrypto

cammanager: cammanager.o utility.o
	$(CC) -o cammanager cammanager.o utility.o $(CFLAGS)

gpumanager.o: gpumanager.c utility.h
	$(CC) -c gpumanager.c $(CFLAGS) $(CAMERA_MACROS)

cammanager.o: cammanager.c utility.h
	$(CC) -c cammanager.c $(CFLAGS) $(CAMERA_MACROS)

utility.o: utility.c utility.h
	$(CC) -c utility.c $(CFLAGS)

clean:
	rm gpumanager cammanager *.o *~
