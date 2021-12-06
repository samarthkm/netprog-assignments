#include <netinet/in.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <unistd.h>
#include <signal.h>
#include <sys/socket.h>
#include <string.h>
#include <arpa/inet.h>

#define BUFCAP 1256

int port;

// substring function (both ends included) (allocates memory on heap)
char* substr (char* string, int start, int end) {
  if (end - start + 1 > strlen(string)) return NULL;
  int len = end + 2 - start;
  int i;
  char* sub = (char*) malloc (sizeof(char) * len);
  for (i = 0; i + start <= end; i++) sub[i] = string[i + start];
  sub[i] = '\0';
  return sub;
}

// function to get index of character
int index_of (char c, int start, char* string) {
  for (int i = start; i < strlen(string); i++) if (string[i] == c) return i;
  return -1;
}

int words(char* string, char delim) {
   int count;
   int prev_was_delim = 0;
   for (int i = 0; string[i] != '\0'; i++) {
      if (string[i] == delim) {
         if (prev_was_delim != 1) {
            prev_was_delim = 1;
            count++;
         } 
      } else {
         prev_was_delim = 0;
      }
   }
   return count + 1;
}

char** split(char* string, char delim) {
   int count = words(string, delim);
   int start = 0;
   char** words = (char**) malloc(count * sizeof(char**));
   for (int i = 0; i < count; i++) {
      int idpipe = index_of(delim, start, string);
      if (idpipe != -1) {
         words[i] = substr(string, start, idpipe-1);
         start = idpipe + 1;
      } else {
         words[i] = substr(string, start, strlen(string)-1);
         break;
      }
   }
   return words;
}

int main(int argc, char* argv[]) {
   int pid;
   pid = fork();

   if (pid == 0) {
      // using this as the port number
      // because it will be unique to
      // each client
      int mypid = getppid();
      int listening_on;
      listening_on = socket(AF_INET, SOCK_STREAM, 0);
      struct sockaddr_in serv;
      serv.sin_port = mypid;
      serv.sin_addr.s_addr = htonl(INADDR_ANY);
      serv.sin_family = AF_INET;
      bind(listening_on, (struct sockaddr*)&serv, sizeof(serv));
      listen(listening_on, 5);

      while (1) {
         struct sockaddr_in client;
         unsigned int clen = sizeof(client);
         int confd = accept(
               listening_on, 
               (struct sockaddr*)&client, 
               &clen);

         char buf[BUFCAP];
         read(confd, buf, BUFCAP);
         printf("(incoming message) : %s\n", buf);
 
         if (!strcmp(buf, "LEAV")) break;
         printf(" > ");
         fflush(stdout);
         close(confd);
      }
   }
   else {
      port = atoi(argv[1]);
      int sock = socket(AF_INET, SOCK_STREAM, 0);
      struct sockaddr_in serv;
      serv.sin_port = port;
      serv.sin_addr.s_addr = htonl(INADDR_ANY);
      serv.sin_family = AF_INET;
      char err[5];
      int i = connect(sock, (struct sockaddr*)&serv, sizeof(serv));
      read(sock, err, 5);
      if (!strcmp(err, "OOPS")) {
         printf("server busy, try again later\n");
         exit(0);
      }
      if (i < 0) {
         printf("server busy, try again later\n");
         exit(0);
      }

      while (1) {
         char buf[BUFCAP];
         printf(" > ");
         fgets(buf, sizeof(buf), stdin);
         buf[strlen(buf)-1]='\0';
         char** pieces = split(buf, ' ');

         if (!strcmp(pieces[0], "JOIN")) {
            char buffy[BUFCAP];
            sprintf(buffy, "%s %d", buf, getpid());
            bzero(buf, sizeof(buf));
            strcpy(buf, buffy);
         }

         write(sock, buf, sizeof(buf));
         if (!strcmp(pieces[0], "LEAV")) break;
      }
      close(sock);
   }
   return 0;
}
