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

#!/bin/bash -e

sudo apt-get update

# KVM
sudo apt-get install -y \
        linux-tools-generic linux-tools-$(uname -r) \
        pkg-config zip g++ zlib1g-dev unzip python binutils-gold gnupg \
	python-setuptools python-pip iperf3

#sudo apt-get install -y golang-1.10
pushd /tmp > /dev/null
sudo rm -rf /usr/local/go/
wget https://dl.google.com/go/go1.11.linux-amd64.tar.gz
sudo tar -xvf go1.11.linux-amd64.tar.gz
sudo mv go /usr/local
export GOROOT=/usr/local/go
export GOPATH=$HOME/go
export PATH=$GOPATH/bin:$GOROOT/bin:$PATH
source ~/.bash_profile
go version

go get github.com/pkg/sftp
go get golang.org/x/sys/unix
popd > /dev/null

# Install the orchestration python lib
pushd distbenchr > /dev/null
sudo python setup.py install
popd > /dev/null

pushd lancet > /dev/null
make
popd > /dev/null

# Build synthetic server
pushd lancet/servers > /dev/null
make
popd > /dev/null
