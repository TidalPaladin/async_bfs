CC := g++
CFLAGS := -std=c++11 -pthread -g3

BFS : main.cpp
	$(CC) $(CFLAGS) -o $@ $^

.PHONY : clean
clean :
	rm -f *.exe
