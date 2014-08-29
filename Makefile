# Copyright (c) 2013 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

#
# GNU Make based build file.  For details on GNU Make see:
# http://www.gnu.org/software/make/manual/make.html
#

#
# Get pepper directory for toolchain and includes.
#
# If NACL_SDK_ROOT is not set, then assume it can be found three directories up.
#
THIS_MAKEFILE := $(abspath $(lastword $(MAKEFILE_LIST)))
NACL_SDK_ROOT ?= $(abspath $(dir $(THIS_MAKEFILE))../..)

# Project Build flags
WARNINGS := -Wno-long-long -Wall -Wswitch-enum -pedantic -Werror
CXXFLAGS := -pthread -std=c++11 $(WARNINGS)

#
# Compute tool paths
#
GETOS := python $(NACL_SDK_ROOT)/tools/getos.py
OSHELPERS = python $(NACL_SDK_ROOT)/tools/oshelpers.py
OSNAME := $(shell $(GETOS))
RM := $(OSHELPERS) rm

PNACL_TC_PATH := $(abspath $(NACL_SDK_ROOT)/toolchain/$(OSNAME)_pnacl)
PNACL_CXX := $(PNACL_TC_PATH)/bin/pnacl-clang++
PNACL_FINALIZE := $(PNACL_TC_PATH)/bin/pnacl-finalize
CXXFLAGS := -I$(NACL_SDK_ROOT)/include -Ilzo -Iboost_1_56_0
LDFLAGS := -L$(NACL_SDK_ROOT)/lib/pnacl/Release -lppapi_cpp -lppapi -pthread

#
# Disable DOS PATH warning when using Cygwin based tools Windows
#
CYGWIN ?= nodosfilewarning
export CYGWIN


# Declare the ALL target first, to make the 'all' target the default build
all: hello_tutorial_decompress.pexe hello_tutorial_compress.pexe

clean:
	$(RM) hello_tutorial_decompress.pexe hello_tutorial_decompress.bc minilzo.o
minilzo.o: lzo/minilzo.c lzo/minilzo.h
	$(PNACL_CXX) -c lzo/minilzo.c -O3 $(CXXFLAGS)

hello_tutorial_decompress.bc: hello_tutorial_decompress.cc lzo/minilzo.h minilzo.o boost_1_56_0/boost/uuid/sha1.hpp 
	$(PNACL_CXX) -o hello_tutorial_decompress.bc minilzo.o -x c++ boost_1_56_0/boost/uuid/sha1.hpp hello_tutorial_decompress.cc -O2 -std=gnu++11 $(CXXFLAGS) $(LDFLAGS)
hello_tutorial_decompress.pexe: hello_tutorial_decompress.bc
	$(PNACL_FINALIZE) -o $@ $<

hello_tutorial_compress.bc: hello_tutorial_compress.cc lzo/minilzo.h minilzo.o boost_1_56_0/boost/uuid/sha1.hpp
	$(PNACL_CXX) -o hello_tutorial_compress.bc minilzo.o -x c++ boost_1_56_0/boost/uuid/sha1.hpp hello_tutorial_compress.cc -O2 -std=gnu++11 $(CXXFLAGS) $(LDFLAGS)
hello_tutorial_compress.pexe: hello_tutorial_compress.bc
	$(PNACL_FINALIZE) -o $@ $<

#
# Makefile target to run the SDK's simple HTTP server and serve this example.
#
HTTPD_PY := python $(NACL_SDK_ROOT)/tools/httpd.py

.PHONY: serve
serve: all
	$(HTTPD_PY) -C $(CURDIR)
