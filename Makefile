CC := g++
CFLAGS := -std=c++11 -pthread

BFS.exe : main.cpp
	$(CC) $(CFLAGS) -o $@ $^

.PHONY : clean
clean :
	rm -f *.exe
