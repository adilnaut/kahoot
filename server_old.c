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
#include <stdlib.h>


#define QLEN 5
#define BUFSIZE 4096
#define NUMTHREADS 1020

int passivesock( char *service, char *protocol, int qlen, int *rport);

// known udesired behavior:
// 1. When the number of people connected, but doesn't joined yet is desirable, quiz starts

pthread_mutex_t mutex ;
// initializing conds
//http://pubs.opengroup.org/onlinepubs/7908799/xsh/pthread_cond_init.html
// ref start
pthread_cond_t cond = PTHREAD_COND_INITIALIZER;
pthread_cond_t cond_quiz = PTHREAD_COND_INITIALIZER;
// ref end

pthread_mutex_t mutex_admin;
pthread_mutex_t mutex_group_full;
pthread_mutex_t mutex_quiz;
pthread_mutex_t mutex_q;
sem_t first;

int msock; //main socket
int curr_num = 0;
int lnum = 1;
int answered = 0;
int answered_right = 0;
int group_is_full = 0;
char Questions[1000][BUFSIZE];
char Answers[1000][BUFSIZE];
int num_questions = 0;
char winner_name[BUFSIZE];
char result_string[BUFSIZE];


void *thread_per_client( void *ign ){
  // better rewrite and empy one buffer after every read
  char buf[BUFSIZE];
  char buf1[BUFSIZE];
  char buf2[BUFSIZE];
  char name[BUFSIZE];
  struct sockaddr_in fsin;
  int alen;
  int ssock; //client socket
  int cc;
  int name_count = 0;
  char *out_s;
  int score = 0;
  alen = sizeof(fsin);
  ssock = accept(msock, (struct sockaddr *)&fsin, &alen );
  if (ssock < 0){
    fprintf(stderr, "accept: %s\n", strerror(errno) );
    exit(-1);
  }
  printf( "A client has arrived for echoes.\n" );
	fflush( stdout );
  int admin = 0;
  pthread_mutex_lock( &mutex_admin );
  if (curr_num == 0) {
    curr_num++;
    admin = 1;
  } else{
    printf("Waiting for the first\n");
    fflush( stdout );
    // strange usage of semaphore i guess, but it works ¯\_(ツ)_/¯
    sem_wait( &first );
    sem_post( &first );

    // pthread_cond_wait( &cond_admin, &mutex_admin);
    printf("First is unlocked\n");
    fflush( stdout );
  }
  pthread_mutex_unlock( &mutex_admin );

  if ( admin ){
    out_s = "QS|ADMIN\r\n";
    write(ssock, out_s, strlen(out_s));

    if ( (cc = read(ssock, buf, BUFSIZE)) <= 0 ){
      printf("The client has gone.\n");
      curr_num--;
      close(ssock);
    }
    else{
      int i;
      char lnum_a[BUFSIZE];
      int count = 0;
      int lnum_count = 0;
      name_count = 0;
      // bad parsing
      for(int i; i < cc;i++){
        if ( (count == 1) && (buf[i]) != '|'){
          name[name_count] = buf[i];
          name_count++;
        }
        if ( (count > 1) && (buf[i]) != '\r'){
          lnum_a[lnum_count] = buf[i];
          lnum_count++;
        }
        if (buf[i] == '|'){
          count++;
        }
      }
      lnum_a[lnum_count] = '\0';
      lnum = atoi(lnum_a);
      printf("Lnum: %i\n", lnum);
      sem_post( &first );

      out_s = "WAIT\r\n";
      if ( write( ssock, out_s, strlen(out_s)) < 0){
        curr_num--;
        printf("The client has gone.\n");
        close( ssock);
      }
      printf("Curr num:%i, curr lnum:%i\n", curr_num, lnum);
      pthread_mutex_lock( &mutex );
      // block current thread untill signaled that all group is connected
      pthread_cond_wait( &cond, &mutex );
      pthread_mutex_unlock( &mutex );
    }
  }

  pthread_mutex_lock( &mutex_group_full );
  if (!group_is_full){
    if (!admin){
      curr_num++;
    }
    if (curr_num > lnum){
      group_is_full = 1;
    }
  }
  pthread_mutex_unlock( &mutex_group_full );

  if (!group_is_full){
      if (!admin) {
        out_s = "QS|JOIN\r\n";
        write(ssock, out_s, strlen(out_s));
        if ( (cc = read(ssock, buf2, BUFSIZE)) <= 0 ){
          curr_num--;
          printf("The client has gone.\n");
          close(ssock);
        }
        else{
          int count = 0;
          name_count = 0;
          for(int i; i < cc;i++){
            if ( (count == 1) && ( (buf2[i]) != '\n') ) {
              name[name_count] = buf2[i];
              name_count++;
            }
            if (buf2[i] == '|'){
              count++;
            }
          }
          if (curr_num == lnum ){
              out_s = "WAIT\r\n";
              write(ssock, out_s, strlen(out_s));
              pthread_mutex_lock( &mutex );
              printf("Before broadcast\n");
              //signal all threads that group is full
              pthread_cond_broadcast(&cond);
              printf("After broadcast\n");

              pthread_mutex_unlock( &mutex );
          } else {
              out_s = "WAIT\r\n";
              if ( write( ssock, out_s, strlen(out_s)) < 0){
                curr_num--;
                printf("The client has gone.\n");
                close( ssock);
              }
              printf("Curr num:%i, curr lnum:%i\n", curr_num, lnum);
              pthread_mutex_lock( &mutex );
              // block current thread untill signaled that all group is connected
              pthread_cond_wait( &cond, &mutex );
              pthread_mutex_unlock( &mutex );
          }

        }
      }

    for(int i = 0; i < num_questions+1;i++){
      char leng[BUFSIZE];
      //use of snprintf
      //https://stackoverflow.com/questions/308695/how-do-i-concatenate-const-literal-strings-in-c
      printf("rgege\n" );
      char outs[BUFSIZE];
      snprintf(outs, sizeof(outs), "QUES|%lu|%s", strlen(Questions[i]), Questions[i]);
      printf("ergrg\n" );
      write(ssock, outs, strlen(outs));
      int ans_first = 0;
      if ( (cc = read(ssock, buf1, BUFSIZE)) <= 0 ){
        printf("The client has gone.\n");

        pthread_mutex_lock( &mutex_quiz );
        curr_num--;
        if (curr_num == answered){
        pthread_cond_broadcast( &cond_quiz );
        }
        pthread_mutex_unlock( &mutex_quiz );

        close(ssock);
        break;
      }
      else{
        printf("Client said %s\n", buf1);
        fflush( stdout );
        char answ[BUFSIZE];
        int answ_count = 0;
        int count = 0;
        for(int i; i < cc;i++){
          if ( (count == 1) && ( (buf1[i]) != '\n') ) {
            answ[name_count] = buf1[i];
            answ_count++;
          }
          if (buf1[i] == '|'){
            count++;
          }
        }
        pthread_mutex_lock( &mutex_q );
        answered++;
        printf("Client's Answer:%s\n", buf1);
        printf("True Answer%s\n", Answers[i]);
        buf1[( strlen(buf1)-1 )] = '\0';
        if ( strcmp(buf1,Answers[i]) == 0){
          answered_right++;
          score += 1;
          if (answered_right == 1){
            strcpy(winner_name, name);
            score += 1;
            ans_first = 1;
          }
        } else{
          if (strcmp(buf1,"NOANS") == 0){
            score += 0;
          } else{
            score -= 1;
          }
        }
        if ( i == num_questions ){
          snprintf(result_string, sizeof(result_string), "%s|%s|%d",result_string, name,score);
        }

        pthread_mutex_unlock( &mutex_q );
        printf("Name: %s, answered = %i, curr_num = %i\n", name, answered, curr_num);
        fflush( stdout );
        if (answered == curr_num){
          pthread_mutex_lock( &mutex_quiz );
          pthread_cond_broadcast( &cond_quiz );
          pthread_mutex_unlock( &mutex_quiz );
          answered = 0;
          answered_right = 0;
        } else{
          pthread_mutex_lock( &mutex_quiz );
          pthread_cond_wait(&cond_quiz, &mutex_quiz );
          pthread_mutex_unlock( &mutex_quiz );
        }

      }
      snprintf(outs, sizeof(outs), "WIN|%s", winner_name);
      write(ssock, outs, strlen(outs));
      if ( i == num_questions){
        write(ssock, result_string, strlen(result_string));
      }
    }

  } else{
    out_s = "QS|FULL\r\n";
    write(ssock, out_s, strlen(out_s));
    printf("The client has gone.\n");
    close(ssock);
  }
  curr_num = 0;
  close(ssock);
}


int main(int argc, char *argv[]){
  char * service; //port
  char * filename;
  pthread_mutex_init( &mutex, NULL );
  pthread_mutex_init( &mutex_admin, NULL );
  pthread_mutex_init( &mutex_group_full, NULL );
  pthread_mutex_init( &mutex_admin, NULL );
  pthread_mutex_init( &mutex_quiz, NULL );
  pthread_mutex_init( &mutex_q, NULL );
  sem_init( &first, 0, 0);

  int rport = 0; // shows whether port is set by OS


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

  // reading file by characters
  // https://stackoverflow.com/questions/3463426/in-c-how-should-i-read-a-text-file-and-print-all-strings
  // ref start
  int c;
  FILE *file;
  file = fopen(filename, "r");
  //assumption that number of questions will not be more than 1000

  int count = 0;
  int prev_new_line = 0;
  int prev_question = 0;
  if (file) {
      while ((c = getc(file)) != EOF){
        if (prev_question){
          Answers[num_questions][count] = c;
        } else{
          Questions[num_questions][count] = c;
        }
        if (c == '\n'){
          if (prev_new_line) {
            if (prev_question){
              prev_question = 0;
              Answers[num_questions][count - 1] = '\0';
              num_questions++;
            } else{
              Questions[num_questions][count - 1] = '\0';
              prev_question = 1;
            }
            count = 0;
          } else{
            count++;
          }
          prev_new_line = 1;
        } else{
          prev_new_line = 0;
          count++;
        }
      }
      fclose(file);
  }

  //вынужденный костыль
  Answers[num_questions][strlen(Answers[num_questions])-1] = '\0';
  strcpy(result_string, "RESULT|");
  // ref end


  msock = passivesock( service, "tcp", QLEN, &rport);
  //rport after passivesock returns port set, if set by OS
  if (rport){
    printf("Server: port %i\n", rport);
    fflush( stdout);
  }

  pthread_t threads[NUMTHREADS];
  int status, i;


  while(1){
  for (i = 0; i < NUMTHREADS;i++){
    status = pthread_create( &threads[i], NULL, thread_per_client, NULL );

    if (status != 0){
      printf( "pthread_create error %d.\n", status );
			exit( -1 );
    }
  }
  }


  for (i = 0; i < NUMTHREADS; i++)
    pthread_join( threads[i], NULL);
  pthread_exit( 0 );
}
