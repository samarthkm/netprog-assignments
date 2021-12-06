#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include "fcgi_stdio.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/signal.h>

#define PORTNO 8081

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

//function to find location of string
int find_loc_of_string(char text[], char pattern[]) {
  int a, b, c,loc = -1;
  int p = strlen(pattern);
  int t = strlen(text);
  if (p > t) return -1;
  for (a = 0; a <= t - p; a++) {
    loc = c = a;
    for (b = 0; b < p; b++) {
      if (pattern[b] == text[c]) c++;
      else break;
    }
    if (b == p) return loc;
  }
  return -1;
}

//function to read file contents and write to a buffer
char* read_file(char* fname) {
    ssize_t size;
    struct stat st;
    char* buf;
    int fd,len;
    stat(fname,&st);
    len = st.st_size;
    buf = (char*) malloc(len+1);
    if((fd = open(fname,O_RDONLY)) == -1) {
        free(buf);
        return NULL;
    }
    size = read(fd,buf,len);
    if(size>0) {
        buf[len] = '\0';
        close(fd);
        return buf;
    }
    close(fd);
    return NULL;
}

//function to read fcgi files
char* read_fcgi_file(FCGI_FILE* fname) {
    char* buf =(char*) malloc(50000);
    FCGI_fread(buf,50000,1,fname);
    return buf;
}

//execute .cgi files
void fastcgi(char* fname, int i) {
  int len = strlen(fname);
  char* buf;
  char snd[5000];
  char exe[5000];
  bzero(exe,sizeof(exe));
  strcpy(exe,"./");
  strcat(exe,fname);
  FCGI_FILE* f = popen(exe,"r");
  buf = read_fcgi_file(f);
  strcpy(snd,"\n\n<html><head><title>EDS 100</title></head><body><textarea style=\"resize:none; width:1500px; height:700px;\">");
  strcat(snd,buf);
  strcat(snd,"</textarea></html>");
  send(i,snd,strlen(snd),0);
}

int main(int argc,char* argv[]) {
    int sockfd,connfd,pid;
    struct sockaddr_in serv_addr,cli_addr;
    socklen_t clilen;
    char *buf = "HTTP/1.1 200 OK\nKeep-Alive: timeout=15, max=100\nConnection: Keep-Alive\nContent-Type: text/html\n\n<!doctype html><html><head><title>EDS</title></head><body><form method=\"GET\"><label for=\"filename\">Enter filename:</label><input type=\"text\" id=\"filename\" name=\"filename\" /><input type=\"submit\" value=\"Submit\" /></form></body></html>";
    char data[1000000];
    if((sockfd = socket(AF_INET,SOCK_STREAM,0)) < 0) {
        perror("socket");
        exit(1);
    }

    //server setup
    bzero(&serv_addr,sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    serv_addr.sin_port = htons(PORTNO);

    if(bind(sockfd,(struct sockaddr*) &serv_addr,sizeof(serv_addr)) < 0) {
        perror("bind");
        exit(1);
    }

    if(listen(sockfd,10) < 0) {
        perror("listen");
        exit(1);
    }

    fd_set fds,readyfds;
    FD_ZERO(&fds);
    FD_SET(sockfd,&fds);
    //int max_socket = sockfd;
    int firstconn[1024];
    memset(firstconn,0,1024*sizeof(int));
    
    int first = 1;
    while(1) {
        readyfds = fds; 
        if(select(FD_SETSIZE,&readyfds,NULL,NULL,NULL) < 0) {
          perror("select");
          exit(1);
        }
        for(int i=0;i<FD_SETSIZE;i++) {
          if(FD_ISSET(i,&readyfds)) {
            if(i == sockfd) {
              clilen = sizeof(cli_addr);
              if((connfd = accept(sockfd, (struct sockaddr *) &cli_addr, &clilen)) < 0) {
                perror("accept");
              }
              FD_SET(connfd,&fds);
              //if(connfd>max_socket) max_socket = connfd;
              if(!firstconn[connfd]) {
                firstconn[connfd] = 1;
                send(connfd, buf, strlen (buf), 0);
              }
            }
            else {              
              bzero(&data,sizeof(data));
              int n;
              recv(i,data,sizeof(data),0); 
              write(1,data,sizeof(data));
              int m = find_loc_of_string(data,"GET /?filename=");
              if(m >= 0) {
                m+=15;
                int j = 0;
                char fname[1000];
                while(data[m+j] != ' ') {
                    fname[j] = data [m+j];
                    j++;
                }
                fname[j] = '\0';
                int len = strlen(fname);
                m = find_loc_of_string(fname,".cgi");
                if(m >= 0) {
                  int pid = fork();
                  if(pid == 0) {
                    write(1,&i,sizeof(i));
                    fastcgi(fname,i); //for .cgi files fork and execute fastcgi process
                    exit(0);
                  }
                  else {
                    continue;
                  }
                }
                char* filecontents;
                char snd[20000];
                filecontents = read_file(fname);
                sprintf(snd,"%s%s%s","\n\n<html><head></head><body><textarea style=\"resize:none; width:1500px; height:700px;\">",filecontents,"</textarea></html>");
                send (i, snd, strlen (snd), 0);
                bzero(&snd,sizeof(snd));
              }
            }
          }
        } 
    }
}
