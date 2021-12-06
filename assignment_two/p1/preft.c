#include <netinet/in.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <string.h>

#define SESHFILE "names"
#define BUFCAP 1256

int CPP, T, N, PORT;
// process local, thread global
int PROCS_DONE = 0;
int THREADS_DONE = 0;
int THREADS_COMP = 0;
int listening_on;
pthread_mutex_t mut;

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

// int threads_running = 0;
// int connections = 0;
// pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

void* thread_main(void* args) {
   int listening_on = *(int*)args;
   printf("created thread %lu in %d\n", pthread_self(), getpid());
   fflush(stdout);

   struct sockaddr_in client;
   unsigned int clen = sizeof(client);

   while (1) {
      int confd = accept(listening_on, (struct sockaddr*)&client, &clen);
      pthread_mutex_lock(&mut);
      THREADS_DONE += 1;
      pthread_mutex_unlock(&mut);
      if (THREADS_DONE > CPP) {
         printf("Connections per process exceeded.\n");
         printf("Try again after some time\n");
         write(confd, "OOPS", 5);
         continue;
      }
      write(confd, "FINE", 5);
      while (1) {
         char buf[BUFCAP];
         int portnum;
         char* name;
         read(confd, buf, BUFCAP);
         printf("[ %lu in %d ] got : [on %d] %s\n", pthread_self(), getpid(), confd, buf);

         char** pieces = split(buf, ' ');
         // printf("command : %s    | %d\n", pieces[0], strcmp(pieces[0], "JOIN")); 

         if (!strcmp(pieces[0], "JOIN")) {
            FILE* f = fopen(SESHFILE, "a");
            char entry[50];
            fprintf(f, "%s\t%s\n", pieces[1], pieces[2]);
            fclose(f);
            name = strdup(pieces[1]);
            portnum = atoi(pieces[2]);
         }

         else if (!strcmp(pieces[0], "CHAT")) {
            FILE* f = fopen(SESHFILE, "r");
            char name[10];
            char pid[10];

            while (EOF != fscanf(f, "%s\t%s", name, pid)) {
               if (!strcmp(name, pieces[1])) {
                  int cl = socket(AF_INET, SOCK_STREAM, 0);
                  int pidint = atoi(pid);
                  struct sockaddr_in serv;
                  serv.sin_port = pidint;
                  serv.sin_addr.s_addr = htonl(INADDR_ANY);
                  serv.sin_family = AF_INET;
                  connect(cl, (struct sockaddr*)&serv, sizeof(serv));
                  int spc1, spc2;
                  spc1 = index_of(' ', 0, buf);
                  spc2 = index_of(' ', spc1+1, buf);
                  write(cl, substr(buf, spc2+1, strlen(buf)-1), BUFCAP);
                  close(cl);
               }
            }
         }

         else if (!strcmp(pieces[0], "LEAV")) {
            pthread_mutex_lock(&mut);
            perror("something");
            // printf("threads done : %d\n", THREADS_DONE);
            pthread_mutex_unlock(&mut);
            int cl = socket(AF_INET, SOCK_STREAM, 0);
            struct sockaddr_in serv;
            serv.sin_port = portnum;
            serv.sin_addr.s_addr = htonl(INADDR_ANY);
            serv.sin_family = AF_INET;
            connect(cl, (struct sockaddr*)&serv, sizeof(serv));
            write(cl, pieces[0], BUFCAP);
            close(cl);

            pthread_mutex_lock(&mut);
            THREADS_COMP += 1;
            FILE* f = fopen(SESHFILE, "r");
            FILE* out = fopen(SESHFILE"_tmp", "w"); // since the compiler makes "hi""bye" into "hibye" @ compiletime, this works
            char n[10], id[10];
            while(fscanf(f, "%s\t%s", n, id) != EOF) {
               if (strcmp(name, n)) {
                  fprintf(out, "%s\t%s\n", n, id);
               }
            }
            fclose(out);
            fclose(f);
            remove(SESHFILE);
            rename(SESHFILE"_tmp", SESHFILE);
            pthread_mutex_unlock(&mut);

            break;
         }
      }
   }
   return NULL;
}

// every process has a thread manager function called child_process_run
void child_process_run(int CPP, int T, int listening_on) {
   THREADS_DONE = 0;
   THREADS_COMP = 0;
   pthread_mutex_init(&mut, NULL);

   for (int i=0; i<T; i++) {
      unsigned long tid;
      pthread_create(&tid, NULL, thread_main, (void*)&listening_on);
   }

   while (1) {
      pthread_mutex_lock(&mut);
      if (THREADS_COMP == CPP) break;
      pthread_mutex_unlock(&mut);
   }
   kill(getppid(), SIGUSR1);
}

void handle_sigchild(int sig) {
   PROCS_DONE += 1;
   printf("process done. exiting.\n");
   fflush(stdout);
   // if (PROCS_DONE >= N) {
   //    remove(SESHFILE);
   //    exit(0);
   // }
   int pid = fork();
   if (pid == -1) {
      printf("fork failure\n"); 
      exit(1);
   }

   if (pid == 0) child_process_run(CPP, T, listening_on);
   else return;
}

int main(int argc, char* argv[]) {
   int pid, tid; // for proc and thread ids.

   if (argc != 5) {
      printf("expected 4 arguments; got %d\n", argc-1);
      exit(1);
   }

   // getting command line args
   PORT = atoi(argv[1]);
   N    = atoi(argv[2]);
   T    = atoi(argv[3]);
   CPP  = atoi(argv[4]);

   listening_on = socket(AF_INET, SOCK_STREAM, 0);
   struct sockaddr_in serv;
   serv.sin_port = PORT;
   serv.sin_addr.s_addr = htonl(INADDR_ANY);
   serv.sin_family = AF_INET;
   bind(listening_on, (struct sockaddr*)&serv, sizeof(serv));
   listen(listening_on, CPP*N);
   
   signal(SIGUSR1, handle_sigchild);

   // create the pool of processes
   for (int i=0; i<N; i++) {
      pid = fork();
      switch (pid) {
         case -1: 
            printf("fork failure\n"); 
            exit(1);

         case 0:
            // inside child process
            child_process_run(CPP, T, listening_on);
            break;

         // no default since parent doesnt 
         // need to do anything special
      }
      // to stop children from making more children
      if (pid == 0) exit(0);
   }

   // parent process (children die before reaching this line)
   // put parent to sleep. The signal handler will deal with
   // creating new processes and managing the process pool.
   while (1) {
      pause();
   }
   return 0;
}
