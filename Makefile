CC = gcc
CFLAGS = -std=gnu17 -Wall -Wextra -Werror

OBJS = router.o main.o

all: router

router: $(OBJS)
	$(CC) $(CFLAGS) -o router $(OBJS)

router.o: router.c router.h
main.o: main.c router.h

clean:
	rm -f *.o

distclean:
	rm -f *.o router