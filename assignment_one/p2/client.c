#include<arpa/inet.h>
#include<fcntl.h>
#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<sys/socket.h>
#include<sys/stat.h>
#include<unistd.h>

#define SERVER_IP "127.0.0.1"
#define SERVER_PORT 9000
#define COMMAND_LENGTH 256
#define WORD_SIZE 1024
#define RECV_LENGTH 2048
#define MAX_FILE_SIZE 5242880 
#define CHUNK_SIZE 1048577
#define UP 1
#define RM 2
#define CAT 3
#define CP 4
#define MV 5
#define LS 6
#define EX 7

int get_cmd_start(char* command) {
  int iter;
  for (iter = 0; command[iter] == ' '; iter++);
  return iter;
}

int index_of_char_from(char c, int start, char* string) {
  for (int i = start; i < strlen(string); i++) {
      if (string[i] == c) return i;
  }
  return -1;
}

int next_non_whitespace_from(char* command, int index) {
  int i;
  for (i = index; command[i] == ' '; i++);
  return i;
}

void get_word_from_index(char* command, char* word, int index,char delim) {
  int i;
  for (i = index; !(command[i] == delim || command[i] == '\0'); i++) {
    word[i-index] = command[i];
    if (i-index >= WORD_SIZE-1) {
      break;
    }
  }
  word[i] = '\0';
}

int count_words(char *command, char delim) {
    int count = 1;
    int space_before = 0;

    for (int i = 0; i < strlen(command); i++) {
        if (!space_before) {
            if (command[i] == delim) {
                count++;
                space_before = 1;
            }
        }
        else {
            if (command[i] != delim) {
                space_before = 0;
            }
        }
    }
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

void split_command_into_words(char* command, char** args,char delim) {
   int num_args = count_words(command, delim);
   int j = get_cmd_start(command) - 1;

   for (int i = 0; i < num_args; i++) {
      args[i] = (char*) malloc(1000 * sizeof(char));
      get_word_from_index(command, args[i], j+1,delim);
      j += strlen(args[i]) + 1;
      j = next_non_whitespace_from(command, j) - 1;
   }
}

void up(int connfd,char* fname) {
    int filefd,bytes_read;

    if((filefd = open(fname,O_RDONLY)) == -1) {
        perror("open");
        exit(1);
    }

    char filebuf[MAX_FILE_SIZE];
    bytes_read = read(filefd,filebuf,MAX_FILE_SIZE);
    filebuf[bytes_read]='\0';
    
    char send[bytes_read+sizeof(int)];
    sprintf(send,"%d%s",bytes_read,filebuf);
    if(write(connfd,send,MAX_FILE_SIZE+sizeof(int)) == -1) {
        perror("upload");
        exit(1);
    }
} 

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


int main() {
    struct sockaddr_in server_address;
    bzero(&server_address,sizeof(server_address));
    server_address.sin_family = AF_INET;
    inet_aton(SERVER_IP,&(server_address.sin_addr));
    server_address.sin_port = htons(SERVER_PORT);
    int connfd = socket(AF_INET,SOCK_STREAM,0);
    if(connect(connfd, (struct sockaddr*)&server_address, sizeof(server_address)) == -1) {
        perror("connect");
        exit(1);
    }
    
    while(1) {
        printf("\nbigfs$>>");
        char* buf = malloc(sizeof(char)*COMMAND_LENGTH);
        if(scanf("%[^\n]", buf) == -1) {
            continue;
        }
        if(buf[0] == '\0' || buf == NULL) {
            continue;
        }
        getchar();
        
        buf[strlen(buf)] = '\0';

        int num_args = count_words(buf, ' ');
        char** args;
        args = (char **)malloc(sizeof(char *) * num_args);
        split_command_into_words(buf,args,' ');
     
        if(strcmp(args[0],"up") == 0) {
            int bytes_read;
            char cmd_num[2];
            sprintf(cmd_num,"%d",(int)UP);
            printf("cmd code : %s\n",cmd_num);
            printf("filename : %s",args[1]);
    
            write(connfd,cmd_num,sizeof(cmd_num));        
            write(connfd,args[1],COMMAND_LENGTH);        
        }

        else if (strcmp(args[0], "rm") == 0) {
            int bytes_read;
            char cmd_num[2];
            sprintf(cmd_num,"%d",(int)RM);
            printf("cmd code : %s\n",cmd_num);
            printf("filename : %s",args[1]);
            
            write(connfd,cmd_num,sizeof(cmd_num));
            write(connfd,args[1],COMMAND_LENGTH);  
        }

        else if (strcmp(args[0], "cat") == 0) {
            int bytes_read;
            char cmd_num[2],response[MAX_FILE_SIZE];
            sprintf(cmd_num,"%d",(int)CAT);
            printf("cmd code : %s\n",cmd_num);
            printf("filename : %s\n",args[1]);
            
            write(connfd,cmd_num,sizeof(cmd_num));
            write(connfd,args[1],COMMAND_LENGTH); 

            bzero(response,MAX_FILE_SIZE);
            strcpy(response,"");

            for(int i=0;i<num_ds();i++) {
                char chunk[CHUNK_SIZE];
                bzero(chunk,CHUNK_SIZE);
                int bytes_read = read(connfd,chunk,sizeof(chunk));

                if(bytes_read>0) {
                    printf("%s\n", chunk);
                }
            }
        }

        else if (strcmp(args[0], "cp") == 0) {
            int bytes_read;
            char cmd_num[2];
            sprintf(cmd_num,"%d",(int)CP);
            printf("cmd code : %s\n",cmd_num);
            printf("filename : %s ",args[1]);
            printf("copied to: %s\n", args[2]);
            
            write(connfd,cmd_num,sizeof(cmd_num));
            write(connfd,args[1],COMMAND_LENGTH);
            write(connfd,args[2],COMMAND_LENGTH);  
        }

        else if (strcmp(args[0], "mv") == 0) {
            int bytes_read;
            char cmd_num[2];
            sprintf(cmd_num,"%d",(int)MV);
            printf("cmd code : %s\n",cmd_num);
            printf("filename : %s ",args[1]);
            printf("copied to: %s\n", args[2]);
            
            write(connfd,cmd_num,sizeof(cmd_num));
            write(connfd,args[1],COMMAND_LENGTH);
            write(connfd,args[2],COMMAND_LENGTH);  
        }

        else if (strcmp(args[0], "ls") == 0) {
            int bytes_read;
            char cmd_num[2];
            sprintf(cmd_num,"%d",(int)LS);
            printf("cmd code : %s\n\n",cmd_num);
            write(connfd,cmd_num,sizeof(cmd_num));
            char files[4096];
            read(connfd, files, 4096);
            files[strlen(files)] = '\0';
            printf("files:\n======\n%s", files);
        }

        else if (strcmp(args[0], "exit") == 0) {
            int bytes_read;
            char cmd_num[2];
            sprintf(cmd_num,"%d",(int)EX);
            printf("cmd code : %s\n\n",cmd_num);
            write(connfd,cmd_num,sizeof(cmd_num));
            close(connfd);
            exit(0);
        }
    }
    close(connfd);
}
