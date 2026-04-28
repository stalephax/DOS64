# Dans programs/lib/Makefile
CXX = g++
CXXFLAGS = -ffreestanding -fno-exceptions -fno-rtti -nostdlib \
           -fno-pic -no-pie -mcmodel=small

SRCS = stdcalls.cpp stdstring.cpp math.cpp
OBJS = $(SRCS:.cpp=.o)

libdos64.lib: $(OBJS)
	ar rcs libdos64.lib $(OBJS)

%.lib: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) libdos64.lib