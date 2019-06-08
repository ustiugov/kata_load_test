
#Open Source License.
#
#Copyright 2019 Ecole Polytechnique Federale Lausanne (EPFL)
#
#Permission is hereby granted, free of charge, to any person obtaining a copy
#of this software and associated documentation files (the "Software"), to deal
#in the Software without restriction, including without limitation the rights
#to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
#copies of the Software, and to permit persons to whom the Software is
#furnished to do so, subject to the following conditions:
#
#The above copyright notice and this permission notice shall be included in
#all copies or substantial portions of the Software.
#
#THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
#IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
#FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
#AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
#LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
#OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
#THE SOFTWARE.

#!/bin/bash

THR_NUM=$1
NUM=$2
RUNTIME=$3
echo Running VM with runtime=$RUNTIME, thread/vcpus_num=$THR_NUM, VM count=$NUM

sudo sysctl -w net.ipv4.ip_local_port_range="51000 65535"
sudo sysctl -w net.ipv4.conf.all.forwarding=1
# Avoid "neighbour: arp_cache: neighbor table overflow!"
sudo sysctl -w net.ipv4.neigh.default.gc_thresh1=1024
sudo sysctl -w net.ipv4.neigh.default.gc_thresh2=2048
sudo sysctl -w net.ipv4.neigh.default.gc_thresh3=4096

for ((i=0; i<NUM; i++)); do
    port=$((33000 + i))
    # for some reason, N cpu demands --cpus=(N+1) in docker (at least, for Kata)
    #docker run --cpus=$((THR_NUM-1)) -dit --name alpine_${i} --runtime=${RUNTIME} -p $port:5201 \ FIXME
    docker run -dit --name alpine_${i} --runtime=${RUNTIME} -p $port:5201 \
        ustiugov/alpine_gv /tmp/servers/linux_synthetic $THR_NUM 5201
done

echo Guests are ready!
