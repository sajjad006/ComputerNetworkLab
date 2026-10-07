CXX = g++
CXXFLAGS = -std=c++17 -O2 -Wall

all: channel_server station

channel_server: channel_server.cpp common/protocol.h common/net_util.h
	$(CXX) $(CXXFLAGS) -o channel_server channel_server.cpp

station: station.cpp common/protocol.h common/net_util.h
	$(CXX) $(CXXFLAGS) -o station station.cpp

clean:
	rm -f channel_server station results/*.csv
