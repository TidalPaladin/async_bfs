CC := g++
CFLAGS := -std=c++11 -pthread

HS.exe : main.cpp
	$(CC) $(CFLAGS) -o $@ $^

.PHONY : clean
clean :
	rm -f *.exe
