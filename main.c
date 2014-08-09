#include "stackexchange.h"
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#define DO_PRINT (0)

static void print_question(question_t* q)
{
  printf("TITLE: %s\nAUTHOR: %s\nSCORE: %i\
      \n%s\n\n----------------------------------------------------------------------\
      \nANSWERS:\n\n", q->title, q->post->owner->display_name, q->post->score, q->post->body);
  for (uint16_t i = 0; i < q->answer_count; ++i)
  {
    printf("------------\nUSER: %s\nSCORE: %i\n%s\n\n", q->answers[i]->post->owner->display_name,
        q->answers[i]->post->score, q->answers[i]->post->body);
  }
}


int main()
{
  char* tags[] = {"gcc", "c"};
  stack_query_t query = {"makefile", 5, 1, tags, 2, "stackoverflow"};

  stack_search_res_t* result = stack_search(&query);

  for (uint8_t i = 0; i < result->question_count; ++i)
  {
    stack_question_fill_answers(result->questions[i]);
#if DO_PRINT
    print_question(result->questions[i]);
    printf("--------------------------------------------------------------------------\n");
    printf("--------------------------------------------------------------------------\n");
#endif
    stack_question_free(result->questions[i]);
  }
  free(result->questions);
  free(result);
}
