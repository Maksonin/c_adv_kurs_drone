taskkill /f /im drone.exe
gcc -Wall -pg -std=c99 -g -IH:/GB/C/PDcurses -c drone.c -o drone.o
gcc -pg -g -no-pie -std=c99 -o drone.exe drone.o H:/GB/C/PDcurses/wincon/pdcurses.a -IH:/GB/C/PDcurses
drone.exe
pause(0)