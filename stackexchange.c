#include "stackexchange.h"

#include "httpclient/httpclient.h"
#include <jansson.h>
#include <string.h>

#define SAFE_FREE(x) if((x) != NULL) { free(x); x = NULL; }
#define FILTER_SEARCH "!-MOdcb64tAbSDB630lrfCo5NzmAoAybzt"
#define FILTER_ANSWERS ")3so9SF-6Is5yiSumyL5a1r9-087in*H4yg-LXvJ6eI8PiyjAosj3iz)"

/* default values for search query, used in fields where user have failed to provide any data */
static const stack_query_t default_query = {"", 5, 1, NULL, 0, "stackoverflow"};


/* Getting a string directly from a json node is not safe. 
* this function safely copies the string to new memory
*/
static char* _json_string_get(json_t* str_obj)
{
  const char* str = json_string_value(str_obj);
  char* result = (char*) malloc(strlen(str) + 1);
  strcpy(result, str);
  /* str is free'd by json_decref(). Leaving this to local implementation */
  return result;
}


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
  if (json_is_string(display_name) && result->display_name == NULL)
  {
    result->display_name = _json_string_get(display_name);
  }
  
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
  if (json_is_string(body) && result->body == NULL)
    result->body = _json_string_get(body);

  json_t* post_id = json_object_get(post, "post_id");
  if (!json_is_integer(post_id))
  {
    post_id = json_object_get(post, "answer_id");
  }
  if (!json_is_integer(post_id))
  {
    post_id = json_object_get(post, "question_id");
  }

  if (json_is_integer(post_id))
    result->post_id = (uint64_t) json_integer_value(post_id);
  
  json_t* score = json_object_get(post, "score");
  if (json_is_integer(score))
    result->score = (int32_t) json_integer_value(score);

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
    result->title = _json_string_get(title);

  json_t* link = json_object_get(question, "link");
  if (json_is_string(link))
    result->link = _json_string_get(link);
  
  json_t* answers = json_object_get(question, "answers");
  if (json_is_array(answers))
  {
    if (result->answers == NULL)
    {
      result->answer_count = json_array_size(answers);
      result->answers = (answer_t**) malloc(sizeof(answer_t*) * result->answer_count);
      memset(result->answers, 0, sizeof(answer_t*) * result->answer_count);
    }

    for (uint16_t i = 0; i < result->answer_count; ++i)
    {
      json_t* answer = json_array_get(answers, i);
      if (json_is_object(answer))
        result->answers[i] = parse_json_answer(answer, result->answers[i]);
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
        result->tags[i] = _json_string_get(tag);
    }
  }

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

  uint16_t query_len = strlen("api.stackexchange.com/2.2/search?page=XX&pagesize=XX&order=desc&sort=votes&intitle=&site=&filter=&tagged=");
  query_len += strlen(query->in_title);
  query_len += strlen(query->site);
  query_len += strlen(FILTER_SEARCH);
  for (uint8_t i = 0; i < query->tag_count; ++i)
  {
    query_len += strlen(query->tags[i]) + 1;
  }

  char* search_string = (char*) malloc(query_len + 1);
  memset(search_string, 1, query_len + 1);

  sprintf(search_string, "api.stackexchange.com/2.2/search?page=%d&pagesize=%d&order=desc&sort=votes&intitle=%s&site=%s&filter=%s&tagged=", query->page, query->results, query->in_title, query->site, FILTER_SEARCH);

  for (uint8_t i = 0; i < query->tag_count; ++i)
  {
    sprintf(search_string, "%s%s;", search_string, query->tags[i]);
  }

  search_string[query_len] = '\0';
  
  http_response_t* http_resp = http_request(search_string, HTTP_REQ_GET, NULL, 0); 
  
  free(search_string);

  if (http_resp == NULL || http_resp->status != HTTP_SUCCESS)
  {
    free(result);
    return NULL;
  }
  
  json_error_t error;
  json_t* root = json_loadb(http_resp->contents, http_resp->length, 0, &error);

  if (!root)
  {
    http_response_free(http_resp);
    free(result); 
    return NULL;
  }

  json_t* items = json_object_get(root, "items");
  if (!json_is_array(items))
  {
    free(result);
    http_response_free(http_resp);
    json_decref(root);
    return NULL;
  }

  result->question_count = json_array_size(items);
  result->questions = (question_t**) malloc(sizeof(question_t*) * result->question_count);
  memset(result->questions, 0, sizeof(question_t*) * result->question_count);

  for (uint8_t i = 0; i < result->question_count; ++i)
  {
    json_t* question = json_array_get(items, i);
    if (json_is_object(question))
      result->questions[i] = parse_json_question(question, NULL);
  }
  json_decref(root); 
  
  http_response_free(http_resp);
  return result;
}

void stack_question_fill_answers(question_t* question)
{
  uint16_t query_len = strlen("api.stackexchange.com/2.2/questions/XXXXXXX?order=desc&sort=votes&site=&filter=");

  if (question->site == NULL)
    question->site = default_query.site;

  query_len += strlen(question->site);
  query_len += strlen(FILTER_ANSWERS);

  char* search_string = (char*) malloc(query_len + 1);

  sprintf(search_string, "api.stackexchange.com/2.2/questions/%ld?order=desc&sort=votes&site=%s&filter=%s", question->post->post_id, question->site, FILTER_ANSWERS);

  http_response_t* http_resp = http_request(search_string, HTTP_REQ_GET, NULL, 0); 
  
  free(search_string);

  if (http_resp->status != HTTP_SUCCESS)
  {
    return; 
  }
  
  json_error_t error;
  json_t* root = json_loadb(http_resp->contents, http_resp->length, 0, &error);


  http_response_free(http_resp);

  if (!root)
  {
    return;
  }

  json_t* items = json_object_get(root, "items");
  if (!json_is_array(items))
  {
    json_decref(root);
    return; 
  }

  json_t* item = json_array_get(items, 0);
  if (json_is_object(item))
  {
    parse_json_question(item, question);
  }
  json_decref(root);
  return;
}

void stack_user_free(user_t* user)
{
  if (user != NULL)
    SAFE_FREE(user->display_name);
  SAFE_FREE(user);
}

void stack_post_free(post_t* post)
{
  if (post != NULL)
  {
    SAFE_FREE(post->body);
    stack_user_free(post->owner);
    SAFE_FREE(post);
  }
}

void stack_answer_free(answer_t* answer)
{
  if (answer != NULL)
  {
    stack_post_free(answer->post);
    SAFE_FREE(answer);
  }
}

void stack_question_free(question_t* question)
{
  if (question != NULL)
  {
    stack_post_free(question->post);
    SAFE_FREE(question->title);
    SAFE_FREE(question->link);
    if (question->answers != NULL)
      for (uint16_t i = 0; i < question->answer_count; ++i)
        stack_answer_free(question->answers[i]);
    SAFE_FREE(question->answers);
    //SAFE_FREE(question->site);
    if (question->tags != NULL)
      for (uint16_t i = 0; i < question->tag_count; ++i)
        SAFE_FREE(question->tags[i]);
    SAFE_FREE(question->tags);
    SAFE_FREE(question);
  }
}

