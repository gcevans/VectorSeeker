CXX = g++
CC = gcc
BUILDTEST = -O3 -vec-report3 -vec-threshold0 -inline-debug-info -g
PINPATH = ../pin
#uncomment to build thread safe version of vectorseeker
#THREADED = -DTHREADSAFE
#uncomment to use old shadowmemory
#NOCACHESM = -DNOSHAODWCACHE
XEDNAME = xed-intel64
XEDPATH = $(PINPATH)/extras/$(XEDNAME)
INCDIR = -I$(PINPATH)/source/include/pin -I$(PINPATH)/source/include/pin/gen -I$(PINPATH)/extras/components/include -I$(PINPATH)/extras/$(XEDNAME)/include -I$(PINPATH)/source/tools/InstLib
LINKDIR1 = -L$(PINPATH)/intel64/runtime/cpplibs -L$(PINPATH)/extras/$(XEDNAME)/lib -L$(PINPATH)/pin/intel64/lib -L$(PINPATH)intel64/lib-ext -L$(PINPATH)/intel64/runtime/glibc
LINKDIR2 = -L$(PINPATH)/extras/$(XEDNAME)/lib -L$(PINPATH)/intel64/lib -L$(PINPATH)/intel64/lib-ext
#LIBS = -lpin -lxed -ldwarf -lelf -ldl
LIBS = -lpin -lxed -lpindwarf -ldl
COPTS1 = -g -std=gnu++0x -c -Wall -Werror -Wno-unknown-pragmas  -O3 -fomit-frame-pointer $(THREADED) -DBIGARRAY_MULTIPLIER=1 -DUSING_XED  -fno-strict-aliasing
COPTS2 = -fno-stack-protector -DTARGET_IA32E -DHOST_IA32E -fPIC -DTARGET_LINUX -O3 -fomit-frame-pointer $(NOCACHESM)
LOPTS = -g -std=gnu++0x -Wl,--hash-style=sysv -shared -Wl,-Bsymbolic -Wl,--version-script=$(PINPATH)/source/include/pin/pintool.ver

all : tracer.so mintest deeploops mintest-nodebug

tracer.o : tracer.cpp tracer.h tracerlib.h tracer_decode.h shadow.h resultvector.h tracebb.h instructions.h output.h threads.h
	$(CXX) $(COPTS1) $(INCDIR) $(COPTS2) -o tracer.o tracer.cpp

tracer_decode.o : tracer_decode.cpp tracer_decode.h tracer.h instructions.h shadow.h
	$(CXX) $(COPTS1) $(INCDIR) $(COPTS2) -o tracer_decode.o tracer_decode.cpp

tracebb.o : tracebb.cpp tracer_decode.h tracer.h tracebb.h shadow.h instructions.h resultvector.h
	$(CXX) $(COPTS1) $(INCDIR) $(COPTS2) -o tracebb.o tracebb.cpp

shadow.o : shadow.cpp shadow.h
	$(CXX) $(COPTS1) $(INCDIR) $(COPTS2) -o shadow.o shadow.cpp

output.o : output.cpp output.h instructions.h tracer.h
	$(CXX) $(COPTS1) $(INCDIR) $(COPTS2) -o output.o output.cpp

threads.o : threads.cpp threads.h
	$(CXX) $(COPTS1) $(INCDIR) $(COPTS2) -o threads.o threads.cpp

resultvector.o : resultvector.cpp resultvector.h
	$(CXX) $(COPTS1) $(INCDIR) $(COPTS2) -o resultvector.o resultvector.cpp

tracer.so : tracer.o tracer_decode.o shadow.o resultvector.o tracebb.o output.o threads.o
	$(CXX) $(LOPTS) $(LINKDIR1) -o tracer.so tracer.o tracer_decode.o shadow.o resultvector.o output.o threads.o tracebb.o $(LINKDIR2) $(LIBS)
	
mintest : mintest.o dummy.o tracerlib.o
	$(CXX) -g -o mintest mintest.o dummy.o tracerlib.o

mintest-nodebug : mintest-nodebug.o dummy.o tracerlib.o
	$(CXX) -o mintest-nodebug mintest-nodebug.o dummy.o tracerlib.o

mintest-nodebug.o : mintest.cpp dummy.h tracerlib.h
	$(CXX) -O1 -c -o mintest-nodebug.o mintest.cpp
	
mintest.o : mintest.cpp dummy.h tracerlib.h
	$(CXX) -O1 -g -c -gdwarf-2 -o mintest.o mintest.cpp
	
deeploops : deeploops.o dummy.o tracerlib.o
	$(CXX) -g -O0 -o deeploops deeploops.o dummy.o tracerlib.o
	
deeploops.o : deeploops.c dummy.h tracerlib.h
	$(CC) -O1 -std=c99 -c -g -gdwarf-2 -o deeploops.o deeploops.c
	
deeploops-sol : deeploops-sol.o dummy.o tracerlib.o
	$(CXX) -g -o deeploops-sol deeploops-sol.o dummy.o tracerlib.o
	
deeploops-sol.o : deeploops-sol.c dummy.h tracerlib.h
	$(CC) -O1 -std=c99 -c -g -gdwarf-2 -o deeploops-sol.o deeploops-sol.c
	
buildtest.o : mintest.cpp dummy.h tracerlib.h
	$(CXX) $(BUILDTEST) -c -o mintest.o mintest.cpp
	
dummy.o : dummy.cpp dummy.h
	$(CC) -c -o dummy.o dummy.cpp

tracerlib.o : tracerlib.c tracerlib.h
	$(CC) -c -O0 -o tracerlib.o tracerlib.c
	
runtest : tracer.so mintest
	setarch `uname -m` -R $(PINPATH)/pin.sh -injection child -t tracer.so -o mintest.log -- ./mintest

rundeeploops : tracer.so deeploops
	setarch `uname -m` -R $(PINPATH)/pin.sh -injection child -t tracer.so -o deeploops.log -- ./deeploops

rundeepbbloops : tracer.so deeploops
	setarch `uname -m` -R $(PINPATH)/pin.sh -injection child -t tracer.so -o deeploops.log -bb -- ./deeploops

clean :
	rm -f *.o *.so mintest mintest.log deeploops deeploops.log deeploops-sol deeploops-sol.log mintest-nodebug
	
