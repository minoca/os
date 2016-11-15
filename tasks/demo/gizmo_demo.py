##
## Copyright (c) 2014 Minoca Corp.
##
##    This file is licensed under the terms of the GNU General Public License
##    version 3. Alternative licensing terms are available. Contact
##    info@minocacorp.com for details. See the LICENSE file at the root of this
##    project for complete licensing information..
##
## Script Name:
##
##     gizmo_demo.py
##
## Abstract:
##
##     This script runs the Gizmo 2 demo that performs various activities in
##     a loop.
##
## Author:
##
##     Evan Green 27-Jan-2015
##
## Environment:
##
##     Python
##

from BaseHTTPServer import BaseHTTPRequestHandler, HTTPServer
import ctypes
import datetime
import math
import os
import random
import stat
import subprocess
import time

html_begin = """
<html>
<head>
<style media="screen" type="text/css">
html {
  font-family: sans-serif;
  background-color: #f8f8f8;
}

.btn {
  display: block;
  padding: 6px 12px;
  margin-top: 20px;
  margin-bottom: 20px;
  margin-left: 30px;
  margin-right: 30px;
  font-size: 30px;
  font-weight: normal;
  line-height: 1.42857143;
  text-align: center;
  white-space: nowrap;
  vertical-align: middle;
  cursor: pointer;
  -webkit-user-select: none;
     -moz-user-select: none;
      -ms-user-select: none;
          user-select: none;
  background-image: none;
  border: 1px solid transparent;
  border-radius: 4px;
  color: #241c3b;
  background-color: #C2D840;
  border-color: #A6BC1E;
  text-decoration: none;
}

.btn:focus,
.btn:active:focus,
.btn.active:focus {
  outline: thin dotted;
  outline: 5px auto -webkit-focus-ring-color;
  outline-offset: -2px;
}
.btn:hover,
.btn:focus {
  color: #fff;
  text-decoration: none;
}
.btn:active,
.btn.active {
  background-image: none;
  outline: 0;
  -webkit-box-shadow: inset 0 3px 5px rgba(0, 0, 0, .125);
          box-shadow: inset 0 3px 5px rgba(0, 0, 0, .125);
}

ul {
    list-style-type:none;
    padding-left: 0px;
}

</style>
</head>
<body>
"""
html_end = """
<br />
<br />
<script type="text/javascript">
var count = 30;
var redirect = "/activity/";
function countDown(){
    var timer = document.getElementById("timer");
    if(count > 0){
        count--;
        timer.innerHTML = "Calculating primes and redirecting <a href=/>home</a> in " + count + " seconds.";
        setTimeout("countDown()", 1000);
    }else{
        window.location.href = redirect;
    }
}
</script>
<span id="timer">
<script type="text/javascript">countDown();</script>
</span>
</body>
</html>
"""

index = """
<h1>Gizmo 2 + Minoca OS</h1>
<h2>Ten second whiz-bang activities:</h2>

<ul>
<li><a class="btn" href="/build_zlib/">Compile</a></li>
<li><a class="btn" href="/compress_file/">Compress</a></li>
<li><a class="btn" href="/calculate_primes/">Calculate Primes</a></li>
<li><a class="btn" href="/profile_test/">Real-time Profiling Challenge</a></li>
</ul>
"""

profile_page = """
<h1>Profiling Puzzler</h1>
<p>You've just built a new board that changes a traffic light via USB, but you
suspect your driver is taking longer than it should when sending I/O. So you
design a test to change the light rapidly, and enable Minoca's stack sampling
profiler.</p>
<p>Take a look at the profiling data collected on the touchscreen PC in the
lower left pane of the debugger. Can you find out where all the CPU time is
going in your "onering" driver?
</p>
<p>Hint: Follow the hit counts starting at AySysenterHandlerAsm until you find
a function that starts with "onering", then take a look at the corresponding
source.</p>
"""

compile_compare = """
<h2>Compare to Raspberry Pi:</h2>
<pre>
real 57.00
user 29.73
sys 8.12
</pre>

<p>
*Comparison run on Minoca OS r867. Results are averaged (mean) across 10 runs,
standard deviations 1.18, 0.0135, and 0.046.
</p>
"""

compress_compare = """
<h2>Compare to Raspberry Pi:</h2>
<pre>
real 168.65
user 115.15
sys 15.48
</pre>

<p>
*Comparison run on Minoca OS r867. Results are averaged (mean) across 5 runs,
standard deviations 8.57, 1.29, and 0.11.
</p>
"""

class MinocaHTTPRequestHandler(BaseHTTPRequestHandler):
    def do_GET(self):
        self.send_response(200)
        self.send_header('Content-Type', 'text-html')
        self.end_headers()
        self.wfile.write(html_begin)
        send_end = True
        if self.path == '/':
            self.wfile.write(index)

        elif self.path == '/activity/':
            self.run_activity()

        elif self.path == '/build_zlib/':
            self.run_compile()

        elif self.path == '/compress_file/':
            self.run_compress()

        elif self.path == '/calculate_primes/':
            self.run_calc_primes()

        elif self.path == '/profile_test/':
            self.run_profile_test()

        elif self.path == '/exit/':
            self.socket.close()

        else:
            self.send_error(404, 'File not found!!')
            send_end = False

        if send_end:
            self.wfile.write(html_end);

        return

    def run_activity(self):
        nth, start, prime, tdelta = calc_primes()
        self.wfile.write(index)
        self.wfile.write("<br /><br /><h2>Last Prime Calculation:</h2>")
        self.display_calc_primes(nth, start, prime, tdelta)
        return

    def run_compile(self):
        self.wfile.write("<h1>Compiling zlib</h1>")
        print("Compiling zlib...")
        args = ['time', 'sh', './build_zlib.sh']
        self.run_command(args)
        self.wfile.write(compile_compare)
        print("Done")
        return

    def run_compress(self):
        self.wfile.write("<h1>Compressing some files</h1>")
        print("Compressing some files...")
        args = ['time', 'sh', './tar_file.sh']
        self.run_command(args)
        self.wfile.write(compress_compare)
        print("Done")
        return

    def run_calc_primes(self):
        self.wfile.write("<h1>Calculating Prime Numbers</h1>")
        nth, start, prime, tdelta = calc_primes()
        self.display_calc_primes(nth, start, prime, tdelta)
        return

    def run_profile_test(self):
        print("Enabling kernel stack sampling")
        print("Changing light rapidly to aggregate samples.")
        minoca_set_profiling(True)
        t0 = datetime.datetime.now()
        t1 = t0
        i = 0
        while (t1 - t0).total_seconds() < 10:
            change_light(i)
            i += 1
            time.sleep(0.1)
            t1 = datetime.datetime.now()

        print("Disabling kernel stack sampling")
        minoca_set_profiling(False)
        self.wfile.write(profile_page)
        return

    def run_command(self, args):
        process = subprocess.Popen(args=args,
                                   stdout=subprocess.PIPE,
                                   stderr=subprocess.PIPE,
                                   shell=False,
                                   env=os.environ)

        out, err = process.communicate()
        result = "<pre>"
        result += "Process %d exited with status %d\n" % \
                 (process.pid, process.returncode)

        result += out
        if err:
            result += "stderr: " + err

        result += "</pre>"
        self.wfile.write(result)
        return

    def display_calc_primes(self, nth, start, prime, tdelta):
        result = "<p>The %dth prime number starting at %d is %d.</p>" % \
                 (nth, start, prime)

        result += "<p>Calculated in %f seconds.</p>" % tdelta
        self.wfile.write(result)
        return

def change_light(value):
    value = int(value) % 8
    os.system("./usbrelay %d" % value)

def calc_primes():
    start = random.randint(0, 10000)
    nth = random.randint(10000, 50000)
    t0 = datetime.datetime.now()
    prime = calc_nth_prime(start, nth)
    t1 = datetime.datetime.now()
    tdelta = (t1 - t0).total_seconds()
    return (nth, start, prime, tdelta)

def is_prime(num):
    for j in range(2,int(math.sqrt(num)+1)):
        if (num % j) == 0:
            return False
    return True

##
## This routine calculates the nth prime number starting a given number.
##

def calc_nth_prime(start, nth):
    i = 0
    update = random.randint(300, 2500)
    num = start
    print("Calculating %d primes starting at %d" % (nth, start))
    print("nth: prime")
    while i < nth:
        if is_prime(num):
            i += 1

            ##
            ## Periodically update the light.
            ##

            if i >= update:
                change_light(num / 4)
                print "%d: %d" % (i, num)
                update += random.randint(300, 2500)

            if i == nth:
                return num

        num += 1

    return 0

##
## Define some Minoca specific C constants.
##

SystemInformationSp = 6
SpInformationGetSetState = 1

SpGetSetStateOperationNone = 0
SpGetSetStateOperationOverwrite = 1
SpGetSetStateOperationEnable = 2
SpGetSetStateOperationDisable = 3

PROFILER_TYPE_FLAG_STACK_SAMPLING = 0x00000001L

STATUS_SUCCESS = 0L
STATUS_NOT_FOUND = -2L
STATUS_BUFFER_TOO_SMALL = -35L

class SP_GET_SET_STATE_INFORMATION(ctypes.Structure):
    _fields_ = [
        ("operation", ctypes.c_ulong),
        ("profilertypeflags", ctypes.c_ulong)
    ]

def minoca_set_profiling(enable):
    information = SP_GET_SET_STATE_INFORMATION()
    information.operation = SpGetSetStateOperationDisable
    information.profilertypeflags = PROFILER_TYPE_FLAG_STACK_SAMPLING;
    if enable:
        information.operation = SpGetSetStateOperationEnable

    information_pointer = ctypes.cast(ctypes.addressof(information),
                                      ctypes.c_char_p)

    size = ctypes.c_int(ctypes.sizeof(information))
    result = minoca_get_set_system_information(SystemInformationSp,
                                               SpInformationGetSetState,
                                               information_pointer,
                                               size,
                                               1)

    if result != STATUS_SUCCESS:
        print("Failed to get profiling: %d" % result)

    return

def minoca_get_set_system_information(c_type,
                                      c_subtype,
                                      c_buffer,
                                      c_size,
                                      c_set):

    libminocaos = ctypes.cdll.LoadLibrary("libminocaos.so")
    get_system_information = libminocaos.OsGetSetSystemInformation
    get_system_information.argtypes = [ctypes.c_int,
                                       ctypes.c_int,
                                       ctypes.c_char_p,
                                       ctypes.POINTER(ctypes.c_int),
                                       ctypes.c_int]

    get_system_information.restype = ctypes.c_ulong
    return get_system_information(c_type,
                                  c_subtype,
                                  c_buffer,
                                  ctypes.byref(c_size),
                                  c_set)

def run():
    random.seed()
    print('Firing up HTTP Server...')
    server_address = ('', 8000)
    httpd = HTTPServer(server_address, MinocaHTTPRequestHandler)
    print('HTTP Server is running...')
    httpd.serve_forever()

if __name__ == '__main__':

    ##
    ## Symlink /bin/time in case I forget.
    ##

    if (not os.path.exists('/bin/time')) and (os.path.exists('/bin/swiss')):
        os.symlink('swiss', '/bin/time')

    run()
