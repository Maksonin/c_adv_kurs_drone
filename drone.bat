taskkill /f /im drone.exe
gcc -Wall -pg -std=c99 -g -c drone.c -o drone.o
gcc -pg -g -no-pie -std=c99 -o drone.exe drone.o pdcurses.a
pause(0)
drone.exe