all: pop3filter management mediatypesfilter

fromzero: clean all ; ./pop3filter

pop3filter:
	cd pop3filter; make all

management:
	cd management; make all

mediatypesfilter:
	cd mediatypesfilter; make all

clean:
	cd pop3filter; make clean
	cd management; make clean
	cd mediatypesfilter; make all

.PHONY: all clean fromzero pop3filter management
