life:  life.c lcgrand.o list.o game.o
	gcc -Wall -g -o life life.c lcgrand.o list.o game.o -lpthread
lcgrand.o: lcgrand.c lcgrand.h
	gcc -g -c lcgrand.c lcgrand.h
list.o: list.c list.h
	gcc -g -c list.c list.h
game.o: game.c game.h
	gcc -g -c game.c game.h

clean:
	rm *.o
	rm *.gch
