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

import os
from fabric.tasks import execute
from fabfile import *
import distbenchr as dbr
import signal
import time

PHYS_CPU_NUM    = 48
MEM_SIZE        = 128
MAX_UVM_NUM     = 768
MAX_VCPU_NUM    = 16
BASE_PORT       = 33000
CONN_NUM        = MAX_UVM_NUM * MAX_VCPU_NUM
TH_RATE_BASE    = 2000
TH_RATE_STEP    = 2000
TH_RATE_MAX     = 50000
LT_RATE         = TH_RATE_BASE
LAT_DIST        = 'fixed'
MEAN_LAT        = 1000 # usec

args = {
        'thr_n'         : 1,
        'vcpu_n'        : 1,
        'vm_n'          : PHYS_CPU_NUM,
        'mem_s'         : MEM_SIZE,
        'base_port'     : BASE_PORT,
        'conn_n'        : CONN_NUM,
        'th_rate_base'  : TH_RATE_BASE,
        'th_rate_step'  : TH_RATE_STEP,
        'th_rate_max'   : TH_RATE_MAX,
        'lt_rate'       : LT_RATE,

        'lat_dist'      : LAT_DIST,
        'mean_lat'      : MEAN_LAT,

        'exp_prefix'    : 'exp_prefix',
        }

def eval_kata():
    args['exp_prefix'] = 'kata'
    #for vcpu_num in [1, 4, 16]:
    for vcpu_num in [1]:
        args['vcpu_n'] = vcpu_num
        args['thr_n']  = vcpu_num
        for uvm_num in range(2, 50, 2):
            args['vm_n'] = uvm_num

            execute(server_cleanup)
            execute(run_kata, **args)
            mnt = dbr.Monitor()
            args['should_wait']=True
            mnt.bg_execute(run_lancet, **args)
            mnt.monitor()
            mnt.killall()

def eval_single_vm():
    args['exp_prefix'] = 'single_vm'
    #for vcpu_num in [1, 4, 16]:
    for vcpu_num in [1]:
        args['vcpu_n'] = vcpu_num
        args['thr_n']  = vcpu_num
        for uvm_num in [1, 40, 30, 20, 10]:
            args['vm_n'] = uvm_num

            execute(server_cleanup)
            mnt = dbr.Monitor()
            args['should_wait']=False
            mnt.bg_execute(run_single_vm, **args)
            time.sleep(15 + uvm_num/10)
            args['should_wait']=True
            mnt.bg_execute(run_lancet, **args)
            mnt.monitor()
            mnt.killall()

def main():
    # Normal fabric usage - synchronous
    execute(get_info)

    MLN = 1000 * 1000

    # lat/throughput curves
    for mean_lat in [100]: # FAKE SERVICE TIME (SPINNING): 100usec for each RPC packet
        args['mean_lat'] = mean_lat
        args['th_rate_base'] = 10 * 1000 # START LOAD IS 10k QPS
        args['th_rate_step'] = 70 * 1000 # INCREASE THE LOAD BY THE FOLLOWING STEP
        args['th_rate_max'] = 350 * 1000 # FINAL LOAD
        
        eval_single_vm()
        eval_kata()
    
if __name__ == "__main__":
    # FIXME: Terminal is messed up without this
    os.setpgrp() # create new process group, become its leader
    try:
        main()
    except:
        import traceback
        traceback.print_exc()
    finally:
        os.killpg(0, signal.SIGKILL) # kill all processes in my group
