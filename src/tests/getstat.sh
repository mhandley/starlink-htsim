#!/usr/bin/bash

# make
# ./1xcp > 1xcp_out.txt
# ../parse_output xcp.log -ascii > 1xcp_out_log.txt
# cat 1xcp_out_log.txt | grep XCP_SINK > temp.txt

# make
# ./1tcp > 1tcp_out.txt
# ../parse_output tcp.log -ascii > 1tcp_out_log.txt
# cat 1tcp_out_log.txt | grep TCP_SINK > temp1.txt

# make
# ./2xcp > 2xcp_out.txt
# ../parse_output xcp2.log -ascii > 2xcp_out_log.txt
# cat 2xcp_out_log.txt | grep XCP_SINK > temp2xcp.txt

make
./4xcp > 4xcp_out.txt
../parse_output xcp4.log -ascii > 4xcp_out_log.txt
cat 4xcp_out_log.txt | grep XCP_SINK > temp4xcp.txt