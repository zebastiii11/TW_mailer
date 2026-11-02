CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -pedantic -g

.PHONY: all clean

all: twmailer-client twmailer-server

twmailer-client: twmailer-client.cpp socket_utils.cpp socket_utils.h
	$(CXX) $(CXXFLAGS) -o twmailer-client twmailer-client.cpp socket_utils.cpp

twmailer-server: twmailer-server.cpp socket_utils.cpp socket_utils.h
	$(CXX) $(CXXFLAGS) -o twmailer-server twmailer-server.cpp socket_utils.cpp

clean:
	rm -f twmailer-client twmailer-server