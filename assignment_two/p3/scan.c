#include <asm-generic/socket.h>
#include <bits/types/struct_timeval.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netinet/ip.h>
#include <strings.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <netinet/ip_icmp.h>
#include <stdio.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>

extern int errno;

unsigned int get_system_ip_addr() {
   char* random_addr = "10.0.0.1";
   int dns_port = 53;
   struct sockaddr_in serv;
   int sock = socket (AF_INET, SOCK_DGRAM, 0);
   memset(&serv, 0, sizeof(serv));
   serv.sin_family = AF_INET;
   serv.sin_addr.s_addr = inet_addr(random_addr);
   serv.sin_port = htons(dns_port);

   int err = connect(sock, (const struct sockaddr*) &serv, sizeof(serv));

   struct sockaddr_in name;
   socklen_t namelen = sizeof(name);
   err = getsockname(sock, (struct sockaddr*) &name, &namelen);
   close(sock);
   return name.sin_addr.s_addr;
}

#define BUFLEN 4096
char sbuf[BUFLEN], rbuf[BUFLEN];
int icmpsock;
struct sockaddr_in dest;

void sendICMP(int id) {
   bzero(sbuf, sizeof(sbuf));
   int i;
   struct icmphdr *icmp;
   char *ptr;
   icmp = (struct icmphdr*)sbuf;
   icmp->type = ICMP_ECHO;
   icmp->code = 0;
   icmp->checksum = 25;
   icmp->un.echo.id = id;
   i = sendto(icmpsock, sbuf, BUFLEN, 0, (struct sockaddr*)&dest, sizeof(dest));
   if (i < 0) {
      printf("something went wrong while sending icmp packet\n");
      perror("icmp");
   }
}

void recvICMP(int id) {
   bzero(rbuf, sizeof(rbuf));
   int n;
   unsigned int froml;
   struct sockaddr_in from;
   n = recvfrom(icmpsock, rbuf, BUFLEN, 0, (struct sockaddr*)&from, &froml);
   int ipheaderl;
   struct icmphdr *icmp;
   struct ip *ip;
   char *ptr;

   ip = (struct ip*)rbuf;
   ipheaderl = ip->ip_hl << 2;
   icmp = (struct icmphdr*)(rbuf + ipheaderl);
   if (icmp->un.echo.id == id) {
      if (icmp->type == ICMP_ECHO || icmp->type == ICMP_ECHOREPLY) {
         char *ipbuf;
         ipbuf = inet_ntoa(ip->ip_src);
         printf("\nFOUND : %s\n", ipbuf);
         printf("SEARCHING ");
         fflush(stdout);
      }
   } else {
      printf(".");
      fflush(stdout);
   }
}

void lookup_live_ips(char* mask) {
   int n_myip = get_system_ip_addr();
   int myip = ntohl(n_myip);
   int mymask = ntohl(inet_addr(mask));
   
   int start = myip & mymask;
   int end = start + (~mymask);
   char ip[50];

   int id = getpid() & 0xffff;
   dest.sin_port = 0;
   dest.sin_family = AF_INET;
   icmpsock = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
   struct timeval tv;
   tv.tv_sec = 1;
   tv.tv_usec = 0;
   setsockopt(icmpsock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

   for (int i = start; i <= end; i++) {
      dest.sin_addr.s_addr = htonl(i);
      sendICMP(id);
      if (i == start) printf("SEARCHING ");
      recvICMP(id);
   }
   printf("\n");
}

void scan_ports() {
   int sockfd;
   int udps, icmpr;
   struct icmphdr *icmp;
   struct iphdr *ip;
   char recvbuf[4096];
   struct sockaddr_in serv;
   bzero(&serv, sizeof(serv));
   serv.sin_addr.s_addr = inet_addr("0.0.0.0");
   serv.sin_family = AF_INET;

   // TCP open port scan
   printf("looking for open tcp ports\n");
   for (int i=0; i<=65535; i++) {
      serv.sin_port = htons(i);
      sockfd = socket(AF_INET, SOCK_STREAM, 0);
      int n = connect(sockfd, (struct sockaddr*)&serv, sizeof(serv));
      if (n >= 0) {
         printf("PORT %8d  .....  OPEN   (tcp)\n", i);
      } else {
         fflush(stdout);
      }
      close(sockfd);
   }

   // bzero(&serv, sizeof(serv));
   // serv.sin_addr.s_addr = inet_addr("0.0.0.0");
   // serv.sin_family = AF_INET;
   // UDP
   // printf("looking for open udp ports\n");
   // bzero(recvbuf, sizeof(recvbuf));
   // for (int i=20; i<=65535; i++) {
   //    serv.sin_port = htons(i);
   //    udps = socket(AF_INET, SOCK_DGRAM, 0);
   //    icmpr = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);

   //    bind(sockfd, (struct sockaddr*)&serv, sizeof(serv));
   //    bind(sockfd, (struct sockaddr*)&serv, sizeof(serv));

   //    char dummy = 0;
   //    int n = sendto(udps, &dummy, 1, 0, (struct sockaddr*)&serv, sizeof(serv));
   //    if (n == 1) {
   //       printf("PORT %8d  .....  OPEN   (udp)\n", i);
   //    } else {
   //       fflush(stdout);
   //    }
   //    
   //    // if it failed, wait for a bit and try again
   //    sleep(1);
   //    n = sendto(udps, &dummy, 1, 0, (struct sockaddr*)&serv, sizeof(serv));
   //    if (n == 1) {
   //       printf("PORT %8d  .....  OPEN   (udp)\n", i);
   //    } else {
   //       fflush(stdout);
   //    }

   //    close(sockfd);
   //    close(icmpr);
   // }
}

int main(int argc, char* argv[]) {
   if(argc < 2) {
      printf("performing only port scan since netmask was not provided.\n");
      scan_ports();
      fflush(stdout);
   }
   else {
      scan_ports();
      fflush(stdout);
      lookup_live_ips(argv[1]);
   }
   return 0;
}
