EXE	 := dingusppc
MODULES := . devices debugger
SRCS := $(foreach sdir,$(MODULES),$(wildcard $(sdir)/*.cpp))
#SRCS = $(wildcard *.cpp)
OBJS := $(patsubst %.cpp, %.o, $(SRCS))

CXX	     = g++
CXXFLAGS = -g -c -Wall -std=c++11
LFLAGS	 =

VPATH := devices

DEPS = $(OBJS:.o=.d)

all: $(EXE)

$(EXE) : $(OBJS)
	$(CXX) -o $@ $^

%.o : %.cpp
	$(CXX) $(CXXFLAGS) -MMD -o $@ $<

clean:
	rm -rf *.o *.d devices/*.o devices/*.d $(EXE)
	rm -rf *.o *.d debugger/*.o debugger/*.d $(EXE)

-include $(DEPS)
