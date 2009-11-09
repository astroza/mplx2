CC=gcc
CC_FLAGS=-fPIC
LINKER=ld
L_FLAGS=-shared
OBJ=mplx2.o
LIB=mplx2.so
INCLUDE=-I.
PREFIX=/usr

all: $(OBJ)
	$(LINKER) $(L_FLAGS) $(OBJ) -o $(LIB)

mplx2.o: mplx2.c
	$(CC) $(INCLUDE) $(CC_FLAGS) mplx2.c -c

clean:
	rm -f *.o $(LIB)
install: all
	cp mplx2.so $(PREFIX)/lib
	mkdir -p $(PREFIX)/include/mplx2
	cp mplx2.h $(PREFIX)/include/mplx2
