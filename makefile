OBJS	 = main.o macioserial.o macscsi.o macswim3.o mpc106.o openpic.o poweropcodes.o  \
           ppcfpopcodes.o ppcgekkoopcodes.o ppcmemory.o ppcopcodes.o viacuda.o davbus.o \
           debugger.o

OUT	     = dingusppc
CXX	     = g++
CXXFLAGS = -g -c -Wall -std=c++11
LFLAGS	 =

all: $(OBJS)
	$(CXX) -g $(OBJS) -o $(OUT) $(LFLAGS)

*.o : *.cpp
	$(CXX) $(CXXFLAGS) -c $(input) -o $(output)

clean:
	rm -f *.o $(OUT)
