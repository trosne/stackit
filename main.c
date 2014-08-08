#include "stackexchange.h"
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

int main()
{
  char* tags[] = {"gcc", "c"};
  stack_query_t query = {"makefile", 5, 1, tags, 2, "stackoverflow"};

  stack_search_res_t* result = stack_search(&query);

  for (uint8_t i = 0; i < result->question_count; ++i)
  {
    printf("TITLE: %s\nUSER: %s\n", 
        result->questions[i]->title, 
        result->questions[i]->post->owner->display_name);

    stack_question_free(result->questions[i]);
  }
  free(result->questions);
  free(result);
}
