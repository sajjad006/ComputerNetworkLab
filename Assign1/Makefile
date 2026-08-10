CXX = g++
CXXFLAGS = -std=c++17 -O2 -Wall
COMMON = common.cpp error_injector.cpp framing.cpp

all: sender receiver driver

sender: sender.cpp $(COMMON)
	$(CXX) $(CXXFLAGS) -o sender sender.cpp $(COMMON)

receiver: receiver.cpp $(COMMON)
	$(CXX) $(CXXFLAGS) -o receiver receiver.cpp $(COMMON)

driver: driver.cpp $(COMMON)
	$(CXX) $(CXXFLAGS) -o driver driver.cpp $(COMMON)

clean:
	rm -f sender receiver driver results.csv rx*.txt
