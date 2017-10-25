CC=g++
CFLAGS=-lstd++ -c

oss: oss.o
	$(CC) oss.o -o oss

oss.o: oss.cpp
	$(CC) $(CFLAGS) oss.cpp

.PHONY: clean

clean:
	rm -f *.o
	rm -f oss
	rm -f *.txt
	rm -f *.out
	rm -f user
