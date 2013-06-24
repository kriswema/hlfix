BINARIES_DIR = bin/
GLOBAL_BINARIES_DIR = /usr/bin/
PROGNAME = hlfix
OBJECTS = main.o geo.o rmf.o cd.o map.o

GCC = g++

{$S}.cpp{$O}.o:
	$(GCC) -c -o $@ $<

main.o: rmf.h geo.h cd.h
rmf.o: rmf.h geo.h
geo.o: geo.h rmf.h
cd.o: rmf.h geo.h
map.o: geo.h

all: $(OBJECTS)
	@mkdir -p $(BINARIES_DIR)
	$(GCC) -o $(BINARIES_DIR)$(PROGNAME) $(OBJECTS)
	@make clean

clean:
	@rm -fRv $(OBJECTS)

purge:
	@make clean
	@rm -fRv $(BINARIES_DIR)

install:
	mv $(BINARIES_DIR)* $(GLOBAL_BINARIES_DIR)
