CC = g++
CFLAGS = -Wall -std=c++11 -g -D XCP_STATIC_NETWORK -D MPXCP_VERSION_1
CRT="`pwd`"
INCLUDE= -I/$(CRT)/.. -I$(CRT) 
#-I$(CRT)/ksp -I$(CRT)/ksp/boost
LIB=-L.. 
#-Lksp

HDRS=isl.h node.h satellite.h city.h constellation.h binary_heap.h

all:	starlink xcp-test main_static_network
#htsim_ndp_incast_shortflows_demo 

starlink:	main.o isl.o node.o satellite.o city.o constellation.o binary_heap.o ../libhtsim.a
	$(CC) $(CFLAGS) main.o isl.o node.o satellite.o city.o constellation.o binary_heap.o $(LIB) -lhtsim -o starlink

xcp-test:	main-xcp-test.o ../libhtsim.a
	$(CC) $(CFLAGS) main-xcp-test.o $(LIB) -lhtsim -o xcp-test

main_static_network: main-static-network.o isl.o node.o satellite.o city.o constellation.o binary_heap.o ../libhtsim.a
	$(CC) $(CFLAGS) main-static-network.o isl.o node.o satellite.o city.o constellation.o binary_heap.o $(LIB) -lhtsim -o main_static_network

main.o: main.cpp $(HDRS)
	$(CC) $(INCLUDE) $(CFLAGS) -c main.cpp 

main-static-network.o: main-static-network.cpp $(HDRS)
	$(CC) $(INCLUDE) $(CFLAGS) -c main-static-network.cpp

main-xcp-test.o: main-xcp-test.cpp $(HDRS)
	$(CC) $(INCLUDE) $(CFLAGS) -c main-xcp-test.cpp 

isl.o:	isl.cpp $(HDRS)
	$(CC) $(INCLUDE) $(CFLAGS) -c isl.cpp

node.o:	node.cpp $(HDRS)
	$(CC) $(INCLUDE) $(CFLAGS) -c node.cpp

satellite.o:	satellite.cpp $(HDRS)
	$(CC) $(INCLUDE) $(CFLAGS) -c satellite.cpp

city.o:	city.cpp $(HDRS)
	$(CC) $(INCLUDE) $(CFLAGS) -c city.cpp

binary_heap.o:	city.cpp $(HDRS)
	$(CC) $(INCLUDE) $(CFLAGS) -c binary_heap.cpp

%.o : %.cpp $(HDRS)
	$(CC) $(INCLUDE) $(CFLAGS) -c $< -o $@

clean:	
	rm -f *.o starlink xcp-test
