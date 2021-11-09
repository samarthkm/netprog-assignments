#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <fcntl.h>

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

void shell(int socket, char* name) {
   // starting shell in user's home directory
   int res = chdir(getenv("HOME"));
   if (res < 0) {
      printf("warning : could not cd\n");
   }

   // socket is already connected to the server
   char cmd[256];
   char out[BUFLEN];

   while(1) {
      printf("clsh$ ");
      fgets(cmd, sizeof(cmd), stdin);
      cmd[strlen(cmd)-1] = '\0';

      if(strlen(cmd) == 0) continue;

      if(cmd[0] != 'n' || index_of('.', 0, cmd) == -1) { 
         if(!(strcmp(cmd, "clustertop") && strcmp(cmd, "nodes") && strcmp(cmd, "exit"))) {
            break;
         }
         // normal command
         char** splitesh = split(cmd, ' ');
         if (!strcmp(splitesh[0], "cd")) {
            int res = chdir(splitesh[1]);
            if (res < 0) printf("could not cd into %s\n", splitesh[1]);
         }
         else {
            system(cmd);
         }
         continue;
      }

      // send the command to the server
      write(socket, cmd, sizeof(cmd));
      
      if (!strcmp(cmd, "exit")) return;

      // wait for response and read when available
      int bytes = read(socket, out, BUFLEN);
      out[bytes] = '\0';

      // check if directory changed
      // if (cmd[0] == 'n' && (index_of('d', 0, cmd) - index_of('c', 0, cmd) == 1) && out[0] == '/') {
      //    int res = chdir(out);
      //    if (res < 0) {
      //       printf("warning : could not cd into [ %s ]\n", out);
      //    }
      // }

      // display the output
      printf("output:\n%s\n", out);
      bzero(out, sizeof(out));
   }
   close(socket);
}

char* execute(char* cmd, char* input) {
   int nwrds = words(cmd, ' ');
   char** wrds = split(cmd, ' ');
   char* prog = wrds[0];

   if (!strcmp(prog, "cd")) {
      char* dst = substr(cmd, 
            index_of(' ', 0, cmd) + 1, 
            strlen(cmd));
      printf("cd-ing into %s\n", dst);
      int res = chdir(dst);
      if (res < 0) {
         printf("warning : could not cd\n");
      }

      char* pwd = (char*) malloc(256 * sizeof(char));
      getcwd(pwd, 256*sizeof(char));
      free(dst);
      return pwd;
   }
 
   else {
      pid_t pid;
      int infd[2];
      int outfd[2];
      char *out = malloc(BUFLEN * sizeof(char));
      int status;

      pipe(infd);
      pipe(outfd);
      if (input != NULL && input[0] != '\0') write(outfd[1], input, strlen(input));
      close(outfd[1]);
      pid = fork();
      if (pid == 0)
      {
         dup2(outfd[0], STDIN_FILENO);
         dup2(infd[1], STDOUT_FILENO);
         close(outfd[1]);
         close(infd[0]);

         if (!strcmp(prog, "cl")) {
            char* part1 = substr(cmd, index_of('l', 0, cmd)+2, index_of('.', 0, cmd)-1);
            char* part2 = substr(cmd, index_of('.', 0, cmd)+1, strlen(cmd)-1);
            bzero(cmd, 256*sizeof(char));
            sprintf(cmd, "%s%d%s", part1, getpid(), part2);
            free(part1);
            free(part2);
            printf("cmd : %s", cmd);
         }

         system(cmd);
         exit(1);
      }
      close(outfd[0]);
      close(infd[1]);

      read(infd[0], out, BUFLEN);

      kill(pid, SIGKILL);
      waitpid(pid, &status, 0);
      return out;
   }
   return NULL;
}

void background(int socket) {
   // starting shell in user's home directory
   int res = chdir(getenv("HOME"));
   if (res < 0) {
      printf("warning : could not cd\n");
   }
   // socket is already set to listen on its port
   struct sockaddr_in srv;
   char cmd[256];
   char input[BUFLEN];
   unsigned int len = sizeof(srv);
   while (1) {
      int confd = accept(socket, SA(&srv), &len);
      // recieve command from client
      read(confd, cmd, sizeof(cmd));
      read(confd, input, BUFLEN);
      // get the output after evaluating the command
      char* out = execute(cmd, input);
      // write output back to the socket
      write(confd, out, BUFLEN);
      close(confd);
   }
   close(socket);
}

int main(int argc, char** argv) {
   if (argc < 2) exit(1);
   char* cname = argv[1];
   
   int pid = fork();

   if (pid == 0) {
      int sock = socket(AF_INET, SOCK_STREAM, 0);
      struct sockaddr_in me;
      me.sin_port = htons(sport + get_cnum(cname));
      me.sin_addr.s_addr = htonl(ipadr);
      me.sin_family = AF_INET;
      bind(sock, SA(&me), sizeof(me));
      listen(sock, 6);
      background(sock);
   } else {
      int sock = socket(AF_INET, SOCK_STREAM, 0);
      struct sockaddr_in srv;
      srv.sin_port = htons(sport);
      srv.sin_addr.s_addr = htonl(ipadr);
      srv.sin_family = AF_INET;
      connect(sock, SA(&srv), sizeof(srv));
      shell(sock, cname);
      close(sock);
   }
}
