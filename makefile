CC = gcc
CFLAG = -g

all: oss user

oss: oss.o
	$(CC) $(CFLAG) -o oss oss.o

user: user.o
	$(CC) $(CFLAG) -o user user.o

oss.o: oss.c
	$(CC) $(CFLAG) -c oss.c

user.o: user.c
	$(CC) $(CFLAG) -c user.c

clean:
	rm -rf *.o oss user
