#include<arpa/inet.h>
#include<fcntl.h>
#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<sys/socket.h>
#include<sys/stat.h>
#include<unistd.h>
#include<signal.h>

#define SERVER_IP "127.0.0.1"
#define SERVER_PORT 9000
#define COMMAND_LENGTH 256
#define WORD_SIZE 1024
#define RECV_LENGTH 2048
#define MAX_FILE_SIZE 5242880
#define SA(x) ((struct sockaddr*)x)
#define UP 1

#define CHUNK_SIZE 1048577

int num_ds() {
    FILE *fp;
    char c;
    int count = 0;
    fp = fopen("config.txt", "r");
    if (fp == NULL)
    {
        printf("Could not open config file");
        return 0;
    }
    for (c = getc(fp); c != EOF; c = getc(fp)) if (c == '\n') count = count + 1;
    fclose(fp);
    return count;
}

int main(int argc,char* argv[]) {
    if(argc!=2) {
        printf("Please enter : <command> <filename>\n");
        exit(1);
    }

    int cli_sock,serv_port,serv_sock;
    serv_port = atoi(argv[1]);
    struct sockaddr_in server_address,client_address;
    bzero(&server_address,sizeof(server_address));
    server_address.sin_family = AF_INET;

	server_address.sin_addr.s_addr = inet_addr(SERVER_IP);
	server_address.sin_port = htons(SERVER_PORT + serv_port); // listening on port 9000+i where i is given in argv[1]

    if((serv_sock = socket(AF_INET,SOCK_STREAM,0)) < 0) {
        perror("creating socket");
        exit(1);
    }

    bind(serv_sock,SA(&server_address),sizeof(server_address));
    listen(serv_sock,6);

    while(1) {
        int len = sizeof(client_address);
        int connfd = accept(serv_sock, SA(&client_address), &len);

        int pid = fork();

        if(pid == 0) {
            // connfd is connected to name server now
            char filename[COMMAND_LENGTH], otherfile[COMMAND_LENGTH];
            char chunk[CHUNK_SIZE],cmd_str[2],nstr[2],cn[2];
            int chunknum,cmd_typ;

            bzero(cmd_str,sizeof(cmd_str));
            read(connfd,cmd_str,sizeof(cmd_str));
            cmd_str[strlen(cmd_str)] = '\0';
            cmd_typ = atoi(cmd_str);
            printf("Command type : %d",cmd_typ);
                                
            char path[1024], otherpath[1024];
            char mkpath[1024];

            switch(cmd_typ) {
                case 1:
                    // read filename and a chunk
                    bzero(filename,COMMAND_LENGTH);
                    read(connfd, filename, COMMAND_LENGTH);
                    filename[strlen(filename)] = '\0';
                    printf("Filename : %s\n",filename);
                    
                    bzero(cn,sizeof(cn));
                    read(connfd,cn,sizeof(cn));
                    cn[strlen(cn)] = '\0';
                    printf("chunk num : %s\n",cn);

                    bzero(chunk,CHUNK_SIZE);
                    int bytes_read = read(connfd, chunk, CHUNK_SIZE);
                    chunk[strlen(chunk)] = '\0';
                    printf("%s\n",chunk);

                    sprintf(path, "ds%s/%s.%s", argv[1], filename, cn);
                    sprintf(mkpath, "ds%s", argv[1]);
                    mkdir(mkpath, 0777);

                    if(bytes_read<=0) break;

                    int file = open(path, O_CREAT | O_WRONLY, 0777);
                    write(file, chunk, strlen(chunk));
                    close(file);
                    break;
                case 2:
                    // read filename and a chunk
                    bzero(filename,COMMAND_LENGTH);
                    read(connfd, filename, COMMAND_LENGTH);
                    filename[strlen(filename)] = '\0';
                    printf("Filename : %s\n",filename);
                    
                    bzero(cn,sizeof(cn));
                    read(connfd,cn,sizeof(cn));
                    cn[strlen(cn)] = '\0';
                    printf("chunk num : %s\n",cn);

                    sprintf(path, "ds%s/%s.%s", argv[1], filename, cn);
                    printf("deleting %s\n", path);
                    printf("rem : %d\n", remove(path));
                    break;
                case 3:
                    bzero(filename,COMMAND_LENGTH);
                    read(connfd, filename, COMMAND_LENGTH);
                    filename[strlen(filename)] = '\0';
                    printf("Filename : %s\n",filename);
                    
                    bzero(cn,sizeof(cn));
                    read(connfd,cn,sizeof(cn));
                    cn[strlen(cn)] = '\0';
                    printf("chunk num : %s\n",cn);

                    sprintf(path, "ds%s/%s.%s", argv[1], filename, cn);
                    printf("catting Path : %s\n",path);

                    //int file2 = open(path,O_RDONLY);
                    
                    bzero(chunk,CHUNK_SIZE);
                    FILE *f = fopen(path, "r");
                    fseek(f, 0, SEEK_END);
                    long fsize = ftell(f);
                    fseek(f, 0, SEEK_SET);

                    //char *string = malloc(fsize + 1);
                    fread(chunk, 1, fsize, f);
                    fclose(f);
                    //chunk[fsize] = 0;
                    //int br = read(file2, chunk, CHUNK_SIZE);
                    //printf("bytes read : %d\n",br);
                    //chunk[strlen(chunk)] = '\0';
                    //printf("%s\n",chunk);
                    printf("%d -:- \n", fsize);

                    write(connfd,chunk,CHUNK_SIZE);
                    break;
                case 4: case 5:
                    // read filename and a chunk
                    bzero(filename,COMMAND_LENGTH);
                    read(connfd, filename, COMMAND_LENGTH);
                    filename[strlen(filename)] = '\0';
                    printf("Filename : %s\n",filename);
                    
                    bzero(otherfile,COMMAND_LENGTH);
                    read(connfd, otherfile, COMMAND_LENGTH);
                    otherfile[strlen(otherfile)] = '\0';
                    printf("Otherfile : %s\n",otherfile);

                    bzero(cn,sizeof(cn));
                    read(connfd,cn,sizeof(cn));
                    cn[strlen(cn)] = '\0';
                    printf("chunk num : %s\n",cn);

                    sprintf(path, "ds%s/%s.%s", argv[1], filename, cn);
                    sprintf(otherpath, "ds%s/%s.%s", argv[1], otherfile, cn);
                    printf("copying %s to %s\n", path, otherpath);
                    
                    FILE *f1, *f2;
                    f1 = fopen(path,"r");
                    f2 = fopen(otherpath,"w+");
                    char c;
                    while((c = getc(f1)) != EOF) putc(c,f2);
                    rewind(f1);
                    rewind(f2);

                    if (cmd_typ == 5) {
                        remove(path);
                    }

                    fclose(f1);
                    fclose(f2);
                    break;
                
                // case 7:
                //     kill(getppid(), SIGKILL);
                //     exit(0);
                //     break;
            }
        }
    }
    close(serv_sock);
}