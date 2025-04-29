all: drone
	drone.exe
	
drone: drone.o
	gcc -pg -g -no-pie -std=c99 -o drone.exe drone.o pdcurses.a

drone.o: drone.c
	gcc -Wall -pg -std=c99 -g -c drone.c -o drone.o

clean:
	rm *.o
	rm drone.exe
	rm drone