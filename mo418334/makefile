PROGRAMS = screen-worms-server screen-worms-client
CXX = g++
CFLAGS = -Wall -Wextra -g -O2 -std=c++17

all: $(PROGRAMS)

CLIENT_SOURCES = client/screen-worms-client.cpp
SERVER_SOURCES = server/screen-worms-server.cpp server/game_manager.cpp
COMMON = common/const.h common/events.h common/exceptions.h common/messages.h
UTILS = utils/id_manager.h utils/rng.h utils/timer.h utils/util_func.h utils/util_func.cpp

screen-worms-server: $(SERVER_SOURCES) $(COMMON) $(UTILS)
	$(CXX) $(CFLAGS) -o $@ $^

screen-worms-client: $(CLIENT_SOURCES) $(COMMON) $(UTILS)
	$(CXX) $(CFLAGS) -o $@ $^

.PHONY: all clean

clean:
	rm -rf $(PROGRAMS) *.o
