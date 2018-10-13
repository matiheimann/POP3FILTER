include Makefile.inc
SOURCES = $(wildcard *.c)
OBJECTS = $(SOURCES:.c=.o)
DEPS = $(wildcard include/*.h)
MEDIATYPESFILTER = mediatypesfilter

fromzero: clean all

all: $(MEDIATYPESFILTER)

%.o: %.c $(DEPS)
	$(GCC) $(GCCFLAGS) -I./include -c $< -o $@  


$(MEDIATYPESFILTER): $(DEPS) $(OBJECTS)
	$(GCC) -o $@ $(SOURCES) -I./include $(GCCFLAGS)


clean: 
	rm -rf *.o $(MEDIATYPESFILTER)

.PHONY: all clean