// -*- c-basic-offset: 4; tab-width: 8; indent-tabs-mode: t -*-

#include "network.h"
#include "logfile.h"
#include "loggers.h"
#include "xcp.h"
#include "xcpqueue.h"
#include "tcp.h"
#include "queue.h"
#include "pipe.h"

int main() {
    EventList eventlist;
    eventlist.setEndtime(timeFromSec(40));
    Logfile logfile("xcp.log", eventlist);
    logfile.setStartTime(timeFromSec(0));
    XcpSinkLoggerSampling sinkLogger(timeFromMs(1000), eventlist);
    QueueLoggerSampling queueLogger(timeFromMs(10), eventlist);

    logfile.addLogger(sinkLogger);
    logfile.addLogger(queueLogger);

    XcpRtxTimerScanner xcpRtxScanner(timeFromMs(10), eventlist);
    Packet::set_packet_size(566);
    // outgoing
#define RTT1 timeFromMs(50)
#define RTT2 timeFromMs(50)
    Pipe pipe1out(RTT1/2, eventlist); pipe1out.setName("pipe1out"); logfile.writeName(pipe1out);
    Pipe pipe2out(RTT2/2, eventlist); pipe2out.setName("pipe2out"); logfile.writeName(pipe2out);
    // return
    Pipe pipe1back(RTT1/2, eventlist); pipe1back.setName("pipe1back"); logfile.writeName(pipe1back);
    Pipe pipe2back(RTT2/2, eventlist); pipe2back.setName("pipe2back"); logfile.writeName(pipe2back);

#define FEEDER_BUFFER memFromPkt(100)
#define SW_BUFFER memFromPkt(100)
#define INGRESS_LINKSPEED speedFromMbps((uint64_t)20)
#define CORE_LINKSPEED speedFromMbps((uint64_t)10)
    XcpQueue srcqueue(INGRESS_LINKSPEED, FEEDER_BUFFER, eventlist,NULL,1.0);
    srcqueue.setName("SrcQueue");
    logfile.writeName(srcqueue);

    XcpQueue sinkqueue(CORE_LINKSPEED, FEEDER_BUFFER, eventlist,NULL,1.0);
    sinkqueue.setName("SinkQueue");
    logfile.writeName(sinkqueue);

    XcpQueue swqueueout(CORE_LINKSPEED, SW_BUFFER, eventlist, &queueLogger,1.0);
    swqueueout.setName("SwQueueOut");
    logfile.writeName(swqueueout);

    XcpQueue swqueueback(INGRESS_LINKSPEED, SW_BUFFER, eventlist,NULL,1.0);
    swqueueout.setName("SwQueueOut");
    logfile.writeName(swqueueout);

    XcpSrc* xcp_src;
    XcpSink* xcp_sink;

    xcp_src = new XcpSrc(NULL, NULL, eventlist);
    xcp_src->setName("XcpSrc");
    //xcp_src->_ssthresh = timeAsSec(RTT1+RTT2) * speedFromMbps((uint64_t)10);
    logfile.writeName(*xcp_src);

    xcp_sink = new XcpSink();
    xcp_sink->setName("XcpSink");
    logfile.writeName(*xcp_sink);

    xcpRtxScanner.registerXcp(*xcp_src);

    Route* routeout, *routeback;
    routeout = new route_t();
    routeout->push_back(&srcqueue);
    routeout->push_back(&pipe1out);
    routeout->push_back(&swqueueout);
    routeout->push_back(&pipe2out);
    routeout->push_back(xcp_sink);
    
    routeback = new route_t();
    routeback->push_back(&sinkqueue);
    routeback->push_back(&pipe2back);
    routeback->push_back(&swqueueback);
    routeback->push_back(&pipe1back);
    routeback->push_back(xcp_src);
    
    xcp_src->connect(*routeout, *routeback, *xcp_sink, 0);
    sinkLogger.monitorSink(xcp_sink);
    // GO!                                                                                      
    while (eventlist.doNextEvent()) {
    }
}
