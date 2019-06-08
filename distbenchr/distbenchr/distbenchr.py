
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

from fabric.api import env
from fabric.tasks import execute
import subprocess
import os

def run_bg(group_name):
    def decorator(fn):
        def wrapper(*args, **kwargs):
            fn(*args, **kwargs)
        wrapper._group_name = group_name
        wrapper._original = fn.__name__
        return wrapper
    return decorator

class Handler(object):
    def __init__(self, handler, should_wait):
        self.handler = handler
        self.should_wait = should_wait

class Monitor(object):

    def __init__(self):
        self.handlers = {}
        self.count = 0
        self.wait_pids = []

    def bg_execute(self, fn, *args, **kwargs):
        def handle_strings(x):
            if type(x) is str:
                return "\"{}\"".format(x)
            return str(x)

        should_wait = True
        str_args_l = map(handle_strings, args)
        f = lambda x: "{}={}".format(str(x[0]), handle_strings(x[1]))
        if "should_wait" in kwargs.keys():
            should_wait = kwargs['should_wait']
            kwargs.pop("should_wait")
        str_kwargs_l = map(f, kwargs.items())
        fab_args = ",".join(str_args_l+str_kwargs_l)
        if fab_args:
            fmt_arg = "{}:{}".format(fn._original, fab_args)
        else:
            fmt_arg = fn._original
        for server in env.roledefs[fn._group_name]:
            p = subprocess.Popen(["fab", fmt_arg, "-H", server])
            self.handlers[p.pid] = Handler(p, should_wait)
            if should_wait:
                self.wait_pids.append(p.pid)
            self.count += 1

    def killall(self):
        for p in self.handlers.values():
            p.handler.kill()

    def monitor(self, update_freq=1, cleanup_slack=2):
        while self.wait_pids:
            pid, status = os.wait()
            if status != 0:
                print "Status received: {}".format(status)
                self.handlers.pop(pid)
                self.killall()
                break
            if self.handlers[pid].should_wait:
                self.wait_pids.remove(pid)
                del self.handlers[pid]
            else:
                print "Warning: Process terminated normally - no wait"
