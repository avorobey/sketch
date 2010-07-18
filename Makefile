all: sketch

sketch: sketch.o symbols.o builtins.o
	g++ -o sketch sketch.o builtins.o symbols.o

sketch.o: sketch.c common.h
	gcc -Wall -std=c99 -c sketch.c

builtins.o: builtins.c common.h
	gcc -Wall -std=c99 -c builtins.c

symbols.o: symbols.cc
	g++ -Wall -c symbols.cc

clean:
	rm *.o sketch
