.phony all:
all: pman

pman: pman.c
	gcc pman.c -lreadline -lhistory -o rsi

.PHONY clean:
	clean:
	-rm -rf *.o *.exe

