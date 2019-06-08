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


#!/usr/bin/python

from fabric.api import env, roles, run
from distbenchr import run_bg
import os

LANCET_DIR =  "/path/to/lancet/dir" # CONFIGURE HERE
SERVER_DIR = "/path/to/repo/dir" # CONFIGURE HERE
S_IP = "10.90.36.32" # SET SERVER IP HERE
BASE_RES_PATH   = '/path/to/results/dir' # CONFIGURE HERE

env.roledefs = {
        'servers': [ S_IP ],
        'clients': ['icnals15', 'icnals12', 'icnals07'], # SET CLIENT IPs HERE
        'driver': ['icnals12'], # ONE OF THE CLIENTS THAT WILL COORDINATE THE OTHERS
        }


@roles('servers', 'clients', 'driver')
def get_info():
    run('hostname')

def get_dst_list(base_port, port_num):
    dst_list = ""
    base_port = int(base_port)
    port_num = int(port_num)
    for port in range (base_port, base_port+port_num-1):
        dst_list += S_IP + ":" + str(port) + ","
    dst_list += S_IP + ":" + str(base_port+port_num-1)

    return dst_list

# Cleans server setup
@roles('servers')
def server_cleanup():
    cmd = "{}/helper_scripts/9.cleanup.sh".format(SERVER_DIR)
    run(cmd)

@roles('servers')
def run_kata(thr_n, vcpu_n, vm_n, mem_s, base_port, conn_n, th_rate_base,
        th_rate_step, th_rate_max, lt_rate, lat_dist, mean_lat, exp_prefix, should_wait=False):
    proc_num = vm_n
    cmd = "{}/helper_scripts/run_docker_vm.sh {} {} kata-runtime".format(SERVER_DIR, thr_n, proc_num)
    run(cmd)

@run_bg('servers')
def run_single_vm(thr_n, vcpu_n, vm_n, mem_s, base_port, conn_n, th_rate_base,
        th_rate_step, th_rate_max, lt_rate, lat_dist, mean_lat, exp_prefix, should_wait=False):
    proc_num = vm_n
    last_port = int(base_port) + int(proc_num)
    port_fwd = "{}-{}:{}-{}".format(base_port, last_port, base_port, last_port)
    runtime = "kata-runtime"
    # FIXME SET CPU COUNT TO THE NUMBER OF CORES AVAILABLE ON THE SERVER SIDE
    cmd = "docker run --cpus=48 --name ubuntu_vm --runtime={} ".format(runtime)
    cmd += "-p {}  ustiugov/ubuntu_kata:latest ".format(port_fwd)
    cmd += "/tmp/run_single_vm_proc.sh {} {}".format(thr_n, proc_num)
    run(cmd)

@run_bg('driver')
def run_lancet(thr_n, vcpu_n, vm_n, mem_s, base_port, conn_n, th_rate_base,
        th_rate_step, th_rate_max, lt_rate, lat_dist, mean_lat, exp_prefix):
    exp_prefix = exp_prefix.strip('\"')
    lat_dist = lat_dist.strip('\"')

    dst_list = get_dst_list( base_port=base_port, port_num=vm_n )
    res_p = "%s/res_%s_%dconn_%sLatDist_%dusec/vcpu_%d/" % (
            BASE_RES_PATH, exp_prefix, int(conn_n), lat_dist, int(mean_lat), int(vcpu_n))

    if not os.path.exists(res_p):
        os.makedirs(res_p)
    res_p += "results_fc{}_.txt".format(vm_n)

    cmd = "cd {} && ".format(LANCET_DIR)
    cmd += "./coordinator/coordinator -comProto TCP -loadThreads 8 -idist fixed "
    cmd += "--appProto synthetic:{}:{} ".format(lat_dist, mean_lat)
    cmd += "-loadAgents icnals12,icnals07 -loadBinary agents/agent -loadConn {} ".format(
            conn_n)
    cmd += "-loadPattern step:{}:{}:{} ".format(
            th_rate_base, th_rate_step, th_rate_max)
    cmd += "-ltAgents icnals15 -ltBinary agents/agent -ltConn {} -lqps {} ".format(
            conn_n, lt_rate)
    cmd += "-targetHost {} ".format(dst_list)
    cmd += "| tee {}".format(res_p)

    run(cmd)
