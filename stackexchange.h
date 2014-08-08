#ifndef _STACKEXCHANGE_H__
#define _STACKEXCHANGE_H__

#include <stdint.h>
#include <stdbool.h>


typedef struct
{
  int32_t reputation;
  uint64_t user_id;
  char* display_name;
} user_t;

typedef struct _answer answer_t;


typedef struct
{
  char* body;
  user_t* owner;
  uint64_t post_id;
  int32_t score;
} post_t;

typedef struct
{
  char* body;
  user_t* owner;
  uint64_t comment_id;
  int32_t score;
  post_t* post; /* either question or answer */
} comment_t;

typedef struct
{
  post_t* post;
  char* title;
  char* link;
  uint16_t answer_count;
  answer_t** answers;
  bool is_answered;
  uint32_t view_count;
  uint16_t tag_count;
  char** tags;
  char* site;
} question_t;

struct _answer
{
  post_t* post;
  question_t* question;
  bool is_accepted;
};

typedef struct
{
  char* in_title;
  uint8_t results;
  uint8_t page;
  char** tags;
  uint8_t tag_count;
  char* site; 
} stack_query_t;

typedef struct
{
  question_t** questions;
  uint8_t question_count;
} stack_search_res_t;


void stack_query_tag_add(stack_query_t* query, char* tag);

stack_search_res_t* stack_search(stack_query_t* query);

void stack_question_fill_answers(question_t* question);

void stack_question_free(question_t* question);

#endif
