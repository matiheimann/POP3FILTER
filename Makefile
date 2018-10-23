all: pop3filter management mediatypesfilter

fromzero: clean all ; ./pop3filter

pop3filter:
	cd pop3filter; make all

management:
	cd management; make all

stripmime:
	cd stripmime; make all

clean:
	cd pop3filter; make clean
	cd management; make clean
	cd stripmime; make clean

.PHONY: all clean fromzero pop3filter management stripmime
