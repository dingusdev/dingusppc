OBJS	= main.o macioserial.o macscsi.o macswim3.o mpc106.o openpic.o poweropcodes.o  \
          ppcfpopcodes.o ppcgekkoopcodes.o ppcmemory.o ppcopcodes.o viacuda.o davbus.o \
          debugger.o addressmap.o
SOURCE	= main.cpp macioserial.cpp macscsi.cpp macswim3.cpp mpc106.cpp openpic.cpp \
          poweropcodes.cpp ppcfpopcodes.cpp ppcgekkoopcodes.cpp ppcmemory.cpp      \
          ppcopcodes.cpp viacuda.cpp davbus.cpp debugger.cpp addressmap.cpp
HEADER	= macioserial.h macscsi.h macswim3.h mpc106.h openpic.h ppcemumain.h ppcmemory.h \
          viacuda.h debugger.h addressmap.h
OUT	    = dingusppc
CC	    = g++
FLAGS	= -g -c -Wall -std=c++11
LFLAGS	=

all: $(OBJS)
	$(CC) -g $(OBJS) -o $(OUT) $(LFLAGS)

main.o: main.cpp
	$(CC) $(FLAGS) main.cpp

macioserial.o: macioserial.cpp
	$(CC) $(FLAGS) macioserial.cpp

macscsi.o: macscsi.cpp
	$(CC) $(FLAGS) macscsi.cpp

macswim3.o: macswim3.cpp
	$(CC) $(FLAGS) macswim3.cpp

mpc106.o: mpc106.cpp
	$(CC) $(FLAGS) mpc106.cpp

openpic.o: openpic.cpp
	$(CC) $(FLAGS) openpic.cpp

poweropcodes.o: poweropcodes.cpp
	$(CC) $(FLAGS) poweropcodes.cpp

ppcfpopcodes.o: ppcfpopcodes.cpp
	$(CC) $(FLAGS) ppcfpopcodes.cpp

ppcgekkoopcodes.o: ppcgekkoopcodes.cpp
	$(CC) $(FLAGS) ppcgekkoopcodes.cpp

ppcmemory.o: ppcmemory.cpp
	$(CC) $(FLAGS) ppcmemory.cpp

ppcopcodes.o: ppcopcodes.cpp
	$(CC) $(FLAGS) ppcopcodes.cpp

viacuda.o: viacuda.cpp
	$(CC) $(FLAGS) viacuda.cpp

davbus.o: davbus.cpp
	$(CC) $(FLAGS) davbus.cpp

debugger.o: debugger.cpp
	$(CC) $(FLAGS) debugger.cpp

addressmap.o: addressmap.cpp
	$(CC) $(FLAGS) addressmap.cpp

clean:
	rm -f $(OBJS) $(OUT)
