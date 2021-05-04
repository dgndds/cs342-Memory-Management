all: libsbmemlib.a  app create_memory_sb destroy_memory_sb

libsbmemlib.a:  sbmemlib.c
	gcc -g -Wall -c sbmemlib.c -lrt -lm -lpthread
	ar -cvq libsbmemlib.a sbmemlib.o
	ranlib libsbmemlib.a

app: app.c
	gcc -g -Wall -o app app.c -L. -lsbmemlib -lrt -lm -lpthread

create_memory_sb: create_memory_sb.c
	gcc -g -Wall -o create_memory_sb create_memory_sb.c -L. -lsbmemlib -lrt -lm -lpthread

destroy_memory_sb: destroy_memory_sb.c
	gcc -g -Wall -o destroy_memory_sb destroy_memory_sb.c -L. -lsbmemlib -lrt -lm -lpthread

clean: 
	rm -fr *.o *.a *~ a.out  app sbmemlib.o sbmemlib.a libsbmemlib.a  create_memory_sb destroy_memory_sb
