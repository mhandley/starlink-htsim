// -*- c-basic-offset: 4; tab-width: 8; indent-tabs-mode: t -*-

/*

MODEL:

o           o
 \         /
 o\       /o
  X-------X
 o/       \o
 /         \
o           o

*/

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
    eventlist.setEndtime(timeFromSec(100));
    Logfile logfile("xcp4.log", eventlist);
    logfile.setStartTime(timeFromSec(0));
    XcpSinkLoggerSampling sink_1_Logger(timeFromMs(1000), eventlist);
    XcpSinkLoggerSampling sink_2_Logger(timeFromMs(1000), eventlist);
    XcpSinkLoggerSampling sink_3_Logger(timeFromMs(1000), eventlist);
    XcpSinkLoggerSampling sink_4_Logger(timeFromMs(1000), eventlist);
    QueueLoggerSampling queueLogger(timeFromMs(10), eventlist);
    QueueLoggerSampling feeder_1_queueLogger(timeFromMs(10), eventlist);
    QueueLoggerSampling feeder_2_queueLogger(timeFromMs(10), eventlist);
    QueueLoggerSampling feeder_3_queueLogger(timeFromMs(10), eventlist);
    QueueLoggerSampling feeder_4_queueLogger(timeFromMs(10), eventlist);
    logfile.addLogger(sink_1_Logger);
    logfile.addLogger(sink_2_Logger);
    logfile.addLogger(sink_3_Logger);
    logfile.addLogger(sink_4_Logger);
    logfile.addLogger(queueLogger);
    logfile.addLogger(feeder_1_queueLogger);
    logfile.addLogger(feeder_2_queueLogger);
    logfile.addLogger(feeder_3_queueLogger);
    logfile.addLogger(feeder_4_queueLogger);

    XcpRtxTimerScanner xcpRtxScanner(timeFromMs(10), eventlist);
    Packet::set_packet_size(566);
    // outgoing
#define RTT1_1 timeFromMs(50)
#define RTT1_2 timeFromMs(50)
#define RTT1_3 timeFromMs(50)
#define RTT1_4 timeFromMs(50)
#define RTT_CORE timeFromMs(50)
#define RTT2_1 timeFromMs(50)
#define RTT2_2 timeFromMs(50)
#define RTT2_3 timeFromMs(50)
#define RTT2_4 timeFromMs(50)
    Pipe pipe1_1out(RTT1_1/2, eventlist); pipe1_1out.setName("pipe1_1out"); logfile.writeName(pipe1_1out);
    Pipe pipe1_2out(RTT1_2/2, eventlist); pipe1_2out.setName("pipe1_2out"); logfile.writeName(pipe1_2out);
    Pipe pipe1_3out(RTT1_3/2, eventlist); pipe1_3out.setName("pipe1_3out"); logfile.writeName(pipe1_3out);
    Pipe pipe1_4out(RTT1_4/2, eventlist); pipe1_4out.setName("pipe1_4out"); logfile.writeName(pipe1_4out);
    Pipe pipe2_1out(RTT2_1/2, eventlist); pipe2_1out.setName("pipe2_1out"); logfile.writeName(pipe2_1out);
    Pipe pipe2_2out(RTT2_2/2, eventlist); pipe2_2out.setName("pipe2_2out"); logfile.writeName(pipe2_2out);
    Pipe pipe2_3out(RTT2_3/2, eventlist); pipe2_3out.setName("pipe2_3out"); logfile.writeName(pipe2_3out);
    Pipe pipe2_4out(RTT2_4/2, eventlist); pipe2_4out.setName("pipe2_4out"); logfile.writeName(pipe2_4out);

    Pipe pipe_core_out(RTT_CORE/2, eventlist); pipe_core_out.setName("pipe_core_out"); logfile.writeName(pipe_core_out);

    // return
    Pipe pipe1_1back(RTT2_1/2, eventlist); pipe1_1back.setName("pipe1_1back"); logfile.writeName(pipe1_1back);
    Pipe pipe1_2back(RTT2_2/2, eventlist); pipe1_2back.setName("pipe1_2back"); logfile.writeName(pipe1_2back);
    Pipe pipe1_3back(RTT2_3/2, eventlist); pipe1_3back.setName("pipe1_3back"); logfile.writeName(pipe1_3back);
    Pipe pipe1_4back(RTT2_4/2, eventlist); pipe1_4back.setName("pipe1_4back"); logfile.writeName(pipe1_4back);
    Pipe pipe2_1back(RTT1_1/2, eventlist); pipe2_1back.setName("pipe2_1back"); logfile.writeName(pipe2_1back);
    Pipe pipe2_2back(RTT1_2/2, eventlist); pipe2_2back.setName("pipe2_2back"); logfile.writeName(pipe2_2back);
    Pipe pipe2_3back(RTT1_3/2, eventlist); pipe2_3back.setName("pipe2_3back"); logfile.writeName(pipe2_3back);
    Pipe pipe2_4back(RTT1_4/2, eventlist); pipe2_4back.setName("pipe2_4back"); logfile.writeName(pipe2_4back);

    Pipe pipe_core_back(RTT_CORE/2, eventlist); pipe_core_back.setName("pipe_core_back"); logfile.writeName(pipe_core_back);

#define FEEDER_BUFFER_1 memFromPkt(2000)
#define FEEDER_BUFFER_2 memFromPkt(2000)
#define FEEDER_BUFFER_3 memFromPkt(2000)
#define FEEDER_BUFFER_4 memFromPkt(2000)
#define SW_BUFFER memFromPkt(100)
#define INGRESS_LINKSPEED_1 speedFromMbps((uint64_t)2)
#define INGRESS_LINKSPEED_2 speedFromMbps((uint64_t)2)
#define INGRESS_LINKSPEED_3 speedFromMbps((uint64_t)2)
#define INGRESS_LINKSPEED_4 speedFromMbps((uint64_t)2)
#define CORE_LINKSPEED speedFromMbps((uint64_t)5)
#define EGRESS_LINKSPEED_1 speedFromMbps((uint64_t)2)
#define EGRESS_LINKSPEED_2 speedFromMbps((uint64_t)2)
#define EGRESS_LINKSPEED_3 speedFromMbps((uint64_t)2)
#define EGRESS_LINKSPEED_4 speedFromMbps((uint64_t)2)

    XcpQueue src_1_queue(INGRESS_LINKSPEED_1, FEEDER_BUFFER_1, eventlist, &feeder_1_queueLogger);
    src_1_queue.setName("Src_1_Queue");
    logfile.writeName(src_1_queue);

    XcpQueue src_2_queue(INGRESS_LINKSPEED_2, FEEDER_BUFFER_2, eventlist, &feeder_2_queueLogger);
    src_2_queue.setName("Src_2_Queue");
    logfile.writeName(src_2_queue);

    XcpQueue src_3_queue(INGRESS_LINKSPEED_3, FEEDER_BUFFER_3, eventlist, &feeder_3_queueLogger);
    src_3_queue.setName("Src_3_Queue");
    logfile.writeName(src_3_queue);

    XcpQueue src_4_queue(INGRESS_LINKSPEED_4, FEEDER_BUFFER_4, eventlist, &feeder_4_queueLogger);
    src_4_queue.setName("Src_4_Queue");
    logfile.writeName(src_4_queue);

    XcpQueue sink_1_queue(EGRESS_LINKSPEED_1, FEEDER_BUFFER_1, eventlist,NULL);
    sink_1_queue.setName("Sink_1_Queue");
    logfile.writeName(sink_1_queue);

    XcpQueue sink_2_queue(EGRESS_LINKSPEED_2, FEEDER_BUFFER_2, eventlist,NULL);
    sink_2_queue.setName("Sink_2_Queue");
    logfile.writeName(sink_2_queue);

    XcpQueue sink_3_queue(EGRESS_LINKSPEED_3, FEEDER_BUFFER_3, eventlist,NULL);
    sink_3_queue.setName("Sink_3_Queue");
    logfile.writeName(sink_3_queue);

    XcpQueue sink_4_queue(EGRESS_LINKSPEED_4, FEEDER_BUFFER_4, eventlist,NULL);
    sink_4_queue.setName("Sink_4_Queue");
    logfile.writeName(sink_4_queue);

    XcpQueue sw_1_queueout(CORE_LINKSPEED, SW_BUFFER, eventlist, &queueLogger);
    sw_1_queueout.setName("Sw_1_QueueOut");
    logfile.writeName(sw_1_queueout);

    XcpQueue sw_2_1_queueout(EGRESS_LINKSPEED_1, SW_BUFFER, eventlist, NULL);
    sw_2_1_queueout.setName("Sw_2_1_QueueOut");
    logfile.writeName(sw_2_1_queueout);

    XcpQueue sw_2_2_queueout(EGRESS_LINKSPEED_2, SW_BUFFER, eventlist, NULL);
    sw_2_2_queueout.setName("Sw_2_2_QueueOut");
    logfile.writeName(sw_2_2_queueout);

    XcpQueue sw_2_3_queueout(EGRESS_LINKSPEED_3, SW_BUFFER, eventlist, NULL);
    sw_2_3_queueout.setName("Sw_2_3_QueueOut");
    logfile.writeName(sw_2_3_queueout);

    XcpQueue sw_2_4_queueout(EGRESS_LINKSPEED_4, SW_BUFFER, eventlist, NULL);
    sw_2_4_queueout.setName("Sw_2_4_QueueOut");
    logfile.writeName(sw_2_4_queueout);
/*
    Queue sw_2_queueout(CORE_LINKSPEED_2, SW_BUFFER, eventlist, &queueLogger);   // Do we need another queue?
    sw_2_queueout.setName("Sw_2_QueueOut");
    logfile.writeName(sw_2_queueout);
*/
    XcpQueue sw_1_queueback(CORE_LINKSPEED, SW_BUFFER, eventlist,NULL);
    sw_1_queueback.setName("Sw_1_QueueBack");
    logfile.writeName(sw_1_queueback);

    XcpQueue sw_2_1_queueback(INGRESS_LINKSPEED_1, SW_BUFFER, eventlist,NULL);
    sw_2_1_queueback.setName("Sw_2_1_QueueBack");
    logfile.writeName(sw_2_1_queueback);

    XcpQueue sw_2_2_queueback(INGRESS_LINKSPEED_2, SW_BUFFER, eventlist,NULL);
    sw_2_2_queueback.setName("Sw_2_2_QueueBack");
    logfile.writeName(sw_2_2_queueback);

    XcpQueue sw_2_3_queueback(INGRESS_LINKSPEED_3, SW_BUFFER, eventlist,NULL);
    sw_2_3_queueback.setName("Sw_2_3_QueueBack");
    logfile.writeName(sw_2_3_queueback);

    XcpQueue sw_2_4_queueback(INGRESS_LINKSPEED_4, SW_BUFFER, eventlist,NULL);
    sw_2_4_queueback.setName("Sw_2_4_QueueBack");
    logfile.writeName(sw_2_4_queueback);

/*
    Queue sw_2_queueback(INGRESS_LINKSPEED_2, SW_BUFFER, eventlist,NULL);
    sw_2_queueout.setName("Sw_2_QueueOut");
    logfile.writeName(sw_2_queueout);
*/
    XcpSrc *xcp_src_1, *xcp_src_2, *xcp_src_3, *xcp_src_4;
    XcpSink *xcp_sink_1, *xcp_sink_2, *xcp_sink_3, *xcp_sink_4;

    xcp_src_1 = new XcpSrc(NULL, NULL, eventlist);
    xcp_src_1->setName("XcpSrc_1");
    xcp_src_1->_ssthresh = timeAsSec(RTT1_1+RTT2_1+RTT_CORE) * speedFromMbps((uint64_t)10);
    logfile.writeName(*xcp_src_1);

    xcp_src_2 = new XcpSrc(NULL, NULL, eventlist);
    xcp_src_2->setName("XcpSrc_2");
    xcp_src_2->_ssthresh = timeAsSec(RTT1_2+RTT2_2+RTT_CORE) * speedFromMbps((uint64_t)10);
    logfile.writeName(*xcp_src_2);

    xcp_src_3 = new XcpSrc(NULL, NULL, eventlist);
    xcp_src_3->setName("XcpSrc_3");
    xcp_src_3->_ssthresh = timeAsSec(RTT1_3+RTT2_3+RTT_CORE) * speedFromMbps((uint64_t)10);
    logfile.writeName(*xcp_src_3);

    xcp_src_4 = new XcpSrc(NULL, NULL, eventlist);
    xcp_src_4->setName("XcpSrc_4");
    xcp_src_4->_ssthresh = timeAsSec(RTT1_4+RTT2_4+RTT_CORE) * speedFromMbps((uint64_t)10);
    logfile.writeName(*xcp_src_4);

    xcp_sink_1 = new XcpSink();
    xcp_sink_1->setName("XcpSink_1");
    logfile.writeName(*xcp_sink_1);

    xcp_sink_2 = new XcpSink();
    xcp_sink_2->setName("XcpSink_2");
    logfile.writeName(*xcp_sink_2);

    xcp_sink_3 = new XcpSink();
    xcp_sink_3->setName("XcpSink_3");
    logfile.writeName(*xcp_sink_3);

    xcp_sink_4 = new XcpSink();
    xcp_sink_4->setName("XcpSink_4");
    logfile.writeName(*xcp_sink_4);

    xcpRtxScanner.registerXcp(*xcp_src_1);
    xcpRtxScanner.registerXcp(*xcp_src_2);
    xcpRtxScanner.registerXcp(*xcp_src_3);
    xcpRtxScanner.registerXcp(*xcp_src_4);

    Route *routeout_1, *routeback_1, *routeout_2, *routeback_2, *routeout_3, *routeback_3, *routeout_4, *routeback_4;
    routeout_1 = new route_t();
    routeout_1->push_back(&src_1_queue);
    routeout_1->push_back(&pipe1_1out);
    routeout_1->push_back(&sw_1_queueout);
    routeout_1->push_back(&pipe_core_out);
    routeout_1->push_back(&sw_2_1_queueout);
    routeout_1->push_back(&pipe2_1out);
    routeout_1->push_back(xcp_sink_1);

    routeout_2 = new route_t();
    routeout_2->push_back(&src_2_queue);
    routeout_2->push_back(&pipe1_2out);
    routeout_2->push_back(&sw_1_queueout);
    routeout_2->push_back(&pipe_core_out);
    routeout_2->push_back(&sw_2_2_queueout);
    routeout_2->push_back(&pipe2_2out);
    routeout_2->push_back(xcp_sink_2);

    routeout_3 = new route_t();
    routeout_3->push_back(&src_3_queue);
    routeout_3->push_back(&pipe1_3out);
    routeout_3->push_back(&sw_1_queueout);
    routeout_3->push_back(&pipe_core_out);
    routeout_3->push_back(&sw_2_3_queueout);
    routeout_3->push_back(&pipe2_3out);
    routeout_3->push_back(xcp_sink_3);

    routeout_4 = new route_t();
    routeout_4->push_back(&src_4_queue);
    routeout_4->push_back(&pipe1_4out);
    routeout_4->push_back(&sw_1_queueout);
    routeout_4->push_back(&pipe_core_out);
    routeout_4->push_back(&sw_2_4_queueout);
    routeout_4->push_back(&pipe2_4out);
    routeout_4->push_back(xcp_sink_4);
    
    routeback_1 = new route_t();
    routeback_1->push_back(&sink_1_queue);
    routeback_1->push_back(&pipe1_1back);
    routeback_1->push_back(&sw_1_queueback);
    routeback_1->push_back(&pipe_core_back);
    routeback_1->push_back(&sw_2_1_queueback);
    routeback_1->push_back(&pipe2_1back);
    routeback_1->push_back(xcp_src_1);

    routeback_2 = new route_t();
    routeback_2->push_back(&sink_2_queue);
    routeback_2->push_back(&pipe1_2back);
    routeback_2->push_back(&sw_1_queueback);
    routeback_2->push_back(&pipe_core_back);
    routeback_2->push_back(&sw_2_2_queueback);
    routeback_2->push_back(&pipe2_2back);
    routeback_2->push_back(xcp_src_2);

    routeback_3 = new route_t();
    routeback_3->push_back(&sink_3_queue);
    routeback_3->push_back(&pipe1_3back);
    routeback_3->push_back(&sw_1_queueback);
    routeback_3->push_back(&pipe_core_back);
    routeback_3->push_back(&sw_2_3_queueback);
    routeback_3->push_back(&pipe2_3back);
    routeback_3->push_back(xcp_src_3);

    routeback_4 = new route_t();
    routeback_4->push_back(&sink_4_queue);
    routeback_4->push_back(&pipe1_4back);
    routeback_4->push_back(&sw_1_queueback);
    routeback_4->push_back(&pipe_core_back);
    routeback_4->push_back(&sw_2_4_queueback);
    routeback_4->push_back(&pipe2_4back);
    routeback_4->push_back(xcp_src_4);
    
    xcp_src_1->connect(*routeout_1, *routeback_1, *xcp_sink_1, 0);
    xcp_src_2->connect(*routeout_2, *routeback_2, *xcp_sink_2, timeFromSec(3));
    xcp_src_3->connect(*routeout_3, *routeback_3, *xcp_sink_3, timeFromSec(6));
    xcp_src_4->connect(*routeout_4, *routeback_4, *xcp_sink_4, timeFromSec(20));

    xcp_src_4->set_flowsize(2300000);
    xcp_src_3->set_flowsize(8000000);

    sink_1_Logger.monitorSink(xcp_sink_1);
    sink_2_Logger.monitorSink(xcp_sink_2);
    sink_3_Logger.monitorSink(xcp_sink_3);
    sink_4_Logger.monitorSink(xcp_sink_4);

    // GO!                                                                                      
    while (eventlist.doNextEvent()) {
    }
}
