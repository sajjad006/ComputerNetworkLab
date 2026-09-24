CXX = g++
CXXFLAGS = -std=c++17 -O2 -Wall
COMMON = common/common.cpp common/error_injector.cpp common/frame.cpp common/channel.cpp common/rtt_timer.cpp

all: sw_sender sw_receiver gbn_sender gbn_receiver sr_sender sr_receiver

sw_sender: stop_and_wait/sender.cpp $(COMMON)
	$(CXX) $(CXXFLAGS) -o sw_sender stop_and_wait/sender.cpp $(COMMON)

sw_receiver: stop_and_wait/receiver.cpp $(COMMON)
	$(CXX) $(CXXFLAGS) -o sw_receiver stop_and_wait/receiver.cpp $(COMMON)

gbn_sender: go_back_n/sender.cpp $(COMMON)
	$(CXX) $(CXXFLAGS) -o gbn_sender go_back_n/sender.cpp $(COMMON)

gbn_receiver: go_back_n/receiver.cpp $(COMMON)
	$(CXX) $(CXXFLAGS) -o gbn_receiver go_back_n/receiver.cpp $(COMMON)

sr_sender: selective_repeat/sender.cpp $(COMMON)
	$(CXX) $(CXXFLAGS) -o sr_sender selective_repeat/sender.cpp $(COMMON)

sr_receiver: selective_repeat/receiver.cpp $(COMMON)
	$(CXX) $(CXXFLAGS) -o sr_receiver selective_repeat/receiver.cpp $(COMMON)

clean:
	rm -f sw_sender sw_receiver gbn_sender gbn_receiver sr_sender sr_receiver rx_*.txt results/*.csv
