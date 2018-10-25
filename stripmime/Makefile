include Makefile.inc
SOURCES = $(wildcard *.c)
OBJECTS = $(SOURCES:.c=.o)
DEPS = $(wildcard include/*.h)
STRIPMIME = stripmime

fromzero: clean all

all: $(STRIPMIME)

%.o: %.c $(DEPS)
	$(GCC) $(GCCFLAGS) -I./include -c $< -o $@  


$(STRIPMIME): $(DEPS) $(OBJECTS)
	$(GCC) -o $@ $(SOURCES) -I./include $(GCCFLAGS)


clean: 
	rm -rf *.o $(STRIPMIME)

.PHONY: all clean