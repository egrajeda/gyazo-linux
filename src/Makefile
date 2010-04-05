all: gyazo.o
	gcc $^ -o gyazo `pkg-config gtk+-2.0 libcurl --libs`

gyazo.o: gyazo.c
	gcc -c $^ -o $@ `pkg-config gtk+-2.0 libcurl --cflags`
