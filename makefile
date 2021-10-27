CC = gcc
CFLAG = -g
TAR1 = oss
TAR2 = user
OBJ1 = oss.o
OBJ2 = user.o

all: $(TAR1) $(TAR2)

$(TAR1): $(OBJ1)
	$(CC) -o $(TAR1) $(OBJ1)

$(TAR2): $(OBJ2)
	$(CC) -o $(TAR2) $(OBJ2)

$(OBJ1): oss.c
	$(CC) $(CFLAG) -c oss.c shared.h

$(OBJ2): user.c
	$(CC) $(CFLAG) -c user.c shared.h

clean:
	rm -rf *.o *.log $(TAR1) $(TAR2)
