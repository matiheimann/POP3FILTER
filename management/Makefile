include Makefile.inc
SOURCES = $(wildcard *.c)
OBJECTS = $(SOURCES:.c=.o)
DEPS = $(wildcard include/*.h)
MANAGEMENT = management

fromzero: clean all

all: $(MANAGEMENT)

%.o: %.c $(DEPS)
	$(GCC) $(GCCFLAGS) -I./include -c $< -o $@  


$(MANAGEMENT): $(DEPS) $(OBJECTS)
	$(GCC) -o $@ $(SOURCES) -I./include $(GCCFLAGS)


clean: 
	rm -rf *.o $(MANAGEMENT)

.PHONY: all clean