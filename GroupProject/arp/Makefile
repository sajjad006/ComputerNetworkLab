CXX = g++
CXXFLAGS = -std=c++17 -Wall -O2
LDFLAGS_WIN = -lws2_32
LDFLAGS_UNIX = -pthread

.PHONY: all clean windows linux mac

all:
	@echo "Use: make windows   OR   make linux   OR   make mac"

windows:
	$(CXX) $(CXXFLAGS) -o lan_hub.exe lan_hub.cpp $(LDFLAGS_WIN)
	$(CXX) $(CXXFLAGS) -o host.exe host.cpp $(LDFLAGS_WIN)
	@echo "Built lan_hub.exe and host.exe"

linux mac:
	$(CXX) $(CXXFLAGS) -o lan_hub lan_hub.cpp
	$(CXX) $(CXXFLAGS) -o host host.cpp $(LDFLAGS_UNIX)
	@echo "Built lan_hub and host"

clean:
	rm -f lan_hub host lan_hub.exe host.exe
