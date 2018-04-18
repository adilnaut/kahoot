#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <sys/select.h>

#define BUFSIZE 4096

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
