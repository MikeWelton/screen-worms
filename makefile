PROGRAMS = screen-worms-server screen-worms-client
CXX = g++
CFLAGS = -Wall -Wextra -g -O2 -std=c++17

all: $(PROGRAMS)

screen-worms-server: screen-worms-server.o
	$(CXX) $(CFLAGS) -o $@ $^

screen-worms-client: screen-worms-client.o
	$(CXX) $(CFLAGS) -o $@ $^

screen-worms-server.o: screen-worms-server.cpp
	$(CXX) $(CFLAGS) -c $<

screen-worms-client.o: screen-worms-client.cpp
	$(CXX) $(CFLAGS) -c $<

.PHONY: all clean

clean:
	rm -rf $(PROGRAMS) *.o
