all: multi_process_thread_server.c
	gcc -o serv multi_process_thread_server.c -lpthread -w
	gcc -o cli client.c -w

clean:
	rm serv
	rm cli
	rm names
