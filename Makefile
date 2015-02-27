all: backend frontend new_backend new_frontend

backend: backend.o ivshmem.o my_socket.o
	gcc -o backend backend.o ivshmem.o my_socket.o

frontend: frontend.o ivshmem.o my_socket.o
	gcc -o frontend frontend.o ivshmem.o my_socket.o

new_backend: new_backend.o ivshmem.o my_socket.o
	gcc -o new_backend new_backend.o ivshmem.o my_socket.o

new_frontend: new_frontend.o ivshmem.o my_socket.o
	gcc -o new_frontend new_frontend.o ivshmem.o my_socket.o

backend.o: backend.c
	gcc -c backend.c

frontend.o: frontend.c
	gcc -c frontend.c

new_backend.o: new_backend.c
	gcc -c new_backend.c

new_frontend.o: new_frontend.c
	gcc -c new_frontend.c

ivshmem.o: ivshmem.c ivshmem.h
	gcc -c ivshmem.c

my_socket.o: my_socket.c my_socket.h
	gcc -c my_socket.c

clean:
	rm -f *.o
	rm -f backend frontend new_backend new_frontend