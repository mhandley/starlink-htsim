CC = g++
CFLAGS = -Wall -g
CRT=`pwd`
INCLUDE= -I/$(CRT)/.. -I$(CRT) 
#-I$(CRT)/ksp -I$(CRT)/ksp/boost
LIB=-L.. 
#-Lksp

HDRS=

all:	1tcp


1tcp:	main-1tcp.o ../libhtsim.a
	$(CC) $(CFLAGS) main-1tcp.o $(LIB) -lhtsim -o 1tcp

main-1tcp.o: main-1tcp.cpp $(HDRS)
	$(CC) $(INCLUDE) $(CFLAGS) -c main-1tcp.cpp 

%.o : %.cpp $(HDRS)
	$(CC) $(INCLUDE) $(CFLAGS) -c $< -o $@

clean:	
	rm -f *.o starlink 1tcp
