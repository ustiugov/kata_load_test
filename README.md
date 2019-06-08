# This is a pre-release of the code to help the community test kata containers

## Instructions
To perform load testing, one needs one server and two client machines.

### Client side
* Allow all users to execute commands with `sudo`.

* Run setup\_all.h to install Go and other necessary components.

* Customize the scripts in `scripts` folder, setting IP addresses of the server and the clients in `fabfile.py`.
Customize `driver.py` to perform the experiment of your choice. 

### Server side
* Install Kata Containers and docker.

* Server code is located under `lancet/servers`. Build by invoking `make`.

### Run the experiment
```
cd scripts/
python ./driver.py
```

The coordinator will run the experiment, coordinating the latency and load clients, 
applying the necessary load on the server that will run a TCP server(s) inside the micro VM(s)
and output the results on the screen:

```
[icnals12] out: Userspace timestamping
[icnals12] out: Will run for 5 sec
[icnals12] out: #ReqCount       QPS     RxBw    TxBw
[icnals12] out: 60016   12002.451047054663      96019.6083764373        95994.00997377763
[icnals12] out: Check inter-arrival: []
[icnals12] out: #Avg Lat        50th    90th    95th    99th
[icnals12] out: 484.469 461.485(459.279, 463.854)       639.896(631.995, 646.651)       704.784(697.147, 715.533)       881.358(867.431, 902.256)
[icnals12] out: 
[icnals12] out: Will run for 5 sec
[icnals12] out: #ReqCount       QPS     RxBw    TxBw
[icnals12] out: 315114  63019.0062558234        504152.0500465872       640385.4487959825
[icnals12] out: Check inter-arrival: []
[icnals12] out: #Avg Lat        50th    90th    95th    99th
[icnals12] out: 142035.165      26908.048(22986.477, 40224.741) 434066.352(258925.123, 438494.125)      438793.823(435180.487, 861523.914)      863186.074(856115.894, 0)
```

The first part of the output says that 12002 QPS throughput was reached with latencies 484usec (avg) to 881usec (99-th percentile). Second part says 63019 QPS and avg latency of 142035usec.
