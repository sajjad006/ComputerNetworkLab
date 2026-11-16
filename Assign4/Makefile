CXX = g++
CXXFLAGS = -std=c++17 -O2 -Wall

HDRS = common/protocol.h common/net_util.h common/walsh.h

all: channel_server sender receiver

channel_server: channel_server.cpp $(HDRS)
	$(CXX) $(CXXFLAGS) -o channel_server channel_server.cpp

sender: sender.cpp $(HDRS)
	$(CXX) $(CXXFLAGS) -o sender sender.cpp

receiver: receiver.cpp $(HDRS)
	$(CXX) $(CXXFLAGS) -o receiver receiver.cpp

clean:
	rm -f channel_server sender receiver results/*.csv
