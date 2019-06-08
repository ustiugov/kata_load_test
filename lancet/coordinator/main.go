
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

// #include "../inc/lancet/coord_proto.h"
import "C"
import (
	"fmt"
	"net"
	"os"
	"time"
//        "strings"
//        "strconv"
)

const (
	tROUGHPUT_AGENT = iota
	lATENCY_AGENT
)

type agent struct {
	name  string
	conn  *net.TCPConn
	aType int
}

func main() {

	serverCfg, expCfg := ParseConfig()

	c := coordinator{}

	if expCfg.thAgents != nil {
		c.thAgents = make([]*agent, len(expCfg.thAgents))
	}
	if expCfg.ltAgents != nil {
		c.ltAgents = make([]*agent, len(expCfg.ltAgents))
	}
	if expCfg.symAgents != nil {
		c.symAgents = make([]*agent, len(expCfg.symAgents))
	}
	c.agentPort = expCfg.agentPort

        /*// Start server with micro VMs
        s := strings.Split(serverCfg.target, ":")
        serverIP := s[0]
        vm_count := strconv.Itoa(len(s) - 1)
        serverArgs := fmt.Sprintf("%s", vm_count)
        fmt.Println("Server IP = %s, starting % VMs\n", serverIP, serverArgs)
        session, err_ := start_vms(serverIP, serverArgs)
        if err_ != nil {
            fmt.Println(err_)
            os.Exit(1)
        }
        defer session.Close()
        time.Sleep(20 * time.Second)
        */

        // Deploy throughput agents
	agentArgs := fmt.Sprintf("-s %s -t %d -c %d -i %s -p %s -r %s -a 0",
		serverCfg.target, serverCfg.thThreads, serverCfg.thConn,
		serverCfg.idist, serverCfg.comProto, serverCfg.appProto)

	for i, a := range expCfg.thAgents {
		session, err := deployAgent(a, expCfg.thBinary, agentArgs)
		if err != nil {
			fmt.Println(err)
			os.Exit(1)
		}
		defer session.Close()
		c.thAgents[i] = &agent{name: a, aType: tROUGHPUT_AGENT}
	}

	// Deploy latency agents
	ltArgs := fmt.Sprintf("-s %s -t %d -c %d -i %s -p %s -r %s -a 1",
		serverCfg.target, serverCfg.ltThreads, serverCfg.ltConn,
		serverCfg.idist, serverCfg.comProto, serverCfg.appProto)
	for i, a := range expCfg.ltAgents {
		session, err := deployAgent(a, expCfg.ltBinary, ltArgs)
		if err != nil {
			fmt.Println(err)
			os.Exit(1)
		}
		defer session.Close()
		c.ltAgents[i] = &agent{name: a, aType: lATENCY_AGENT}
	}

	var symType int
	if expCfg.nicTS {
		fmt.Println("NIC timestamping")
		symType = 2
	} else {
		fmt.Println("Userspace timestamping")
		symType = 3
	}
	symArgs := fmt.Sprintf("-s %s -t %d -c %d -i %s -p %s -r %s -a %d",
		serverCfg.target, serverCfg.thThreads, serverCfg.thConn,
		serverCfg.idist, serverCfg.comProto, serverCfg.appProto, symType)
	for i, a := range expCfg.symAgents {
		session, err := deployAgent(a, expCfg.thBinary, symArgs)
		if err != nil {
			fmt.Println(err)
			os.Exit(1)
		}
		defer session.Close()
		c.symAgents[i] = &agent{name: a, aType: lATENCY_AGENT}
	}

	time.Sleep(5000 * time.Millisecond)

	// Initialize management connections
	for _, a := range c.thAgents {
		tcpAddr, err := net.ResolveTCPAddr("tcp", fmt.Sprintf("%s:%d", a.name, c.agentPort))
		if err != nil {
			fmt.Println("ResolveTCPAddr failed:", err)
			os.Exit(1)

		}
		conn, err := net.DialTCP("tcp", nil, tcpAddr)
		if err != nil {
			fmt.Println("Dial failed:", err)
			os.Exit(1)

		}
		defer conn.Close()
		a.conn = conn
	}
	for _, a := range c.ltAgents {
		tcpAddr, err := net.ResolveTCPAddr("tcp", fmt.Sprintf("%s:%d", a.name, c.agentPort))
		if err != nil {
			fmt.Println("ResolveTCPAddr failed:", err)
			os.Exit(1)

		}
		conn, err := net.DialTCP("tcp", nil, tcpAddr)
		if err != nil {
			fmt.Println("Dial failed:", err)
			os.Exit(1)

		}
		defer conn.Close()
		a.conn = conn
	}
	for _, a := range c.symAgents {
		tcpAddr, err := net.ResolveTCPAddr("tcp", fmt.Sprintf("%s:%d", a.name, c.agentPort))
		if err != nil {
			fmt.Println("ResolveTCPAddr failed:", err)
			os.Exit(1)

		}
		conn, err := net.DialTCP("tcp", nil, tcpAddr)
		if err != nil {
			fmt.Println("Dial failed:", err)
			os.Exit(1)

		}
		defer conn.Close()
		a.conn = conn
	}

	// Run experiment
	err := c.runExp(expCfg.loadPattern, expCfg.ltRate, expCfg.ciSize)
	if err != nil {
		fmt.Println(err)
		os.Exit(1)
	}
}
