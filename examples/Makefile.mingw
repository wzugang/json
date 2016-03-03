all:testd test

testd:test.c
	$(CC) -o $@ $(@:d=.c) ..\json.dll
test:test.c
	$(CC) -o $@ $@.c ..\json.a
	
.PHONY:clean
clean:
	@rm -rf *.o *.dll *.exe	