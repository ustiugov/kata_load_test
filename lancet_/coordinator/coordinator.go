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
	"math"
	"strconv"
	"strings"
	"time"
)

type coordState int
type coordinator struct {
	thAgents     []*agent
	ltAgents     []*agent
	symAgents    []*agent
	agentPort    int
	samples      int
	state        coordState
	samplingRate float64
}

const (
	initialSamplingRate            = 20
	initialSamples                 = 10000
	samplesStep                    = 10000
	maxTries                       = 20
	waitForThroughput   coordState = 0
	waitForLatency      coordState = 1
	exit                coordState = 2
)

func (c *coordinator) testAsymPattern(loadRate, latencyRate int) error {
	// Start loading
	var err error
	if len(c.thAgents) > 0 {
		err = startLoad(c.thAgents, int(loadRate/len(c.thAgents)))
		if err != nil {
			return fmt.Errorf("Error setting load: %v\n", err)
		}
	}
	err = startLoad(c.ltAgents, int(latencyRate))
	if err != nil {
		return fmt.Errorf("Error setting load: %v\n", err)
	}

	// Wait
	time.Sleep(10 * time.Second)

	// Measure
	var latSamplingRate float64
	latPortion := 100.0 * float64(latencyRate) / float64(loadRate+latencyRate)
	if latPortion > float64(c.samplingRate) {
		latSamplingRate = 100.0 * float64(c.samplingRate) / latPortion
	} else {
		latSamplingRate = 100
	}
	err = startMeasure(append(c.thAgents, c.ltAgents...), c.samples, latSamplingRate)
	if err != nil {
		return fmt.Errorf("Error starting load: %v\n", err)
	}

	// Wait for experiment to run
	duration := int(math.Ceil(float64(c.samples) / (float64(latencyRate) * (float64(latSamplingRate) / 100.0))))
	fmt.Printf("Will run for %v sec\n", duration)
	time.Sleep(time.Duration(duration) * time.Second)

	throughputReplies, iaComp, e2 := reportThroughput(c.thAgents)
	if e2 != nil {
		return fmt.Errorf("Error getting throughput replies: %v\n", e2)
	}

	latencyReplies, _, _, _, e3 := reportLatency(c.ltAgents)
	if e3 != nil {
		return fmt.Errorf("Error getting latency replies: %v\n", e3)
	}

	// Report results
	for _, reply := range latencyReplies {
		latAgentThroughput := &reply.Th_data
		throughputReplies = append(throughputReplies, latAgentThroughput)
	}

	agg_throughput := computeStatsThroughput(throughputReplies)
	printThroughputStats(agg_throughput)
	fmt.Printf("Check inter-arrival: %v\n", iaComp)

	computeStatsLatency(latencyReplies)

	return nil
}

func (c *coordinator) testSymPattern(loadRate int) error {
	// Start loading
	perAgentLoad := int(loadRate / len(c.symAgents))
	err := startLoad(c.symAgents, perAgentLoad)
	if err != nil {
		return fmt.Errorf("Error setting load: %v\n", err)
	}

	// Wait
	time.Sleep(5 * time.Second)
	fmt.Printf("The sampling rate is %v\n", c.samplingRate)
	perAgentSampingRate := c.samplingRate //float64(len(c.symAgents)) * c.samplingRate
	fmt.Printf("Per agent sampling rate %v\n", perAgentSampingRate)
	fmt.Println("Start measure...")
	err = startMeasure(c.symAgents, c.samples, perAgentSampingRate)
	if err != nil {
		return fmt.Errorf("Error starting load: %v\n", err)
	}

	// Wait for experiment to run
	sps := (float64(perAgentLoad) * float64(perAgentSampingRate)) / 100.0
	duration := int(math.Ceil(float64(c.samples) / sps))
	fmt.Printf("Will run for %v sec\n", duration)
	time.Sleep(time.Duration(duration) * time.Second)

	latencyReplies, iaComp, convergence, correlations, e2 := reportLatency(c.symAgents)
	if e2 != nil {
		return fmt.Errorf("Error getting latency replies: %v\n", e2)
	}

	// Report results
	throughputReplies := make([]*C.struct_throughput_reply, 0)
	for _, reply := range latencyReplies {
		latAgentThroughput := &reply.Th_data
		throughputReplies = append(throughputReplies, latAgentThroughput)
	}

	time.Sleep(5 * time.Second)
	agg_throughput := computeStatsThroughput(throughputReplies)
	printThroughputStats(agg_throughput)

	agg_lat := computeStatsLatency(latencyReplies)
	fmt.Println("Aggregate latency")
	printLatencyStats(agg_lat)

	fmt.Printf("Result convergence: %v\n", convergence)
	fmt.Printf("Correlations for iidness: %v\n", correlations)
	fmt.Printf("IA Compliance?: %v\n", iaComp)

	return nil
}

func (c *coordinator) fixedSymPattern(loadRate, ciSize int) error {
	// Start loading
	perAgentLoad := int(loadRate / len(c.symAgents))
	perAgentSampingRate := float64(len(c.symAgents)) * c.samplingRate
	err := startLoad(c.symAgents, perAgentLoad)
	if err != nil {
		return fmt.Errorf("Error setting load: %v\n", err)
	}
	c.state = waitForThroughput

	// Wait
	time.Sleep(2 * time.Second)
	tryCount := 0
	expectedRPS := float64(loadRate)
	for tryCount < maxTries {
		switch c.state {
		case waitForThroughput:
			err := startMeasure(c.symAgents, c.samples, float64(len(c.symAgents))*c.samplingRate)
			if err != nil {
				return fmt.Errorf("Error starting load: %v\n", err)
			}

			fmt.Println("Trying throughput")
			// Wait
			time.Sleep(1 * time.Second)
			// Collect throughput
			throughputReplies, iaComp, e2 := reportThroughput(c.symAgents)
			if e2 != nil {
				return fmt.Errorf("Error getting throughput replies: %v\n", e2)
			}
			aggThroughput := computeStatsThroughput(throughputReplies)
			rps := getRPS(aggThroughput)
			fmt.Println(rps)
			fmt.Printf("Throughput should be between %v %v\n", 0.9*expectedRPS, 1.1*expectedRPS)

			// Check if throughput reached
			if rps > expectedRPS*1.1 || rps < expectedRPS*0.90 {
				tryCount += 1
				fmt.Println("Throughput is wrong")
				continue
			}

			fmt.Println(iaComp)
			// Check if IA is ok
			/*
				// Remove for latency vs throughput exp
					notOk := false
					fmt.Println(iaComp)
					for _, val := range iaComp {
						if val == 0 {
							notOk = true
							break
						}
					}
					if notOk {
						tryCount += 1
						fmt.Println("IA is wrong")
						continue
					}
			*/
			c.state = waitForLatency

		case waitForLatency:
			err := startMeasure(c.symAgents, c.samples, float64(len(c.symAgents))*c.samplingRate)
			if err != nil {
				return fmt.Errorf("Error starting load: %v\n", err)
			}

			sps := (float64(perAgentLoad) * float64(perAgentSampingRate)) / 100.0
			duration := int(math.Ceil(float64(c.samples)/sps)) + 1
			fmt.Printf("Will run for %v sec\n", duration)
			fmt.Printf("Sampling rate = %v\n", c.samplingRate)
			fmt.Printf("Number of samples = %v\n", c.samples)
			time.Sleep(time.Duration(duration) * time.Second)

			latencyReplies, iaComp, convergence, correlations, e2 := reportLatency(c.symAgents)
			if e2 != nil {
				return fmt.Errorf("Error getting latency replies: %v\n", e2)
			}

			fmt.Printf("Unhandled IA comp: %v\n", iaComp)

			// Check correlations
			fmt.Printf("Correlations for iidness: %v\n", correlations)
			notOk := false
			for _, val := range correlations {
				if val > 0.25 {
					c.samplingRate /= 2
					perAgentSampingRate = float64(len(c.symAgents)) * c.samplingRate
					notOk = true
					break
				}
			}
			if notOk {
				tryCount += 1
				continue
			}

			// Check convergence
			fmt.Printf("Result convergence: %v\n", convergence)
			notOk = false
			for _, val := range correlations {
				if val == 0 {
					c.samples += 10000
					notOk = true
					break
				}
			}
			if notOk {
				tryCount += 1
				continue
			}

			// Everything is ok so print results
			throughputReplies := make([]*C.struct_throughput_reply, 0)
			for _, reply := range latencyReplies {
				latAgentThroughput := &reply.Th_data
				throughputReplies = append(throughputReplies, latAgentThroughput)
			}

			agg_throughput := computeStatsThroughput(throughputReplies)
			printThroughputStats(agg_throughput)

			agg_lat := computeStatsLatency(latencyReplies)

			// Check intervals
			fmt.Printf("ciSize = %v\n", ciSize)
			if int(agg_lat.P99_k-agg_lat.P99_i) > (ciSize * 1000) {
				c.samples += 10000
				tryCount += 1
				continue
			}

			fmt.Println("Aggregate latency")
			printLatencyStats(agg_lat)

			c.state = exit
		case exit:
			return nil
		}

	}

	return fmt.Errorf("Exp Failure: Max tries reached\n")
}

func (c *coordinator) stepPattern(startLoad, endLoad, step, latencyRate, ciSize int) error {
	loadRate := startLoad
	for loadRate < endLoad {
		err := c.testAsymPattern(loadRate, latencyRate)
		if err != nil {
			return err
		}
		fmt.Println()
		loadRate += step
	}
	return nil
}

func (c *coordinator) runExp(pattern string, latencyRate, ciSize int) error {

	patternAgs := strings.Split(pattern, ":")
	c.samples = initialSamples
	c.samplingRate = initialSamplingRate

	if patternAgs[0] == "fixed" {
		if len(c.symAgents) == 0 {
			return fmt.Errorf("Fixed only supported for symmetric experiments\n")
		}
		loadRate, err := strconv.Atoi(patternAgs[1])
		if err != nil {
			return fmt.Errorf("Error parsing load\n")
		}
		return c.fixedSymPattern(loadRate, ciSize)
	} else if patternAgs[0] == "step" {
		var step, endLoad int
		startLoad, err := strconv.Atoi(patternAgs[1])
		if err != nil {
			return fmt.Errorf("Error parsing load\n")
		}
		step, err = strconv.Atoi(patternAgs[2])
		if err != nil {
			return fmt.Errorf("Error parsing load\n")
		}
		endLoad, err = strconv.Atoi(patternAgs[3])
		if err != nil {
			return fmt.Errorf("Error parsing load\n")
		}
		return c.stepPattern(startLoad, endLoad, step, latencyRate, ciSize)

	} else if patternAgs[0] == "test" {
		loadRate, err := strconv.Atoi(patternAgs[1])
		if err != nil {
			return fmt.Errorf("Error parsing load\n")
		}
		samples, err := strconv.Atoi(patternAgs[2])
		if err != nil {
			return fmt.Errorf("Error parsing samples\n")
		}
		c.samples = samples
		if len(c.symAgents) > 0 {
			return c.testSymPattern(loadRate)
		} else {
			return c.testAsymPattern(loadRate, latencyRate)
		}
	} else if patternAgs[0] == "corr" {
		loadRate, err := strconv.Atoi(patternAgs[1])
		if err != nil {
			return fmt.Errorf("Error parsing load\n")
		}
		samples, err := strconv.Atoi(patternAgs[2])
		if err != nil {
			return fmt.Errorf("Error parsing samples\n")
		}
		c.samples = samples
		smallRates := []float64{0.01, 0.05, 0.1, 0.5, 1}
		for _, r := range smallRates {
			c.samplingRate = float64(r)
			fmt.Printf("Sampling Rate = %v/100\n", c.samplingRate)
			c.testSymPattern(loadRate)
		}
		/*
			for i := 5; i <= 100; i += 5 {
				c.samplingRate = float64(i)
				fmt.Printf("Sampling Rate = %v/100\n", c.samplingRate)
				c.testSymPattern(loadRate)
			}
		*/
	} else {
		return fmt.Errorf("Unknown load pattern")
	}
	return nil
}
