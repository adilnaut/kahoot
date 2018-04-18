#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <errno.h>
#include <pthread.h>
#include <semaphore.h>
#include <sys/select.h>

#define QLEN 5
#define BUFSIZE 4096
#define NUMTHREADS 1020

int passivesock( char *service, char *protocol, int qlen, int *rport);


int main(int argc, char *argv[]){
  char * service; //port
  char * filename;
  int msock; // main socket
  int ssock; //client's socket
  int rport = 0; // shows whether port is set by OS
  char buf[BUFSIZE];
  struct sockaddr_in addr;
  int nfds;
  int cc;
  int alen;
  fd_set read_set;
  fd_set fid_set; // I don't yet understand why, but each time read file descriptors should be copied


  int fd;

  switch (argc) {
    case 1:filename = argv[1];
      //OS chooses port and tells the user
      rport = 1;
      break;
    case 2:
      //OS chooses port and tells the user
      filename = argv[1];
      rport = 1;
      break;
    case 3:
      // use provided by user port
      filename = argv[1];
      service = argv[2];
      break;
    default:
      fprintf( stderr, "usage: server [port]\n" );
      exit(-1);
  }
  msock = passivesock( service, "tcp", QLEN, &rport);
  if (rport){
    printf("Chosen port is: %i\n",rport);
    fflush(stdout);
  }
  //from man select
  //nfds is the highest-numbered file descriptor in any of the three sets, plus 1.
  nfds = msock + 1;
  FD_ZERO(&fid_set);
  FD_SET(msock, &fid_set);
  while(1){
    printf("IN the loop\n");
    // file descriptors we need each time will appear in copy_read_Set
    memcpy( (char *)&read_set, (char *)&fid_set, sizeof(read_set));

    // wait for the sockets who are ready to read or write, or throw an exception
    if ( select(nfds, &read_set, (fd_set *)0, (fd_set *)0,
				(struct timeval *)0) < 0 ){
      fprintf( stderr, "server select: %s\n", strerror(errno) );
      exit(-1);
    }

    // select is unblocked
    if ( FD_ISSET( msock, &fid_set ) )
    {
      int ssock;
      alen = sizeof(addr);
      ssock = accept( msock, (struct sockaddr *)&addr, &alen);
      if (ssock < 0){
        fprintf( stderr, "accept: %s\n", strerror(errno) );
				exit(-1);
      }
      // add new client to the socket set
      FD_SET( ssock, &fid_set );
      //nfds is the highest-numbered file descriptor in any of the three sets, plus 1.
      // so if we have to check whether new socket + 1  has higher number than ndfs
      if (ssock + 1 > nfds){
        nfds = ssock + 1;
      }

    }
    // check other file disriptors than main
    for ( fd = 0; fd < nfds; fd++){
      printf("%i - th file disriptor loop\n", fd);
          if ( fd != msock && FD_ISSET(fd, &read_set)){
            printf("%i-th file discriptor is ready to be read\n", fd);
             if ( (cc = read( fd, buf, BUFSIZE)) <= 0 ){
               printf("Client has gone.\n");
               // don't know why we need (void) casting here when we calling the function
               (void) close(fd);
               FD_CLR( fd, &fid_set);
               // nfds should stay max, but if max is deleted, decrement it
               if (nfds == fd + 1){
                 nfds -= 1;
               }
             } else{
               buf[cc] = '\0';
               printf("The client says: %s\n", buf);

               write( fd, buf, strlen(buf) );
             }


          }
    }

  }








}
