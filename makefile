%.o: %.c %.h
	gcc -c $< -o $@

all: libksocket.a init user1 user2


libksocket.a: ksocket.o 
	ar rcs libksocket.a ksocket.o 

init: initksocket.c libksocket.a
	gcc initksocket.c -L. -lksocket -o init

user1: user1.c libksocket.a init
	gcc user1.c -L. -lksocket -o user1

user2: user2.c libksocket.a init
	gcc user2.c -L. -lksocket -o user2

run1: user1
	./user1 testfile.txt

run2: user2
	./user2 recieved.txt

clean:
	rm -f *.o libksocket.a user1 user2 init recieved.txt
