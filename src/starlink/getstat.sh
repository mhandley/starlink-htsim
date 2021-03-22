#!/usr/bin/bash

make
./main_static_network > 2mpxcp_out.txt
../parse_output mpxcp1.log -ascii > 2mpxcp_out_log.txt
cat 2mpxcp_out_log.txt | grep XCP_SINK > temp2mpxcp.txt