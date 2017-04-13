life: life.c lcgrand.o list.o game.o
	gcc -o life life.c lcgrand.o list.o game.o
lcgrand.o: lcgrand.c lcgrand.h
	gcc -c lcgrand.c lcgrand.h
list.o: list.c list.h
	gcc -c list.c list.h
game.o: game.c game.h
	gcc -c game.c game.h

clean:
	rm *.o
	rm *.gch
