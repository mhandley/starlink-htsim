// -*- c-basic-offset: 4; tab-width: 8; indent-tabs-mode: t -*-

/*

MODEL:

o           o
 \         /
  \       /
   -------
  /       \
 /         \
o           o

*/

#include "network.h"
#include "logfile.h"
#include "loggers.h"
#include "tcp.h"
#include "queue.h"
#include "pipe.h"

int main() {
    EventList eventlist;
    eventlist.setEndtime(timeFromSec(300));
    Logfile logfile("tcp2.log", eventlist);
    logfile.setStartTime(timeFromSec(0));
    TcpSinkLoggerSampling sink_1_Logger(timeFromMs(1000), eventlist);
    TcpSinkLoggerSampling sink_2_Logger(timeFromMs(1000), eventlist);
    QueueLoggerSampling queueLogger(timeFromMs(10), eventlist);
    QueueLoggerSampling feeder_1_queueLogger(timeFromMs(10), eventlist);
    QueueLoggerSampling feeder_2_queueLogger(timeFromMs(10), eventlist);
    logfile.addLogger(sink_1_Logger);
    logfile.addLogger(sink_2_Logger);
    logfile.addLogger(queueLogger);
    logfile.addLogger(feeder_1_queueLogger);
    logfile.addLogger(feeder_2_queueLogger);

    TcpRtxTimerScanner tcpRtxScanner(timeFromMs(10), eventlist);
    Packet::set_packet_size(566);
    // outgoing
#define RTT1_1 timeFromMs(50)
#define RTT1_2 timeFromMs(50)
#define RTT_CORE timeFromMs(50)
#define RTT2_1 timeFromMs(50)
#define RTT2_2 timeFromMs(50)
    Pipe pipe1_1out(RTT1_1/2, eventlist); pipe1_1out.setName("pipe1_1out"); logfile.writeName(pipe1_1out);
    Pipe pipe1_2out(RTT1_2/2, eventlist); pipe1_2out.setName("pipe1_2out"); logfile.writeName(pipe1_2out);
    Pipe pipe2_1out(RTT2_1/2, eventlist); pipe2_1out.setName("pipe2_1out"); logfile.writeName(pipe2_1out);
    Pipe pipe2_2out(RTT2_2/2, eventlist); pipe2_2out.setName("pipe2_2out"); logfile.writeName(pipe2_2out);

    Pipe pipe_core_out(RTT_CORE/2, eventlist); pipe_core_out.setName("pipe_core_out"); logfile.writeName(pipe_core_out);

    // return
    Pipe pipe1_1back(RTT1_1/2, eventlist); pipe1_1back.setName("pipe1_1back"); logfile.writeName(pipe1_1back);
    Pipe pipe1_2back(RTT1_2/2, eventlist); pipe1_2back.setName("pipe1_2back"); logfile.writeName(pipe1_2back);
    Pipe pipe2_1back(RTT2_1/2, eventlist); pipe2_1back.setName("pipe2_1back"); logfile.writeName(pipe2_1back);
    Pipe pipe2_2back(RTT2_2/2, eventlist); pipe2_2back.setName("pipe2_2back"); logfile.writeName(pipe2_2back);

    Pipe pipe_core_back(RTT_CORE/2, eventlist); pipe_core_back.setName("pipe_core_back"); logfile.writeName(pipe_core_back);

#define FEEDER_BUFFER_1 memFromPkt(20)
#define FEEDER_BUFFER_2 memFromPkt(20)
#define SW_BUFFER memFromPkt(200)
#define INGRESS_LINKSPEED_1 speedFromMbps((uint64_t)20)
#define INGRESS_LINKSPEED_2 speedFromMbps((uint64_t)20)
#define CORE_LINKSPEED speedFromMbps((uint64_t)30)
#define EGRESS_LINKSPEED_1 speedFromMbps((uint64_t)20)
#define EGRESS_LINKSPEED_2 speedFromMbps((uint64_t)20)

    Queue src_1_queue(INGRESS_LINKSPEED_1, FEEDER_BUFFER_1, eventlist, &feeder_1_queueLogger);
    src_1_queue.setName("Src_1_Queue");
    logfile.writeName(src_1_queue);

    Queue src_2_queue(INGRESS_LINKSPEED_2, FEEDER_BUFFER_2, eventlist, &feeder_2_queueLogger);
    src_2_queue.setName("Src_2_Queue");
    logfile.writeName(src_2_queue);

    Queue sink_1_queue(EGRESS_LINKSPEED_1, FEEDER_BUFFER_1, eventlist,NULL);
    sink_1_queue.setName("Sink_1_Queue");
    logfile.writeName(sink_1_queue);

    Queue sink_2_queue(EGRESS_LINKSPEED_2, FEEDER_BUFFER_2, eventlist,NULL);
    sink_2_queue.setName("Sink_2_Queue");
    logfile.writeName(sink_2_queue);

    Queue sw_1_queueout(CORE_LINKSPEED, SW_BUFFER, eventlist, &queueLogger);
    sw_1_queueout.setName("Sw_1_QueueOut");
    logfile.writeName(sw_1_queueout);

    Queue sw_2_1_queueout(EGRESS_LINKSPEED_1, SW_BUFFER, eventlist, NULL);
    sw_2_1_queueout.setName("Sw_2_1_QueueOut");
    logfile.writeName(sw_2_1_queueout);

    Queue sw_2_2_queueout(EGRESS_LINKSPEED_2, SW_BUFFER, eventlist, NULL);
    sw_2_2_queueout.setName("Sw_2_2_QueueOut");
    logfile.writeName(sw_2_2_queueout);
/*
    Queue sw_2_queueout(CORE_LINKSPEED_2, SW_BUFFER, eventlist, &queueLogger);   // Do we need another queue?
    sw_2_queueout.setName("Sw_2_QueueOut");
    logfile.writeName(sw_2_queueout);
*/
    Queue sw_1_queueback(CORE_LINKSPEED, SW_BUFFER, eventlist,NULL);
    sw_1_queueback.setName("Sw_1_QueueBack");
    logfile.writeName(sw_1_queueback);

    Queue sw_2_1_queueback(INGRESS_LINKSPEED_1, SW_BUFFER, eventlist,NULL);
    sw_2_1_queueback.setName("Sw_2_1_QueueBack");
    logfile.writeName(sw_2_1_queueback);

    Queue sw_2_2_queueback(INGRESS_LINKSPEED_2, SW_BUFFER, eventlist,NULL);
    sw_2_2_queueback.setName("Sw_2_2_QueueBack");
    logfile.writeName(sw_2_2_queueback);


/*
    Queue sw_2_queueback(INGRESS_LINKSPEED_2, SW_BUFFER, eventlist,NULL);
    sw_2_queueout.setName("Sw_2_QueueOut");
    logfile.writeName(sw_2_queueout);
*/
    TcpSrc *tcp_src_1, *tcp_src_2;
    TcpSink *tcp_sink_1, *tcp_sink_2;

    tcp_src_1 = new TcpSrc(NULL, NULL, eventlist);
    tcp_src_1->setName("TcpSrc_1");
    tcp_src_1->_ssthresh = timeAsSec(RTT1_1+RTT1_2+RTT_CORE) * speedFromMbps((uint64_t)10);
    logfile.writeName(*tcp_src_1);

    tcp_src_2 = new TcpSrc(NULL, NULL, eventlist);
    tcp_src_2->setName("TcpSrc_2");
    tcp_src_2->_ssthresh = timeAsSec(RTT2_1+RTT2_2+RTT_CORE) * speedFromMbps((uint64_t)10);
    logfile.writeName(*tcp_src_2);

    tcp_sink_1 = new TcpSink();
    tcp_sink_1->setName("TcpSink_1");
    logfile.writeName(*tcp_sink_1);

    tcp_sink_2 = new TcpSink();
    tcp_sink_2->setName("TcpSink_2");
    logfile.writeName(*tcp_sink_2);

    tcpRtxScanner.registerTcp(*tcp_src_1);
    tcpRtxScanner.registerTcp(*tcp_src_2);

    Route *routeout_1, *routeback_1, *routeout_2, *routeback_2;
    routeout_1 = new route_t();
    routeout_1->push_back(&src_1_queue);
    routeout_1->push_back(&pipe1_1out);
    routeout_1->push_back(&sw_1_queueout);
    routeout_1->push_back(&pipe_core_out);
    routeout_1->push_back(&sw_2_1_queueout);
    routeout_1->push_back(&pipe1_2out);
    routeout_1->push_back(tcp_sink_1);

    routeout_2 = new route_t();
    routeout_2->push_back(&src_2_queue);
    routeout_2->push_back(&pipe2_1out);
    routeout_2->push_back(&sw_1_queueout);
    routeout_2->push_back(&pipe_core_out);
    routeout_2->push_back(&sw_2_2_queueout);
    routeout_2->push_back(&pipe2_2out);
    routeout_2->push_back(tcp_sink_2);
    
    routeback_1 = new route_t();
    routeback_1->push_back(&sink_1_queue);
    routeback_1->push_back(&pipe1_2back);
    routeback_1->push_back(&sw_1_queueback);
    routeback_1->push_back(&pipe_core_back);
    routeback_1->push_back(&sw_2_1_queueback);
    routeback_1->push_back(&pipe1_1back);
    routeback_1->push_back(tcp_src_1);

    routeback_2 = new route_t();
    routeback_2->push_back(&sink_2_queue);
    routeback_2->push_back(&pipe2_2back);
    routeback_2->push_back(&sw_1_queueback);
    routeback_2->push_back(&pipe_core_back);
    routeback_2->push_back(&sw_2_2_queueback);
    routeback_2->push_back(&pipe2_1back);
    routeback_2->push_back(tcp_src_2);
    
    tcp_src_1->connect(*routeout_1, *routeback_1, *tcp_sink_1, 0);
    tcp_src_2->connect(*routeout_2, *routeback_2, *tcp_sink_2, 0);

    sink_1_Logger.monitorSink(tcp_sink_1);
    sink_2_Logger.monitorSink(tcp_sink_2);

    // GO!                                                                                      
    while (eventlist.doNextEvent()) {
    }
}
