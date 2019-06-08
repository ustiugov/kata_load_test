
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

COUNT=`ls /sys/class/net/ | wc -l`

killall iperf3
killall firecracker
killall linux_synthetic

start=0
upperlim=$COUNT
parallel=10

for ((i=0; i<parallel; i++)); do
  s=$((i * upperlim / parallel))
  e=$(((i+1) * upperlim / parallel))
  for ((j=s; j<e; j++)); do
    sudo ip link del fc-$j-tap0 2> /dev/null
    sudo ip link del proc-$j-tap0 2> /dev/null
  done &
done

wait


rm -rf output/*
rm -rf /tmp/firecracker-sb*

# DMITRII
# Restore ephemeral port range, iptables and prohibit IP forwarding
sudo sysctl -w net.ipv4.ip_local_port_range="32768 60999"
#sudo iptables -t nat -F
sudo iptables-restore $HOME/saved_iptables
sudo sh -c "echo 0 > /proc/sys/net/ipv4/ip_forward" # usually the default

# Remove docker containers
docker stop $(docker ps -q)
docker rm $(docker ps -aq)
sleep 5
docker rm --force $(docker ps -aq)

# Kill the SNMP daemon
sudo service snmpd stop

sudo service docker restart
docker rm --force $(docker ps -aq)

echo Cleaning is done

