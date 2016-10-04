.phony all:
all: pman

pman: pman.c
	gcc pman.c -lreadline -lhistory -o pman

.PHONY clean:
	clean:
	-rm -rf *.o *.exe

