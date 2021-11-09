#include <netinet/in.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <pthread.h>

#define sport 8000
#define ipadr INADDR_ANY
#define BUFLEN 2048
#define SA(x) ((struct sockaddr *) x)

int get_cnum(char* cname) {
   char* num = cname+1;
   return atoi(num);
}

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
   int count = 0;
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

// split command into target client and command
char** process_cmd(char* cmd) {
   char** info = malloc(2*sizeof(char*));
   info[0] = substr(cmd, 0, index_of('.', 0, cmd)-1);
   info[1] = substr(cmd, index_of('.', 0, cmd)+1, strlen(cmd)-1);
   printf("%s, %s\n", info[0], info[1]);
   return info;
}

int count_lines(char* filename) {
    FILE *fp;
    char c;
    int count = 0;
    fp = fopen(filename, "r");
    if (fp == NULL)
    {
        printf("Could not open config file");
        return 0;
    }
    for (c = getc(fp); c != EOF; c = getc(fp)) if (c == '\n') count = count + 1;
    fclose(fp);
    return count;
}

// reads the config file
int* get_ports_config(int* numports) {
   // assuming that the program and the
   // config file are located in the same
   // directory
   FILE* config = fopen("config", "r");
   char line[1024];
   char* fgets_ret;
   int lines = count_lines("config");
   int* ports = (int*) malloc (lines * sizeof(int));

   for(int i=0; i<lines; i++) {
      fgets_ret = fgets(line, sizeof(line), config);
      line[strlen(line)-1] = '\0';
      if(fgets_ret != NULL) {
         char* port = substr(line, index_of('n', 0, line) + 1, index_of(' ', 0, line) - 1);
         ports[i] = atoi(port) + 8000;
         free(port);
      }
   }
   *numports = lines;
   return ports;
}

char* clustertop() {
//   char* mem = "free -m | awk 'NR==2{printf \"Memory Usage: %s/%sMB (%.2f%%)\n\", $3,$2,$3*100/$2 }'";
//   char* cpu = "top -bn1 | grep load | awk '{printf \"CPU Load: %.2f\n\", $(NF-2)}'";
//   char* mem = "top -b -n 1 -p";
//   char* cpu = "| tail -3 | head";
   char cmd[256];
   sprintf(cmd, "cl top -b -n 1 -p . | tail -n 2");
   int numports;
   int* allports = get_ports_config(&numports);
   char tmp[20];
   int n;

   struct addrinfo hints, *res;
   bzero(&hints, sizeof(hints));
   hints.ai_family = AF_INET;
   hints.ai_socktype = SOCK_STREAM;

   char* out = (char*) malloc (sizeof(char) * BUFLEN);
   char outmp[BUFLEN] = {0};

   for(int i=0; i<numports; i++) {
      sprintf(tmp, "%d", allports[i]);
      int sock = socket(AF_INET, SOCK_STREAM, 0);
      struct sockaddr_in cli;
      cli.sin_port = htons(allports[i]);
      cli.sin_addr.s_addr = htonl(ipadr);
      cli.sin_family = AF_INET;
      if (connect(sock, SA(&cli), sizeof(cli)) == -1) continue;

      // send command to client
      write(sock, cmd, 256);
      write(sock, "", BUFLEN);
      // wait for response from client
      int rbytes = read(sock, outmp, BUFLEN);
      sprintf(tmp, "\n[Client n%d]\n\n", allports[i] - 8000);
      strcat(out, tmp);
      strcat(out, outmp);
      bzero(tmp, sizeof(tmp));
      close(sock);
   }
   return out;
}

char* active_nodes() {
   char* jkl = (char*) malloc(BUFLEN * sizeof(char));
   bzero(jkl, BUFLEN);
   FILE* config = fopen("config", "r");
   char line[1024];
   char* fgets_ret;
   int lines = count_lines("config");
   strcat(jkl,"list of nodes listening:\n");
   strcat(jkl, "-----------------------\n");

   for(int i=0; i<lines; i++) {
      fgets_ret = fgets(line, sizeof(line), config);
      line[strlen(line)-1] = '\0';
      if(fgets_ret != NULL) {
         char* name = substr(line, 0, index_of('.', 0, line)-1);
         char* ip = substr(line,index_of(' ', 0, line)+1, strlen(line)-1);
         char* port = substr(line, index_of('n', 0, line) + 1, index_of(' ', 0, line) - 1);
         int actport = atoi(port) + 8000;

         char tmp[10];
         int n;

         struct addrinfo hints, *res;
         bzero(&hints, sizeof(hints));
         hints.ai_family = AF_INET;
         hints.ai_socktype = SOCK_STREAM;

         int sock = socket(AF_INET, SOCK_STREAM, 0);
         struct sockaddr_in cli;
         cli.sin_port = htons(actport);
         cli.sin_addr.s_addr = htonl(ipadr);
         cli.sin_family = AF_INET;
         if (connect(sock, SA(&cli), sizeof(cli)) == -1) continue;

         char hjk[100];
         sprintf(hjk, "%s at ip address %s on port %d\n", name, ip, actport);
         strcat(jkl, hjk);
         close(sock);
         free(name);
         free(ip);
         free(port);
      }
   }
   return jkl;
}

// int num_active_nodes() {
//    FILE* config = fopen("config", "r");
//    int count = 0;
//    char line[1024];
//    char* fgets_ret;
//    int lines = count_lines("config");
// 
//    for(int i=0; i<lines; i++) {
//       fgets_ret = fgets(line, sizeof(line), config);
//       line[strlen(line)-1] = '\0';
//       if(fgets_ret != NULL) {
//          char* port = substr(line, index_of('n', 0, line) + 1, index_of(' ', 0, line) - 1);
//          int actport = atoi(port) + 8000;
// 
//          struct addrinfo hints, *res;
//          bzero(&hints, sizeof(hints));
//          hints.ai_family = AF_INET;
//          hints.ai_socktype = SOCK_STREAM;
// 
//          int sock = socket(AF_INET, SOCK_STREAM, 0);
//          struct sockaddr_in cli;
//          cli.sin_port = htons(actport);
//          cli.sin_addr.s_addr = htonl(ipadr);
//          cli.sin_family = AF_INET;
//          if (connect(sock, SA(&cli), sizeof(cli)) == -1) continue;
//          count ++;
//          close(sock);
//          free(port);
//       }
//    }
//    return count;
// }

void* client_relations(void* data) {
   pthread_detach(pthread_self());

   char cmd[256];
   char input[BUFLEN] = {0};
   int bytes;
   // get the socket connected to the client
   int confd = * (int *) data;

   while (1) {
      bytes = read(confd, cmd, sizeof(cmd));
      if(bytes == 0) continue;
      cmd[bytes] = '\0';
      printf("got command : [ %s ]\n", cmd);

      if (strcmp(cmd, "exit")) {
         if(!strcmp(cmd, "nodes")) {
            printf("showing active nodes\n");
            char* jkl = active_nodes();
            jkl[strlen(jkl)] = '\0';
            printf("%s\n", jkl);
            bzero(input, BUFLEN);
            strcpy(input, jkl);
            free(jkl);
         }
         else if (!strcmp(cmd, "clustertop")) {
            char* jkl = clustertop();
            jkl[strlen(jkl)] = '\0';
            // printf("%s\n", jkl);
            bzero(input, BUFLEN);
            strcpy(input, jkl);
            free(jkl);
         }
         else {
            int ncmds = words(cmd, '|');
            char** cmds = split(cmd, '|');
            for (int i=0; i<ncmds; i++) {
               // get the name of target client and
               // the command to be run, and the "number"
               // of the client to check for open port
               char** info = process_cmd(cmds[i]);

               // @new
               if (info[0][1] == '*') {
                  // run the command on all active nodes and concat the output
                  int numports;
                  int* allports = get_ports_config(&numports);
                  char tmp[10];
                  int n;

                  struct addrinfo hints, *res;
                  bzero(&hints, sizeof(hints));
                  hints.ai_family = AF_INET;
                  hints.ai_socktype = SOCK_STREAM;

                  char out[BUFLEN] = {0};
                  char outmp[BUFLEN] = {0};

                  for(int i=0; i<numports; i++) {
                     sprintf(tmp, "%d", allports[i]);
                     printf("checking port : %d\n", allports[i]);
                     int sock = socket(AF_INET, SOCK_STREAM, 0);
                     struct sockaddr_in cli;
                     cli.sin_port = htons(allports[i]);
                     cli.sin_addr.s_addr = htonl(ipadr);
                     cli.sin_family = AF_INET;
                     if (connect(sock, SA(&cli), sizeof(cli)) == -1) continue;
                     // send command to client
                     write(sock, info[1], 256);
                     write(sock, input, BUFLEN);
                     // wait for response from client
                     int rbytes = read(sock, outmp, BUFLEN);
                     strcat(out, outmp);
                     close(sock);
                     //}
               }
               // show output only once for n*cd
               // if (info[1][0] == 'c' && info[1][1] == 'd') {
               //    out[strlen(out)/num_active_nodes()] = '\0';
               // }
               // printf("out : %s\n", out);
               strcpy(input, out);
               bzero(out, sizeof(out));
               bzero(outmp, sizeof(outmp));
            } else {
               int cnum = get_cnum(info[0]);
               printf("n : %d, port : %d\n", cnum, sport+cnum);
               struct addrinfo hints, *res;
               bzero(&hints, sizeof(hints));
               hints.ai_family = AF_INET;
               hints.ai_socktype = SOCK_STREAM;
               int n;
               int port = sport + cnum;
               char tmp[10];
               sprintf(tmp, "%d", port);

               //if the port is open, open a connection
               int sock = socket(AF_INET, SOCK_STREAM, 0);
               struct sockaddr_in cli;
               cli.sin_port = htons(sport+cnum);
               cli.sin_addr.s_addr = htonl(ipadr);
               cli.sin_family = AF_INET;
               if (connect(sock, SA(&cli), sizeof(cli)) == -1)
                  break;

               // send command to client
               write(sock, info[1], 256);
               write(sock, input, BUFLEN);
               // wait for response from client
               int rbytes = read(sock, input, BUFLEN);
               close(sock);
            }
         }
      }
      // write output to the client requesting command
      write(confd, input, BUFLEN);
   }
      else {
         break;
      }
      bzero(cmd, 256);
      bzero(input, sizeof(input));
   }
   close(confd);
   pthread_exit(NULL);
}

int main() {
   int sock = socket(AF_INET, SOCK_STREAM, 0);
   struct sockaddr_in me;
   me.sin_port = htons(sport);
   me.sin_addr.s_addr = htonl(ipadr);
   me.sin_family = AF_INET;
   bind(sock, SA(&me), sizeof(me));
   listen(sock, 6);

   struct sockaddr_in cli;
   unsigned int len = sizeof(cli);

   while (1) {
      // accept a client connection
      int confd = accept(sock, SA(&cli), &len);
      unsigned long tid;
      pthread_create(&tid, NULL, &client_relations, &confd);
   }
   close(sock);
}
