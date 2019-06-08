
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


R2P2_DIR=../../rpchol/src/r2p2
SRCS_R2P2 := $(wildcard $(R2P2_DIR)/*.c)
OBJ_R2P2=$(patsubst %.c,%.o,$(SRCS_R2P2))
CFLAGS= -Wall -g -MD -O3 -I../inc -I$(R2P2_DIR)/inc -DLINUX
CXXFLAGS= $(CFLAGS) -std=c++11
LDFLAGS= -lm -lpthread

ifeq ($(DUMP_SAMPLES), 1)
		CFLAGS += -DDUMP_SAMPLES
endif

ifeq ($(QUALITY_EXP), 1)
		CFLAGS += -DQUALITY_EXP
endif

#ifeq ($(SINGLE_REQ), 1)
		#CFLAGS += -DSINGLE_REQ
#endif

ifeq ($(REQ_4), 1)
		CFLAGS += -DREQ_4
endif

TARGETS= agent

all: $(TARGETS)

#agent: agent.o manager.o args.o tp_tcp.o tp_r2p2.o rand_gen.o cpp_rand.o app_proto.o stats.o timestamping.o $(OBJ_R2P2)
#	g++ -o $@ $^ $(LDFLAGS)
agent: agent.o manager.o args.o tp_tcp.o rand_gen.o cpp_rand.o app_proto.o stats.o timestamping.o $(OBJ_R2P2)
	g++ -o $@ $^ $(LDFLAGS)

clean:
	rm -f *.o *.d

distclean:
	$(MAKE) clean
	rm -f $(TARGETS)
