#include "stackexchange.h"

#include "httpclient/httpclient.h"
#include <jansson.h>
#include <string.h>


#define FILTER_SEARCH "!-MOdcb64tAbSDB630lrfCo5NzmAoAybzt"
#define FILTER_ANSWERS ")3so9SF-6Is5yiSumyL5a1r9-087in*H4yg-LXvJ6eI8PiyjAosj3iz)"

/* default values for search query, used in fields where user have failed to provide any data */
static const stack_query_t default_query = {"", 5, 1, NULL, 0, "stackoverflow"};

/* fast macro for safely freeing a variable */
#define SAFE_FREE(x) if ((x)) {free(x); x = NULL; }

static user_t* parse_json_user(json_t* user, user_t* result)
{
  if (result == NULL)
  {
    result = (user_t*) malloc(sizeof(user_t));
    memset(result, 0, sizeof(user_t));
  }

  json_t* reputation = json_object_get(user, "reputation");
  if (json_is_integer(reputation))
    result->reputation = json_integer_value(reputation);

  json_t* user_id = json_object_get(user, "user_id");
  if (json_is_integer(user_id))
    result->user_id = json_integer_value(user_id);

  json_t* display_name = json_object_get(user, "display_name");
  if (json_is_string(display_name))
    result->display_name = (char*) json_string_value(display_name);
  
  SAFE_FREE(reputation);
  SAFE_FREE(user_id);
  SAFE_FREE(display_name);
  return result;
}

static post_t* parse_json_post(json_t* post, post_t* result)
{
  if (result == NULL)
  {
    result = (post_t*) malloc(sizeof(post_t));
    memset(result, 0, sizeof(post_t));
  }

  json_t* owner = json_object_get(post, "owner");

  if (json_is_object(owner))
    result->owner = parse_json_user(owner, result->owner);

  json_t* body = json_object_get(post, "body_markdown");
  if (json_is_string(body))
    result->body = (char*) json_string_value(body);

  json_t* post_id = json_object_get(post, "post_id");
  if (!json_is_integer(post_id))
    post_id = json_object_get(post, "answer_id");
  if (!json_is_integer(post_id))
    post_id = json_object_get(post, "question_id");

  if (json_is_integer(post_id))
    result->post_id = (uint64_t) json_integer_value(post_id);
  
  json_t* score = json_object_get(post, "score");
  if (json_is_integer(score))
    result->score = (int32_t) json_integer_value(score);

  SAFE_FREE(owner);
  SAFE_FREE(body);
  SAFE_FREE(post_id);
  SAFE_FREE(score);
  return result;
}

static answer_t* parse_json_answer(json_t* answer, answer_t* result)
{
  if (result == NULL)
  {
    result = (answer_t*) malloc(sizeof(answer_t));
    memset(result, 0, sizeof(answer_t));
  }
  
  result->post = parse_json_post(answer, result->post);

  json_t* is_accepted = json_object_get(answer, "is_accepted");
  if (json_is_boolean(is_accepted))
    result->is_accepted = (json_typeof(is_accepted) == JSON_TRUE);

  return result;
}

static question_t* parse_json_question(json_t* question, question_t* result)
{
  if(result == NULL)
  {
    result = (question_t*) malloc(sizeof(question_t));
    memset(result, 0, sizeof(question_t));
  }

  result->post = parse_json_post(question, result->post);

  json_t* title = json_object_get(question, "title");
  if (json_is_string(title))
    result->title = (char*) json_string_value(title);

  json_t* link = json_object_get(question, "link");
  if (json_is_string(link))
    result->link = (char*) json_string_value(link);
  
  json_t* answers = json_object_get(question, "answers");
  if (json_is_array(answers))
  {
    result->answer_count = json_array_size(answers);
    result->answers = (answer_t**) malloc(sizeof(answer_t*) * result->answer_count);

    for (uint16_t i = 0; i < result->answer_count; ++i)
    {
      json_t* answer = json_array_get(answers, i);
      if (json_is_object(answer))
        result->answers[i] = parse_json_answer(answer, result->answers[i]);
      SAFE_FREE(answer);
    }
  }
  
  json_t* is_answered = json_object_get(question, "is_answered");
  if (json_is_boolean(is_answered))
    result->is_answered = (json_typeof(is_answered) == JSON_TRUE);

  json_t* view_count = json_object_get(question, "view_count");
  if (json_is_integer(view_count))
    result->view_count = json_integer_value(view_count);

  json_t* tags = json_object_get(question, "tags");
  if (json_is_array(tags))
  {
    result->tag_count = json_array_size(tags);
    result->tags = (char**) malloc(sizeof(char*) * result->tag_count);

    for (uint16_t i = 0; i < result->tag_count; ++i)
    {
      json_t* tag = json_array_get(tags, i);
      if (json_is_string(tag))
        result->tags[i] = (char*) json_string_value(tag);
      SAFE_FREE(tag);
    }
  }

  SAFE_FREE(title);
  SAFE_FREE(link);
  SAFE_FREE(answers);
  SAFE_FREE(is_answered);
  SAFE_FREE(view_count);
  SAFE_FREE(tags);
  return result;
}




void stack_query_tag_add(stack_query_t* query, char* tag)
{
  if (query->tag_count == 0 || query->tags == NULL)
    query->tags = (char**) malloc(sizeof(char*));
  else
    query->tags = (char**) realloc(query->tags, sizeof(char*) * (query->tag_count + 1));

  char** new_entry = &query->tags[query->tag_count];
  *new_entry = (char*) malloc(sizeof(char) * strlen(tag));

  strcpy(*new_entry, tag);
  ++query->tag_count;
}


stack_search_res_t* stack_search(stack_query_t* query)
{
  stack_search_res_t* result = (stack_search_res_t*) malloc(sizeof(stack_search_res_t));
  memset(result, 0, sizeof(stack_search_res_t));

  /* fill in default values where the user haven't included anything */
  if (query->in_title == NULL)
    query->in_title = default_query.in_title;
  if (query->results == 0)
    query->results = default_query.results;
  if (query->page == 0)
    query->page = default_query.page;
  if (query->tag_count == 0)
  {
    if (query->tags != NULL)
      free(query->tags);
    
    query->tags = NULL;
  }
  if (query->site == NULL)
    query->site = default_query.site;

  uint16_t query_len = strlen("api.stackexchange.com/2.2/search?page=XX&pagesize=XX&order=desc&sort=votes&intitle=&site=&tagged=");
  query_len += strlen(query->in_title);
  query_len += strlen(query->site);
  query_len += strlen(FILTER_SEARCH);
  for (uint8_t i = 0; i < query->tag_count; ++i)
  {
    query_len += strlen(query->tags[i]) + 1;
  }

  char* search_string = (char*) malloc(query_len);

  sprintf(search_string, "api.stackexchange.com/2.2/search?page=%d&pagesize=%d&order=desc&sort=votes&intitle=%s&site=%s&filter=%s&tagged=", query->page, query->results, query->in_title, query->site, FILTER_SEARCH);

  for (uint8_t i = 0; i < query->tag_count; ++i)
  {
    sprintf(search_string, "%s%s;", search_string, query->tags[i]);
  }
  
  http_response_t* http_resp = http_request(search_string, HTTP_REQ_GET, NULL, 0); 
  
  free(search_string);

  if (http_resp->status != HTTP_SUCCESS)
  {
    free(result);
    return NULL;
  }
  
  json_error_t error;
  json_t* root = json_loads(http_resp->contents, 0, &error);

  if (!root)
  {
    free(result); 
    return NULL;
  }

  json_t* items = json_object_get(root, "items");
  if (!json_is_array(items))
  {
    free(result);
    free(root);
    return NULL;
  }

  result->question_count = json_array_size(items);
  result->questions = (question_t**) malloc(sizeof(question_t*) * result->question_count);

  for (uint8_t i = 0; i < result->question_count; ++i)
  {
    json_t* question = json_array_get(items, i);
    if (json_is_object(question))
      result->questions[i] = parse_json_question(question, result->questions[i]);
    SAFE_FREE(question);
  }
  json_decref(root);
  return result;
}

void stack_question_fill_answers(question_t* question)
{
  uint16_t query_len = strlen("api.stackexchange.com/2.2/questions/XXXXXXX?order=desc&sort=votes&site=&filter=");

  if (question->site == NULL)
    question->site = default_query.site;

  query_len += strlen(question->site);
  query_len += strlen(FILTER_ANSWERS);

  char* search_string = (char*) malloc(query_len);

  sprintf(search_string, "api.stackexchange.com/2.2/questions/%ld?order=desc&sort=votes&site=%s&filter=%s", question->post->post_id, question->site, FILTER_ANSWERS);

  http_response_t* http_resp = http_request(search_string, HTTP_REQ_GET, NULL, 0); 
  
  free(search_string);

  if (http_resp->status != HTTP_SUCCESS)
  {
    return; 
  }
  
  json_error_t error;
  json_t* root = json_loads(http_resp->contents, 0, &error);

  if (!root)
  {
    return;
  }

  json_t* items = json_object_get(root, "items");
  if (!json_is_array(items))
  {
    free(root);
    return; 
  }

  json_t* item = json_array_get(items, 0);
  if (json_is_object(item))
  {
    parse_json_question(item, question);
  }

  SAFE_FREE(item);
  json_decref(root);
  return;
}

