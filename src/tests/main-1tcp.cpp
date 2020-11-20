// -*- c-basic-offset: 4; tab-width: 8; indent-tabs-mode: t -*-

#include "network.h"
#include "logfile.h"
#include "loggers.h"
#include "tcp.h"
#include "queue.h"
#include "pipe.h"

int main() {
    EventList eventlist;
    eventlist.setEndtime(timeFromSec(300));
    Logfile logfile("tcp.log", eventlist);
    logfile.setStartTime(timeFromSec(0));
    TcpSinkLoggerSampling sinkLogger(timeFromMs(1000), eventlist);
    QueueLoggerSampling queueLogger(timeFromMs(10), eventlist);
    
    logfile.addLogger(sinkLogger);
    logfile.addLogger(queueLogger);

    TcpRtxTimerScanner tcpRtxScanner(timeFromMs(10), eventlist);
    Packet::set_packet_size(566);
    // outgoing
#define RTT1 timeFromMs(50)
#define RTT2 timeFromMs(50)
    Pipe pipe1out(RTT1/2, eventlist); pipe1out.setName("pipe1out"); logfile.writeName(pipe1out);
    Pipe pipe2out(RTT2/2, eventlist); pipe2out.setName("pipe2out"); logfile.writeName(pipe2out);
    // return
    Pipe pipe1back(RTT1/2, eventlist); pipe1back.setName("pipe1back"); logfile.writeName(pipe1back);
    Pipe pipe2back(RTT2/2, eventlist); pipe2back.setName("pipe2back"); logfile.writeName(pipe2back);

#define FEEDER_BUFFER memFromPkt(20)
#define SW_BUFFER memFromPkt(220)
#define INGRESS_LINKSPEED speedFromMbps((uint64_t)20)
#define CORE_LINKSPEED speedFromMbps((uint64_t)10)
    Queue srcqueue(INGRESS_LINKSPEED, FEEDER_BUFFER, eventlist,NULL);
    srcqueue.setName("SrcQueue");
    logfile.writeName(srcqueue);

    Queue sinkqueue(CORE_LINKSPEED, FEEDER_BUFFER, eventlist,NULL);
    sinkqueue.setName("SinkQueue");
    logfile.writeName(sinkqueue);

    Queue swqueueout(CORE_LINKSPEED, SW_BUFFER, eventlist, &queueLogger);
    swqueueout.setName("SwQueueOut");
    logfile.writeName(swqueueout);

    Queue swqueueback(INGRESS_LINKSPEED, SW_BUFFER, eventlist,NULL);
    swqueueout.setName("SwQueueOut");
    logfile.writeName(swqueueout);

    TcpSrc* tcp_src;
    TcpSink* tcp_sink;

    tcp_src = new TcpSrc(NULL, NULL, eventlist);
    tcp_src->setName("TcpSrc");
    tcp_src->_ssthresh = timeAsSec(RTT1+RTT2) * speedFromMbps((uint64_t)10);
    logfile.writeName(*tcp_src);

    tcp_sink = new TcpSink();
    tcp_sink->setName("TcpSink");
    logfile.writeName(*tcp_sink);

    tcpRtxScanner.registerTcp(*tcp_src);

    Route* routeout, *routeback;
    routeout = new route_t();
    routeout->push_back(&srcqueue);
    routeout->push_back(&pipe1out);
    routeout->push_back(&swqueueout);
    routeout->push_back(&pipe2out);
    routeout->push_back(tcp_sink);
    
    routeback = new route_t();
    routeback->push_back(&sinkqueue);
    routeback->push_back(&pipe2back);
    routeback->push_back(&swqueueback);
    routeback->push_back(&pipe1back);
    routeback->push_back(tcp_src);
    
    tcp_src->connect(*routeout, *routeback, *tcp_sink, 0);
    sinkLogger.monitorSink(tcp_sink);
    // GO!                                                                                      
    while (eventlist.doNextEvent()) {
    }
}
