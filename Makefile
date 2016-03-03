all:json.o json.dll json.a

CC=gcc
	
json.o:
	$(CC) -c $(@:o=c) -o $@

json.dll:
	$(CC) -shared -fPIC $(@:dll=o) -static-libgcc -o $@
	
json.a:
	ar -rcs $@ $(@:a=o)

.PHONY:clean
clean:
	@rm -rf *.o *.dll *.exe	*.a