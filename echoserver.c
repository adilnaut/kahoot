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
#define MAXUSERS 1020
#define MAXGROUPS 100
#define MAXTOKENS 100
#define MAXQ 100

// UPD, architecture is changed, groups and quizes will both be implemented in the main thread
// however, time costly operations, like file sending will be implemented in the threads


// function and structures to parse strings
// not thread safe

char func_tokens[MAXTOKENS][BUFSIZE];
int func_tcount = 0;

void parse_string(char* buf, int cc){
	char func_tokens[MAXTOKENS][BUFSIZE];
	int command_count = 0;
	int func_tcount = 0;
	for(int i; i < cc;i++){
		if (buf[i] == '|'){
			func_tokens[func_tcount][command_count] = '\0';
			command_count = 0;
			func_tcount++;
			continue;
		}
		if (buf[i] != '\n'){
			func_tokens[func_tcount][command_count] = buf[i];
			command_count++;
		}
	}
}

struct Quiz{
	int answered; // how many people answered
	int answered_right; // how many people answered correctly
	int group_is_full; // 1 if full, 0 if not full
	char Questions[MAXQ][BUFSIZE];
	char Answers[MAXQ][BUFSIZE];
	char winner_name[BUFSIZE];
	int num_questions;
	char result_string[BUFSIZE]; //quiz statistics
};
struct Group{
	char topic[BUFSIZE];
	char groupname[BUFSIZE];
	int groupsize; // maximum groupsize
	int current_groupsize;
	struct User* members[MAXUSERS];
	struct Quiz quiz;
	int leader_fid;
	// fd_set read_set, a_set; // file descriptors for the groups
};

struct User{
	int fid;// unique
	char * name;
	int score;
	int groupnum;
	int is_in_group; // 0 if not in group, 1 otherwise
	// in order to write logic sequentially using multiplexing,
	// we need to keep and properly propagate last state, where logic ended
	//because each time user have written something, there will be a new iteration
	// initialy should be set to -1
	int state;

};
struct User users[MAXUSERS];
int num_of_users = 0;
struct Group groups[MAXGROUPS];
int num_of_groups = 0;
//returns user id by file descriptor, returns -1 if user not found
int get_user_id_by_fid(int fid_in){
	for(int i = 0; i < num_of_users; i++){
		if (fid_in == users[i].fid){
			return i;
		}
	}
	return -1;
}

void joined_group(int group_index, int user_id){
	users[user_id].is_in_group = 1;
	users[user_id].groupnum = group_index;
}
void leaved_group(int user_id){
	users[user_id].is_in_group = 0;
}



// in this thread server will receive the file and parse for quiz
void *thread_for_file( void *ign ){
	int i = ( int ) ign;
	int fd = groups[i].leader_fid;
	char buf[1];
	int file_size;
	char Questions[1000][BUFSIZE];
	char Answers[1000][BUFSIZE];
	int num_questions = 0;
	int count = 0;
	int prev_new_line = 0;
	int prev_question = 0;
	int cc;

	char tokens[MAXTOKENS][BUFSIZE];
	int command_count = 0;
	int tcount = 0;
	int i = 0;
	while (cc = read( fd, buf, 1) > 0){
		if (buf[i] == '|'){
			if (tcount > 0){
				break;
			}
			tokens[tcount][command_count] = '\0';
			command_count = 0;
			tcount++;
			continue;
		}
		if (buf[i] != '\n'){
			tokens[tcount][command_count] = buf[i];
			command_count++;
		}
		i++;
	}
	// add bad quiz message handling
	if ( strcmp(tokens[0], "QUIZ") != 0 ){

	}
	file_size = atoi(tokens[1]);
	// code copied from my own last submission of homework4,
	// this code parses file character by character
	for(int j = 0; j < file_size; j++ ){
		if ( (cc = read( fd, buf, 1)) > 0){
			char c  = buf[0];
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
	}
	struct Quiz new_quiz = {0,0,0,Questions, Answers, "", num_questions,""};
	groups[i].quiz = new_quiz;
}

// linked list groups implementation, slow and unefficient, but straignforward
// returns nothing, because we firstly check whether such group exist
void remove_from_groups(char *groupname){
  int group_index;
  for(int i = 0; i < num_of_groups; i++){
    if ( strcmp(groups[i].groupname, groupname) == 0 ){
      group_index = i;
      break;
    }
  }
  for(int i = group_index; i < num_of_groups - 1; i++){
    groups[i] = groups[i + 1];
  }
  num_of_groups--;
}
//returns nothing because in order to use, firslty check whether it is available name
void add_to_groups(struct Group new_group){
  num_of_groups++;
  groups[num_of_groups] = new_group;
}
// returns 0, if no group with the such name
// returns 1, otherwise
// if there will be time, rewrite to make it more efficient with the hashing
int is_in_groups(char * name_to_check){
  for(int i = 0; i < num_of_groups; i++){
    if ( strcmp(groups[i].groupname, name_to_check) == 0 ){
        return 1;
    }
  }
  return 0;
}

struct Group get_group_by_name( char * gname){
  for(int i = 0; i < num_of_groups; i++){
    if ( strcmp(groups[i].groupname, gname) == 0 ){
        return groups[i];
    }
  }
}

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
  fd_set a_set;


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
  FD_ZERO(&a_set);
  FD_SET(msock, &a_set);
  while(1){

    // file descriptors we need each time will appear in copy_read_Set
    memcpy( (char *)&read_set, (char *)&a_set, sizeof(read_set));

    // wait for the sockets who are ready to read or write, or throw an exception
    if ( select(nfds, &read_set, (fd_set *)0, (fd_set *)0,
				(struct timeval *)0) < 0 ){
      fprintf( stderr, "server select: %s\n", strerror(errno) );
      exit(-1);
    }

    // select is unblocked
    if ( FD_ISSET( msock, &read_set ) )
    {

      alen = sizeof(addr);
      ssock = accept( msock, (struct sockaddr *)&addr, &alen);
      if (ssock < 0){
        fprintf( stderr, "accept: %s\n", strerror(errno) );
				exit(-1);
      }
      // add new client to the socket set
      FD_SET( ssock, &a_set );
      //nfds is the highest-numbered file descriptor in any of the three sets, plus 1.
      // so if we have to check whether new socket + 1  has higher number than ndfs
      if (ssock + 1 > nfds){
        nfds = ssock + 1;
      }
       char * message;

       message = "OPENGROUPS";

       for(int i = 0; i < num_of_groups; i++){
         char outs[BUFSIZE];
         snprintf(outs, sizeof(outs), "|%s|%s|%i|%i", groups[i].topic, groups[i].groupname, groups[i].groupsize, groups[i].current_groupsize);
         strcat(message, outs);
       }
       strcat(message, "\r\n");
       //check if message string is  terminated correctly
       write( fd, message, strlen(message) );
    }
    // check other file disriptors than main
    for ( fd = 0; fd < nfds; fd++){
      // printf("%i - th file disriptor loop\n", fd);
          if ( fd != msock && FD_ISSET(fd, &read_set)){
						int user_id = get_user_id_by_fid(fd);
						int state = users[user_id].state;
            // printf("%i-th file discriptor is ready to be read\n", fd);
             if ( (cc = read( fd, buf, BUFSIZE)) <= 0 ){
               printf("Client has gone.\n");
               close(fd);
               FD_CLR( fd, &a_set);
               // nfds should stay max, but if max is deleted, decrement it
               if (nfds == fd + 1){
                 nfds -= 1;
               }
             } else{
               buf[cc] = '\0';
               printf("The client says: %s\n", buf);
               // parse user's commands
               char **commands;
               int count = 0;
               parse_string(buf, cc);
							 commands = func_tokens;
							 count = func_tcount;

               if (strcmp( commands[0], "GROUP") == 0 ){
									 char * response;
									 if (is_in_groups( commands[1] ) == 1){
										 response = "BAD";
										 write( fd, response, strlen(response) );
									 } else{
										 struct User members[MAXUSERS];
										 struct Group new_group = {commands[1], commands[2], commands[3], 0, members, NULL, fd};
										 response = "QUIZ";
										 write( fd, response, strlen(response) );
										 // potentially dangerous
										 // should rewrite it
										 pthread_t thread;
	    					 		 int temp = num_of_groups;
										 groups[num_of_groups] = new_group;
										 num_of_groups++;
	    			 				 pthread_create(&thread, 0, thread_for_file, (void *) temp);
									 }

               }
							 if (strcmp( commands[0], "CANCEL") == 0 ){

							 }
							 if (strcmp( commands[0], "JOIN") == 0 ){

							 }
							 if (strcmp( commands[0], "LEAVE") == 0 ){

							 }
							 if (strcmp( commands[0], "GETOPENGROUPS") == 0 ){

							 }



               // write( fd, buf, strlen(buf) );
             }


          }
    }

  }








}
