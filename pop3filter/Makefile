include Makefile.inc
SOURCES = $(wildcard *.c)
OBJECTS = $(SOURCES:.c=.o)
DEPS = $(wildcard include/*.h)
POP3FILTER = pop3filter

fromzero: clean all

all: $(POP3FILTER)

%.o: %.c $(DEPS)
	$(GCC) $(GCCFLAGS) -I./include -c $< -o $@  


$(POP3FILTER): $(DEPS) $(OBJECTS)
	$(GCC) -o $@ $(SOURCES) -I./include $(GCCFLAGS)


clean: 
	rm -rf *.o $(POP3FILTER)

.PHONY: all clean