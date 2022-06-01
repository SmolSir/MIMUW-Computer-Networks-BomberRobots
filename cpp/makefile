CFLAGS = -Wall -Wextra -Wconversion -Werror -g -O2 -std=gnu++20 -l pthread	\
	-lboost_program_options  -Wl,-rpath -Wl,/opt/gcc-11.2/lib64
CC = /opt/gcc-11.2/bin/g++-11.2
INC = -I include

all: robots-client robots-server

robots-client: client.o
	$(CC) $(CFLAGS) -o $@ client.o

robots-server: server.o
	$(CC) $(CFLAGS) -o $@ server.o

clean:
	-rm -f *.o robots-client robots-server

.cpp.o:
	$(CC) $(CFLAGS) $(INC) -c $<