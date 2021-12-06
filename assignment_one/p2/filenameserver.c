#include<arpa/inet.h>
#include<fcntl.h>
#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<sys/socket.h>
#include<sys/stat.h>
#include<unistd.h>
#include<signal.h>
#include<dirent.h>

#define SERVER_IP "127.0.0.1"
#define SERVER_PORT 9000
#define COMMAND_LENGTH 256
#define WORD_SIZE 1024
#define RECV_LENGTH 2048
#define MAX_FILE_SIZE 5242880
#define SA(x) ((struct sockaddr*)x)
#define CHUNK_SIZE 1048577

int num_ds() {
    FILE *fp;
    char c;
    int count = 1;
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

char* substr(char* string, int start, int end) {
   if (end - start + 1 > strlen(string)) {
      return NULL;
   }
   int len = end + 2 - start;
   int i;
   char* sub = (char*) malloc (sizeof(char) * len);
   for (i = 0; i + start <= end; i++) {
      sub[i] = string[i + start];
   }
   sub[i] = '\0';
   return sub;
}

int main() {
    int cli_sock,serv_port,serv_sock;
    struct sockaddr_in server_address,client_address;
    bzero(&server_address,sizeof(server_address));
    server_address.sin_family = AF_INET;

	server_address.sin_addr.s_addr = inet_addr(SERVER_IP);
	server_address.sin_port = htons(SERVER_PORT);

    if((serv_sock = socket(AF_INET,SOCK_STREAM,0)) < 0) {
        perror("creating socket");
        exit(1);
    }

    bind(serv_sock,SA(&server_address),sizeof(server_address));
    listen(serv_sock,6);

    while(1) {
        int len = sizeof(client_address);
        printf("waiting on connect\n");
        int connfd = accept(serv_sock, SA(&client_address), &len);
        int pid = fork();
        if(pid==0) {
            while(1) {
                int i,cmd_typ;
                char filename[COMMAND_LENGTH],file_cont[MAX_FILE_SIZE],cmd_num[2], otherfile[COMMAND_LENGTH];
                
                bzero(cmd_num,sizeof(cmd_num));
                read(connfd,cmd_num,sizeof(cmd_num));
                cmd_num[strlen(cmd_num)] = '\0';
                printf("cmd num : %s\n",cmd_num);
                
                cmd_typ = atoi(cmd_num);

                if (cmd_typ != 6) {            
                    bzero(filename,COMMAND_LENGTH);
                    read(connfd,filename,COMMAND_LENGTH);
                    filename[strlen(filename)] = '\0';
                    printf("filename : %s\n",filename);
                }
                
                char chunk[CHUNK_SIZE];
                char response[MAX_FILE_SIZE];
                int filefd,ns = num_ds();
                int dsfd[ns];
                DIR *d;
                struct dirent *dir;
                char files[4096];
                //char filename[512];
                struct sockaddr_in data_server[ns];

                printf("No of servers : %d\n",ns);

                switch(cmd_typ) {
                    case 1: 
                        for(int i=0;i<ns;i++) {
                            bzero(&data_server[i],sizeof(data_server[i]));
                            data_server[i].sin_family = AF_INET;
                            inet_aton(SERVER_IP,&(data_server[i].sin_addr));
                            data_server[i].sin_port = htons(SERVER_PORT+i+1);
                            dsfd[i] = socket(AF_INET,SOCK_STREAM,0);
                            connect(dsfd[i],SA(&data_server[i]),sizeof(data_server[i]));
                            
                            char num[2];
                            write(dsfd[i], cmd_num, sizeof(cmd_num));
                            write(dsfd[i],filename,COMMAND_LENGTH);
                            printf("Connected to server number %d\n",i+1);
                        }
                        printf("%s\n",filename);
                        filefd = open(filename,O_RDONLY);
                        for(int i=0;i<ns;i++) {
                            char chunk[CHUNK_SIZE];
                            bzero(chunk,CHUNK_SIZE);
                            int bytes_read = read(filefd,chunk,CHUNK_SIZE);
                            chunk[bytes_read]='\0';
                            
                            printf("=================================\n%s\n",chunk);
                            if(bytes_read>0) {
                                char cn[2];
                                sprintf(cn,"%d",i+1);
                                write(dsfd[i],cn,sizeof(cn));
                                write(dsfd[i],chunk,CHUNK_SIZE);  
                                close(dsfd[i]);
                            }
                            else {
                                for(int i=0;i<ns;i++) close(dsfd[i]);
                                break;
                            }
                        }
                        for(int i=0;i<ns;i++) close(dsfd[i]);
                        break;
                    case 2:
                        for(int i=0;i<ns;i++) {
                            bzero(&data_server[i],sizeof(data_server[i]));
                            data_server[i].sin_family = AF_INET;
                            inet_aton(SERVER_IP,&(data_server[i].sin_addr));
                            data_server[i].sin_port = htons(SERVER_PORT+i+1);
                            dsfd[i] = socket(AF_INET,SOCK_STREAM,0);
                            connect(dsfd[i],SA(&data_server[i]),sizeof(data_server[i]));
                            
                            char num[2];
                            write(dsfd[i], cmd_num, sizeof(cmd_num));
                            write(dsfd[i],filename,COMMAND_LENGTH);
                            char cn[2];
                            sprintf(cn,"%d",i+1);
                            write(dsfd[i],cn,sizeof(cn));
                            printf("Connected to server number %d\n",i+1);
                        }
                        for(int i=0;i<ns;i++) close(dsfd[i]);
                        break;
                    case 3:
                        for(int i=0;i<ns;i++) {
                            bzero(&data_server[i],sizeof(data_server[i]));
                            data_server[i].sin_family = AF_INET;
                            inet_aton(SERVER_IP,&(data_server[i].sin_addr));
                            data_server[i].sin_port = htons(SERVER_PORT+i+1);
                            dsfd[i] = socket(AF_INET,SOCK_STREAM,0);
                            connect(dsfd[i],SA(&data_server[i]),sizeof(data_server[i]));
                            
                            char num[2];
                            write(dsfd[i], cmd_num, sizeof(cmd_num));
                            write(dsfd[i],filename,COMMAND_LENGTH);
                            printf("Connected to server number %d\n",i+1);
                        }
                        printf("%s\n",filename);
                        //filefd = open(filename,O_RDONLY);
                        

                        for(int i=0;i<ns;i++) {
                            char cn[2];
                            sprintf(cn,"%d",i+1);
                            write(dsfd[i],cn,sizeof(cn));
                            
                            char chunk[CHUNK_SIZE];
                            int bytes_read = read(dsfd[i],chunk,sizeof(chunk));
                            write(connfd,chunk,CHUNK_SIZE);
                            printf("%d : bytes read : %d from %s\n",i+1,bytes_read, cn);

                            bytes_read = 0;
                            bzero(chunk,CHUNK_SIZE);
                        }
                        
                        //response[strlen(response)] = '\0';
                        //printf("strlen of resp : %d",strlen(response));
                        for(int i=0;i<ns;i++) close(dsfd[i]);
                        break;

                    case 4:
                        bzero(otherfile,COMMAND_LENGTH);
                        read(connfd,otherfile,COMMAND_LENGTH);
                        filename[strlen(otherfile)] = '\0';
                        printf("otherfile name : %s\n",otherfile);

                        for(int i=0;i<ns;i++) {
                            bzero(&data_server[i],sizeof(data_server[i]));
                            data_server[i].sin_family = AF_INET;
                            inet_aton(SERVER_IP,&(data_server[i].sin_addr));
                            data_server[i].sin_port = htons(SERVER_PORT+i+1);
                            dsfd[i] = socket(AF_INET,SOCK_STREAM,0);
                            connect(dsfd[i],SA(&data_server[i]),sizeof(data_server[i]));
                            
                            char num[2];
                            write(dsfd[i], cmd_num, sizeof(cmd_num));
                            write(dsfd[i],filename,COMMAND_LENGTH);
                            write(dsfd[i],otherfile,COMMAND_LENGTH);
                            char cn[2];
                            sprintf(cn,"%d",i+1);
                            write(dsfd[i],cn,sizeof(cn));
                            printf("Connected to server number %d\n",i+1);
                        }
                        for(int i=0;i<ns;i++) close(dsfd[i]);
                        break;

                    case 5:
                        bzero(otherfile,COMMAND_LENGTH);
                        read(connfd,otherfile,COMMAND_LENGTH);
                        filename[strlen(otherfile)] = '\0';
                        printf("otherfile name : %s\n",otherfile);

                        for(int i=0;i<ns;i++) {
                            bzero(&data_server[i],sizeof(data_server[i]));
                            data_server[i].sin_family = AF_INET;
                            inet_aton(SERVER_IP,&(data_server[i].sin_addr));
                            data_server[i].sin_port = htons(SERVER_PORT+i+1);
                            dsfd[i] = socket(AF_INET,SOCK_STREAM,0);
                            connect(dsfd[i],SA(&data_server[i]),sizeof(data_server[i]));
                            
                            char num[2];
                            write(dsfd[i], cmd_num, sizeof(cmd_num));
                            write(dsfd[i],filename,COMMAND_LENGTH);
                            write(dsfd[i],otherfile,COMMAND_LENGTH);
                            char cn[2];
                            sprintf(cn,"%d",i+1);
                            write(dsfd[i],cn,sizeof(cn));
                            printf("Connected to server number %d\n",i+1);
                        }
                        for(int i=0;i<ns;i++) close(dsfd[i]);
                        break;
                    
                    case 6:
                        d = opendir("ds1/");
                        strcpy(files, "");
                        if (d) {
                            while ((dir = readdir(d)) != NULL) {
                                if (!strcmp(dir->d_name, ".")) continue;
                                if (!strcmp(dir->d_name, "..")) continue;
                                int last = strlen(dir->d_name) - 1;
                                for (; last > 0; last--) {
                                    if (dir->d_name[last] == '.') break;
                                }
                                char *actfile = substr(dir->d_name, 0, last-1);
                                //printf("file : %s\n", actfile);
                                strcat(files, actfile);
                                strcat(files, "\n");
                                free(actfile);
                            }
                            write(connfd, files, 4096);
                            closedir(d);
                        }
                        break;

                    case 7:
                        //for(int i=0;i<ns;i++) {
                            // bzero(&data_server[i],sizeof(data_server[i]));
                            // data_server[i].sin_family = AF_INET;
                            // inet_aton(SERVER_IP,&(data_server[i].sin_addr));
                            // data_server[i].sin_port = htons(SERVER_PORT+i+1);
                            // dsfd[i] = socket(AF_INET,SOCK_STREAM,0);
                            // connect(dsfd[i],SA(&data_server[i]),sizeof(data_server[i]));
                            
                            // char num[2];
                            // write(dsfd[i], cmd_num, sizeof(cmd_num));
                            // printf("Connected to server number %d\n",i+1);
                        //}
                        //kill(getppid(), SIGKILL);
                        exit(0);
                        break;

                    default:
                        printf("COMMAND NOT SUPPORTED\n");
                }
            }
            exit(0);
        }
    }
    close(serv_sock);
}