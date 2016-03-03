all:json.o json.dll libjson.a

CC=gcc
	
json.o:
	$(CC) -c $(@:o=c) -o $@

json.dll:
	$(CC) -shared -fPIC $(@:dll=o) -static-libgcc -o $@
	
libjson.a:
	ar -rcs $@ $(@:a=0)

.PHONY:clean
clean:
	@rm -rf *.o *.dll *.exe	*.a