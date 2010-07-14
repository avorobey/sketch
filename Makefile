all: sketch

sketch: sketch.o symbols.o
	g++ -o sketch sketch.o symbols.o

sketch.o: sketch.c
	gcc -c sketch.c

symbols.o: symbols.cc
	g++ -c symbols.cc

clean:
	rm *.o sketch
