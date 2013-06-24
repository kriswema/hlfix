PROGNAME = hlfix
OBJECTS = main.o geo.o rmf.o cd.o map.o

GCC = g++

$(PROGNAME).exe: $(OBJECTS)
	$(GCC) -o $(PROGNAME) $(OBJECTS)

{$S}.cpp{$O}.o:
	$(GCC) -c -o $@ $<

main.o: rmf.h geo.h cd.h
rmf.o: rmf.h geo.h
geo.o: geo.h rmf.h
cd.o: rmf.h geo.h
map.o: geo.h

clean:
	@rm -fRv $(OBJECTS)
