all: Ferry

Ferry: threaded_ferry.o
	gcc -o Ferry -pthread threaded_ferry.o

threaded_ferry.o: threaded_ferry.c
	gcc -c threaded_ferry.c

clean:
	rm Ferry*
	rm *.o
