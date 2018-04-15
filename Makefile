.PHONY: test
test: main
	./main

.PHONY: main
main:
	gcc -O3 -std=c11 -o main main.c

