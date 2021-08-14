ifndef TOOL_OBJ
	e:= $(error TOOL_OBJ is not defined)
endif

.PHONY: debug run demos

PIN_ROOT = ../pin

CXX=g++

include mk/Intel.mk

TOOL_CXXFLAGS += -std=c++11 -g 

debug: TOOL_CXXFLAGS += -DDEBUG
debug: all

run: demos
	$(PIN_ROOT)/pin.sh -t obj-intel64/ropshadow.so -- ./samples/hello

demos:
	make -sC samples
