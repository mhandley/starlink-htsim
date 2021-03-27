CC = g++
CFLAGS = -Wall -g -D XCP_STATIC_NETWORK -D MPXCP_VERSION_1
CRT="`pwd`"
INCLUDE= -I/$(CRT)/.. -I$(CRT) -I/$(CRT)/../starlink
#-I$(CRT)/ksp -I$(CRT)/ksp/boost
LIB=-L.. 
#-Lksp

HDRS=

all:	1tcp 2tcp 1xcp 2xcp 4xcp


1tcp:	main-1tcp.o ../libhtsim.a
	$(CC) $(CFLAGS) main-1tcp.o $(LIB) -lhtsim -o 1tcp

2tcp:	main-2tcp.o ../libhtsim.a
	$(CC) $(CFLAGS) main-2tcp.o $(LIB) -lhtsim -o 2tcp

1xcp:	main-1xcp.o ../libhtsim.a
	$(CC) $(CFLAGS) main-1xcp.o $(LIB) -lhtsim -o 1xcp

2xcp:	main-2xcp.o ../libhtsim.a
	$(CC) $(CFLAGS) main-2xcp.o $(LIB) -lhtsim -o 2xcp

4xcp:	main-4xcp.o ../libhtsim.a
	$(CC) $(CFLAGS) main-4xcp.o $(LIB) -lhtsim -o 4xcp

main-1tcp.o: main-1tcp.cpp $(HDRS)
	$(CC) $(INCLUDE) $(CFLAGS) -c main-1tcp.cpp 

main-2tcp.o: main-2tcp.cpp $(HDRS)
	$(CC) $(INCLUDE) $(CFLAGS) -c main-2tcp.cpp 

main-1xcp.o: main-1xcp.cpp $(HDRS)
	$(CC) $(INCLUDE) $(CFLAGS) -c main-1xcp.cpp 

main-2xcp.o: main-2xcp.cpp $(HDRS)
	$(CC) $(INCLUDE) $(CFLAGS) -c main-2xcp.cpp

main-4xcp.o: main-4xcp.cpp $(HDRS)
	$(CC) $(INCLUDE) $(CFLAGS) -c main-4xcp.cpp 

%.o : %.cpp $(HDRS)
	$(CC) $(INCLUDE) $(CFLAGS) -c $< -o $@

clean:	
	rm -f *.o starlink 1tcp 2tcp 1xcp 2xcp 4xcp
