
//Open Source License.
//
//Copyright 2019 Ecole Polytechnique Federale Lausanne (EPFL)
//
//Permission is hereby granted, free of charge, to any person obtaining a copy
//of this software and associated documentation files (the "Software"), to deal
//in the Software without restriction, including without limitation the rights
//to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
//copies of the Software, and to permit persons to whom the Software is
//furnished to do so, subject to the following conditions:
//
//The above copyright notice and this permission notice shall be included in
//all copies or substantial portions of the Software.
//
//THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
//IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
//FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
//AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
//LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
//OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
//THE SOFTWARE.


package main

import (
	"flag"
	"strings"
)

type ServerConfig struct {
	target    string
	thThreads int
	ltThreads int
	thConn    int
	ltConn    int
	idist     string
	appProto  string
	comProto  string
	keyCount  int
}

type ExperimentConfig struct {
	thAgents    []string
	ltAgents    []string
	symAgents   []string
	agentPort   int
	thBinary    string
	ltBinary    string
	ltRate      int
	loadPattern string
	ciSize      int
	nicTS       bool
}

func ParseConfig() (*ServerConfig, *ExperimentConfig) {
	var agentPort = flag.Int("agentPort", 5001, "listening port of the agent")
	var target = flag.String("targetHost", "127.0.0.1:8000", "host:port comma-separated list to run experiment against")
	var thAgents = flag.String("loadAgents", "", "ip of loading agents separated by commas, e.g. ip1,ip2,...")
	var ltAgents = flag.String("ltAgents", "", "ip of latency agents separated by commas, e.g. ip1,ip2,...")
	var symAgents = flag.String("symAgents", "", "ip of latency agents separated by commas, e.g. ip1,ip2,...")
	var thBinary = flag.String("loadBinary", "", "path of the load agent binary")
	var ltBinary = flag.String("ltBinary", "", "path of the latency agent binary")
	var thThreads = flag.Int("loadThreads", 16, "loading threads per agent")
	var ltThreads = flag.Int("ltThreads", 8, "latency threads per agent")
	var thConn = flag.Int("loadConn", 256, "number of loading connections per agent")
	var ltConn = flag.Int("ltConn", 256, "number of latency connections")
	var idist = flag.String("idist", "exp", "interarrival distibution: fixed, exp")
	var appProto = flag.String("appProto", "bmc_fixed:19_fixed:2_1000000_0.998", "application proto: echo:<#bytes>, bmc_<key_gen>_<val_gen>_<key_count>_<rw_ratio>, synthetic:<rand_gen>:<avg>")
	var comProto = flag.String("comProto", "TCP", "TCP|R2P2")
	var ltRate = flag.Int("lqps", 16000, "throughput qps")
	var loadPattern = flag.String("loadPattern", "step:10000:100000:50000", "load pattern fixed:load|step:start:end:step")
	var ciSize = flag.Int("ciSize", 5, "size of 95-confidence interval in us")
	var keyCount = flag.Int("keyCount", 100000, "number of keys if appProto bmc")
	var nicTS = flag.Bool("nicTS", false, "NIC timestamping for symmetric agents")

	flag.Parse()

	serverCfg := &ServerConfig{}
	expCfg := &ExperimentConfig{}

	serverCfg.target = *target
	serverCfg.thThreads = *thThreads
	serverCfg.ltThreads = *ltThreads
	serverCfg.thConn = *thConn
	serverCfg.ltConn = *ltConn
	serverCfg.idist = *idist
	serverCfg.appProto = *appProto
	serverCfg.comProto = *comProto
	serverCfg.keyCount = *keyCount

	if *thAgents == "" {
		expCfg.thAgents = nil
	} else {
		expCfg.thAgents = strings.Split(*thAgents, ",")
	}
	if *ltAgents == "" {
		expCfg.ltAgents = nil
	} else {
		expCfg.ltAgents = strings.Split(*ltAgents, ",")
	}
	if *symAgents == "" {
		expCfg.symAgents = nil
	} else {
		expCfg.symAgents = strings.Split(*symAgents, ",")
	}
	expCfg.agentPort = *agentPort
	expCfg.thBinary = *thBinary
	expCfg.ltBinary = *ltBinary
	expCfg.ltRate = *ltRate
	expCfg.loadPattern = *loadPattern
	expCfg.ciSize = *ciSize
	expCfg.nicTS = *nicTS

	return serverCfg, expCfg
}
