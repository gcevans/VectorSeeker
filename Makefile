CXX = g++
CC = gcc
BUILDTEST = -O3 -vec-report3 -vec-threshold0 -inline-debug-info -g
PINPATH = ../pin
XEDPATH = $(PINPATH)/extras/xed2-intel64
INCDIR = -I$(PINPATH)/source/include/pin -I$(PINPATH)/source/include/pin/gen -I$(PINPATH)/extras/components/include -I$(PINPATH)/extras/xed2-intel64/include -I$(PINPATH)/source/tools/InstLib -I$(BOOST_ROOT)
LINKDIR1 = -L$(PINPATH)/extras/xed2-intel64/lib -L$(PINPATH)/pin/intel64/lib -L$(PINPATH)intel64/lib-ext
LINKDIR2 = -L$(PINPATH)/extras/xed2-intel64/lib -L$(PINPATH)/intel64/lib -L$(PINPATH)/intel64/lib-ext
LIBS = -lpin -lxed -ldwarf -lelf -ldl
COPTS1 = -g -std=gnu++0x -c -Wall -Werror -Wno-unknown-pragmas  -O3 -fomit-frame-pointer -DBIGARRAY_MULTIPLIER=1 -DUSING_XED  -fno-strict-aliasing
COPTS2 = -fno-stack-protector -DTARGET_IA32E -DHOST_IA32E -fPIC -DTARGET_LINUX -O3 -fomit-frame-pointer
LOPTS = -g -std=gnu++0x -Wl,--hash-style=sysv -shared -Wl,-Bsymbolic -Wl,--version-script=$(PINPATH)/source/include/pin/pintool.ver

all : tracer.so mintest deeploops

tracer.o : tracer.cpp tracer.h tracerlib.h tracer_decode.h shadow.h resultvector.h
	$(CXX) $(COPTS1) $(INCDIR) $(COPTS2) -o tracer.o tracer.cpp

tracer_decode.o : tracer_decode.cpp tracer_decode.h tracer.h
	$(CXX) $(COPTS1) $(INCDIR) $(COPTS2) -o tracer_decode.o tracer_decode.cpp

shadow.o : shadow.cpp shadow.h
	$(CXX) $(COPTS1) $(INCDIR) $(COPTS2) -o shadow.o shadow.cpp

resultvector.o : resultvector.cpp resultvector.h
	$(CXX) $(COPTS1) $(INCDIR) $(COPTS2) -o resultvector.o resultvector.cpp

tracer.so : tracer.o tracer_decode.o shadow.o resultvector.o
	$(CXX) $(LOPTS) $(LINKDIR1) -o tracer.so tracer.o tracer_decode.o shadow.o resultvector.o $(LINKDIR2) $(LIBS)
	
memmon.so : memmon.o
	$(CXX) $(LOPTS) $(LINKDIR1) -o memmon.so memmon.o $(LINKDIR2) $(LIBS)
	
memmon.o : memmon.cpp memmon.h
	$(CXX) $(COPTS1) $(INCDIR) $(COPTS2) -o memmon.o memmon.cpp

mintest : mintest.o dummy.o tracerlib.o
	$(CXX) -g -o mintest mintest.o dummy.o tracerlib.o
	
mintest.o : mintest.cpp dummy.h tracerlib.h
	$(CXX) -O1 -c -g -gdwarf-2 -o mintest.o mintest.cpp
	
deeploops : deeploops.o dummy.o tracerlib.o
	$(CXX) -g -o deeploops deeploops.o dummy.o tracerlib.o
	
deeploops.o : deeploops.c dummy.h tracerlib.h
	$(CC) -O1 -std=c99 -c -g -gdwarf-2 -o deeploops.o deeploops.c
	
deeploops-sol : deeploops-sol.o dummy.o tracerlib.o
	$(CXX) -g -o deeploops-sol deeploops-sol.o dummy.o tracerlib.o
	
deeploops-sol.o : deeploops-sol.c dummy.h tracerlib.h
	$(CC) -O1 -std=c99 -c -g -gdwarf-2 -o deeploops-sol.o deeploops-sol.c
	
buildtest.o : mintest.cpp dummy.h tracerlib.h
	$(CXX) $(BUILDTEST) -c -o mintest.o mintest.cpp
	
dummy.o : dummy.cpp dummy.h
	$(CC) -g -c -o dummy.o dummy.cpp

tracerlib.o : tracerlib.c tracerlib.h
	$(CC) -c -g -O0 -o tracerlib.o tracerlib.c
	
runtest : tracer.so mintest
	$(PINPATH)/pin.sh -injection child -t tracer.so -o mintest.log -- ./mintest

rundeeploops : tracer.so deeploops
	$(PINPATH)/pin.sh -injection child -t tracer.so -o deeploops.log -- ./deeploops

clean :
	rm -f *.o *.so mintest mintest.log deeploops deeploops.log deeploops-sol deeploops-sol.log
	
