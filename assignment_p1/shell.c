// using this for 'tee'
#define _GNU_SOURCE

#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#define WORD_SIZE 128
#define MAX_FD_NUMBER 1024

char** all_paths;
int exists = 0;

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

void split_paths() {
   char *paths = getenv("PATH");
   paths = strcat(paths,":");
   int num_paths = count_words(paths,':');
   all_paths = (char**)malloc(num_paths * sizeof(char*));
   int start = 0;
   for(int i=0;i<num_paths;i++) {
      all_paths[i] = substr(
            paths,
            start,
            index_of_char_from(':',start,paths)-1);
      start = index_of_char_from(':',start,paths)+1;

   }
}

void right_redirect(char** args,int num_args) {
   FILE* file = fopen(args[num_args-1],"w");
   if(file == NULL) {
      perror("fopen");
   }
   int fno = fileno(file);
   printf("file fd : %d\n", STDOUT_FILENO);
   printf("remapped fd : %d\n", fno);
   dup2(fno,STDOUT_FILENO);
   close(fno);
   fclose(file);
   args[num_args-1] = NULL;
   args[num_args-2] = NULL;

}

void double_right_redirect(char** args,int num_args) {
   FILE* file = fopen(args[num_args-1],"a");
   if(file == NULL) {
      perror("fopen");
   }
   int fno = fileno(file);
   printf("file fd : %d\n", STDOUT_FILENO);
   printf("remapped fd : %d\n", fno);
   dup2(fno,STDOUT_FILENO);
   close(fno);
   fclose(file);
   args[num_args-1] = NULL;
   args[num_args-2] = NULL;
}

void left_redirect(char** args,int num_args) {
   FILE* file = fopen(args[num_args-1],"r");
   if(file == NULL) {
      perror("fopen");
   }
   int fno = fileno(file);
   printf("file fd : %d\n", STDIN_FILENO);
   printf("remapped fd : %d\n", fno);
   dup2(fno,STDIN_FILENO);
   close(fno);
   fclose(file);
   args[num_args-1] = NULL;
   args[num_args-2] = NULL;
}


void exec_cmd(char** args,int num_args,int in_fd,int out_fd) {
   int input,output,pid,wstatus,bg=0;
   input = dup(STDIN_FILENO);
   output = dup(STDOUT_FILENO);
   if(in_fd >= 0) {
      dup2(in_fd,STDIN_FILENO);
      close(in_fd);
   }
   if(out_fd >= 0) {
      dup2(out_fd,STDOUT_FILENO);
      close(out_fd);
   }

   if(num_args > 1 && args[num_args-1] != NULL && strcmp(args[num_args-1],"&") == 0) {
      args[num_args-1] = NULL;
      bg=1;
   }

   if((pid = fork()) == 0) {
      setpgid(getpid(),getpid());
      split_paths();
      char* paths = getenv("PATH");
      paths = strcat(paths,":");
      int num_paths = count_words(paths,':');
      char* prog_name = strdup(args[0]);
      int found = 0;

      int right = 0;
      int left = 0;
      int doubleright = 0; 

      for(int i = 0;i<num_args-1;i++) {
         if(strcmp(args[i],">>") == 0) {
            doubleright = 1;
         }
         else if(strcmp(args[i],">") == 0) {
            right = 1;
         }
         else if(strcmp(args[i],"<") == 0) {
            left = 1;
         }
      }

      for(int i=0;i<num_paths-1;i++) {
         char p[512];
         strcpy(p,all_paths[i]);
         strcat(p,"/");
         strcat(p,prog_name);
         strcpy(args[0],p);
         if(access(p,F_OK) == 0) {
            found = 1;
            pid_t pid;
            if(right == 1) {
               right_redirect(args, num_args);
            }
            if(doubleright == 1) {
               double_right_redirect(args, num_args);              
            }
            if(left == 1) {
               left_redirect(args, num_args);
            }                    
            if(execv(args[0],args) == -1) {
               perror("execv");
               exit(1);
            }     
         }
      }
      if(!found) {
         char pwd[512];
         //getcwd(pwd, sizeof(pwd));
         //strcat(pwd,"/");
         //strcat(pwd,prog_name);
         //strcpy(args[0],pwd);
         printf("%s\n",prog_name);
         if(access(prog_name,F_OK) == 0) {
            pid_t pid;
            found = 1;
            if(right == 1) {
               right_redirect(args, num_args);
            }
            if(doubleright == 1) {
               double_right_redirect(args, num_args);              
            }
            if(left == 1) {
               left_redirect(args, num_args);
            }                    
            if(execv(args[0],args) == -1) {
               perror("execv");
               exit(1);
            }     
         }
      } 
      if(!found) {
         printf("shellerror: ./%s: No such file or directory\n",prog_name);
         exit(1);
      }
      exit(0);  
   }
   else {
      if(bg == 1) {
         int bgst = waitpid(pid,&wstatus,WNOHANG);
         if(bgst == 0) {
            printf("running program %s (PID %d) in background\n", args[0], pid);
            fflush(stdout);
            fflush(stdin);
         }
         else if (bgst < 0) {
            perror("background failed");
         }
      }
      else {
         if(in_fd == -1) tcsetpgrp(STDIN_FILENO,pid);
         waitpid(pid,&wstatus,WUNTRACED);
         if(in_fd >= 0) {
            dup2(input,STDIN_FILENO);
            close(input);
         } 
         if(out_fd >= 0) {
            dup2(output,STDOUT_FILENO);
            close(output);
         }
         //if(exists) {
         printf("running program %s (PID %d) in foreground\n", args[0], pid);
         if (!wstatus) printf("terminated successfully | STATUS CODE :%d\n",wstatus);
         
         signal(SIGTTOU, SIG_IGN);
         if (tcsetpgrp(STDIN_FILENO, getpid()) == -1) {
            perror("error");
         }
         signal(SIGTTOU, SIG_DFL);
      }
   }

}

void run(char** args,int num_args,int in_fd,int out_fd) {
   char** cmd_arr = (char**)malloc(num_args*sizeof(char*));
   int padnull=1;
   int k,j = 0;
   for(int i=0;i<num_args;i++) {
      if(strcmp(args[i],"|") == 0) {
         int pipefd[2];
         pipe(pipefd);
         cmd_arr[j++] = NULL;
         exec_cmd(cmd_arr,j,in_fd,pipefd[1]);
         char** remaining_cmd = malloc((num_args-i)*sizeof(char*));
         for(k=i+1;k<num_args+1;k++) {
            remaining_cmd[k-1-i] = args[k];
         }
         padnull = 0;
         printf("piping %s to %s ( in_fd : %d out_fd : %d )\n", cmd_arr[0], remaining_cmd[0], pipefd[0], pipefd[1]);     
         //printf("%d : %d : seding nargs : %d\n", num_args, i, num_args-i-1);
         run(remaining_cmd, num_args-i-1, pipefd[0], out_fd);
         return;
      }
      else if(strcmp(args[i],"||") == 0) {
         int a,pipefd1[2],pipefd2[2];
         pipe(pipefd1);
         pipe(pipefd2);
         cmd_arr[j] = NULL;
         exec_cmd(cmd_arr,j,in_fd,pipefd1[1]);

         char buffer[2048];

         // attempting to copy the contents of pipefd1
         // to pipefd2 while retaining the data in pipefd1
         // 
         // read(pipefd1[0],buffer,sizeof(buffer));
         // buffer[strlen(buffer)] = '\0';
         // printf("got : %s", buffer);
         // write(pipefd1[1],buffer,sizeof(buffer));
         // perror("write err");
         // write(pipefd2[1],buffer,sizeof(buffer));

         // using 'tee' to copy data from one pipe to another
         tee(pipefd1[0], pipefd2[1], 4096, 0);
         close(pipefd2[1]);

         char** cmd_arr1 = (char**)malloc((num_args - i) * sizeof(char *));
         char** cmd_arr2 = (char**)malloc((num_args - i) * sizeof(char *));

         for(a = 0; a<num_args-i-1 && strcmp(args[a+i+1],","); a++) {
            cmd_arr1[a] = args[a+i+1];
         }
         cmd_arr1[a] = NULL;

         int l;
         for(l = 0; l<num_args-a-i-2 && strcmp(args[l+i+2+a],","); l++) {
            cmd_arr2[l] = args[l+a+i+2];
         }
         cmd_arr2[l] = NULL;

         printf("piping %s  to \n",  cmd_arr[0]);
         printf("           |---> %s ( in_fd : %d out_fd : %d )\n", cmd_arr1[0], pipefd1[0], pipefd1[1]);
         printf("           |---> %s ( in_fd : %d out_fd : %d )\n\n", cmd_arr2[0], pipefd2[0], pipefd2[1]);

         run(cmd_arr1, a, pipefd1[0], out_fd);
         run(cmd_arr2, l, pipefd2[0], out_fd);
         return;
      }
      else if(strcmp(args[i],"|||") == 0) {
         int a, b, c,pipefd1[2],pipefd2[2], pipefd3[2];
         pipe(pipefd1);
         pipe(pipefd2);
         pipe(pipefd3);
         cmd_arr[j] = NULL;
         exec_cmd(cmd_arr,j,in_fd,pipefd1[1]);
         // using 'tee' to copy data from one pipe to another
         tee(pipefd1[0], pipefd2[1], 4096, 0);
         tee(pipefd1[0], pipefd3[1], 4096, 0);
         close(pipefd2[1]);
         close(pipefd3[1]);

         char** cmd_arr1 = (char**)malloc((num_args - i) * sizeof(char *));
         char** cmd_arr2 = (char**)malloc((num_args - i) * sizeof(char *));
         char** cmd_arr3 = (char**)malloc((num_args - i) * sizeof(char *));

         for(a = 0; a<num_args-i-1 && strcmp(args[a+i+1],","); a++) {
            cmd_arr1[a] = args[a+i+1];
         }
         cmd_arr1[a] = NULL;

         for(b = 0; b<num_args-i-a-2 && strcmp(args[b+a+i+2],","); b++) {
            cmd_arr2[b] = args[b+a+i+2];
         }
         cmd_arr2[b] = NULL;

         for(c = 0; c<num_args-b-a-i-3 && strcmp(args[c+i+3+a+b],","); c++) {
            cmd_arr3[c] = args[c+b+i+a+3];
         }
         cmd_arr3[c] = NULL;

         printf("piping %s  to \n",  cmd_arr[0]);
         printf("           |---> %s ( in_fd : %d out_fd : %d )\n", cmd_arr1[0], pipefd1[0], pipefd1[1]);
         printf("           |---> %s ( in_fd : %d out_fd : %d )\n", cmd_arr2[0], pipefd2[0], pipefd2[1]);
         printf("           |---> %s ( in_fd : %d out_fd : %d )\n\n", cmd_arr3[0], pipefd3[0], pipefd3[1]);

         run(cmd_arr1, a, pipefd1[0], out_fd);
         run(cmd_arr2, b, pipefd2[0], out_fd);
         run(cmd_arr3, c, pipefd3[0], out_fd);
         return;
      }
      else {
         cmd_arr[j] = (char*)malloc(256);
         strcpy(cmd_arr[j],args[i]);
         j++;
         // padnull=1;
      }
   }
   if (padnull && cmd_arr != NULL) {
      cmd_arr[j] = NULL;
      // num_args++;
      //printf("j=%d\n", j);
      exec_cmd(cmd_arr, j, in_fd, out_fd);
   }
}

char** parse_daemonize(char** args,int num_args) {
   char** new_args = (char**)malloc(sizeof(char*) * (num_args-1));
   for(int i=1;i<num_args;i++) {
      new_args[i-1] = args[i];
   }
   return new_args;
}

void daemonize(char** args,int num_args) {
   pid_t pid=0,sid=0;
   if((pid = fork()) == -1) {
      perror("fork");
      exit(1);
   }
   if(pid>0) {
      printf("Killing parent process ...");
      exit(0);
   }
   umask(0);
   sid = setsid();
   if(sid<0) {
      perror("sid");
      exit(1);
   }
   chdir("/");
   for(int i=0;i<MAX_FD_NUMBER;i++) {
      close(i);
   }
   run(args,num_args,-1,-1);
}

int main(int argc,char* argv[]) {
   char buf[256];

   while(1) {
      char pwd[512];
      getcwd(pwd, sizeof(pwd));
      printf("%s> ", pwd);

      // bzero(buf, sizeof(buf));

      char* ret = fgets(buf, sizeof(buf), stdin);
      if (ret == NULL) {
         printf("Got EOF ! Exiting shell\n");
         exit(0);
      }
      buf[strlen(buf)-1] = '\0';
      if (buf[0] == '\0') continue;

      int num_args = count_words(buf, ' ');
      char** args;
      args = (char **)malloc(sizeof(char *) * num_args);
      split_command_into_words(buf,args,' ');

      if(strcmp(buf,"exit") == 0) {
         exit(0);
      }
      else if(strcmp(args[0], "cd") == 0) {
         char* pwd = substr(buf, index_of_char_from(' ', 0, buf) + 1, strlen(buf));
         if (chdir(pwd) < 0) {
            printf("WARNING : could not change directories\n");
         }
         continue;
      }
      else if(strcmp(args[0],"daemonize") == 0) {
         args = parse_daemonize(args, num_args);
         daemonize(args,num_args);
      }

      run(args,num_args,-1,-1);
      free(args);
   }
}
