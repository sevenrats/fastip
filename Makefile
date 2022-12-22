default: build

build: clean
	gcc -Wall -o fastip main.c util.c -l curl 

clean:
	rm -rf fastip 

test: build
	./fastip 