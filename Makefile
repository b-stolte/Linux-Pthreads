main:
	g++ -std=c++11 -pthread -O -o prodcon main.cpp 

debug:
	g++ -std=c++11 -pthread -g main.cpp
