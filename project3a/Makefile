# NAME: Megan Pham, Cody Do
# EMAIL: megspham@ucla.edu, D.Codyx@gmail.com
# ID: 505313300, 105140467

OPTIMIZE = -O2

CC = gcc
CFLAGS = $(OPTIMIZE) -g3 -Wall -Wextra -march=native -mtune=native -mrdrnd -lz -pthread -lrt

default: lab3a.c
	$(CC) $(CFLAGS) lab3a.c -o lab3a

dist: 
	tar -czf lab3a-505313300.tar.gz lab3a.c README Makefile ext2_fs.h

clean: 
	rm -f lab3a *.tar.gz 