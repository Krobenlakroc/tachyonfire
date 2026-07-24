#include "th_spawnset.h"
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include "th_system.h"
#include "../th_fopen.h"

//file entry:
//name px py pz time
//name coursename px py pz time

#define MAX_TOKEN_LENGTH 100

typedef enum {
    TOKEN_STRING,
    TOKEN_FLOAT,
    TOKEN_NEWLINE,
    TOKEN_UNKNOWN
} TokenType;


typedef struct {
    TokenType type;
    char value[MAX_TOKEN_LENGTH];
    float float_value; // Add a field to store the floating-point value
} Token;

// static char *safe_strdup(const char *s) {
//     if (s == NULL) {
//         return NULL;
//     }
//
//     size_t len = strlen(s) + 1; // +1 for the null terminator
//     char *copy = (char *)malloc(len);
//
//     if (copy == NULL) {
//         return NULL;
//     }
//
//     memcpy(copy, s, len); // Use memcpy to copy the string including null terminator
//     return copy;
// }

static int is_float(const char* str, double* float_value) {
    char* endptr;
    *float_value = strtod(str, &endptr);
    return *endptr == '\0' ;//&& strchr(str, '.') != NULL;
}

Token defaultToken()
{
  Token ret;
  ret.float_value = 0.0;
  memset(ret.value,0,MAX_TOKEN_LENGTH);
  ret.type = TOKEN_FLOAT;
  return ret;
}

static Token* tokenize(FILE* file,int* count) {
    *count = 0;
    Token* out_tokens = NULL;
    char ch;
    char buffer[MAX_TOKEN_LENGTH];
    int buffer_index = 0;
    bool comment = false;
    while ((ch = fgetc(file)) != EOF) {
        if (buffer_index == 0 && ch == '#')
        {
          comment = true;
          continue;
        }
        else if (comment && ch != '\n' )
        {
          continue;
        }
        else if (comment && ch == '\n' )
        {
          comment = false;
          buffer_index = 0;
          continue;
        }
        else if (isspace(ch) && ch != '\n') {
            if (buffer_index > 0) {
                buffer[buffer_index] = '\0';
                Token token = defaultToken();
                double float_value;

                if (is_float(buffer, &float_value)) {
                    token.type = TOKEN_FLOAT;
                    token.float_value = float_value;
                } else {
                    token.type = TOKEN_STRING;
                }

                strcpy(token.value, buffer);
                out_tokens = realloc(out_tokens,sizeof(Token)*(*count + 1));
                out_tokens[*count] = token;
                *count = *count + 1;

                buffer_index = 0;
            }
        }
        else if (ch == '\n')
        {
          if (buffer_index > 0) {
              buffer[buffer_index] = '\0';
              Token token = defaultToken();
              double float_value;

              if (is_float(buffer, &float_value)) {
                  token.type = TOKEN_FLOAT;
                  token.float_value = float_value;
              } else {
                  token.type = TOKEN_STRING;
              }

              strcpy(token.value, buffer);
              out_tokens = realloc(out_tokens,sizeof(Token)*(*count + 1));
              out_tokens[*count] = token;
              *count = *count + 1;

              buffer_index = 0;
          }

            Token newline_token;
            newline_token.type = TOKEN_NEWLINE;
            newline_token.value[0] = '\0';  // No value for newline token
            out_tokens = realloc(out_tokens,sizeof(Token)*(*count + 1));
            out_tokens[*count] = newline_token;
            *count = *count + 1;
        }
         else {
            buffer[buffer_index++] = ch;
        }
    }

    if (buffer_index > 0) {
        buffer[buffer_index] = '\0';
        Token token = defaultToken();
        double float_value;

        if (is_float(buffer, &float_value)) {
            token.type = TOKEN_FLOAT;
            token.float_value = float_value;
        } else {
            token.type = TOKEN_STRING;
        }

        strcpy(token.value, buffer);

        out_tokens = realloc(out_tokens,sizeof(Token)*(*count + 1));
        out_tokens[*count] = token;
        *count = *count + 1;
    }

    return out_tokens;
}

typedef struct
{
  const char* stringa;
  const char* stringb;
}EquivSetEntry;

static EquivSetEntry* equiv_set = NULL;
static int equiv_set_count = 0;

typedef struct
{
  int count;
  char* string;
}StringSetEntry;

static int addToSet(StringSetEntry** table,int* count,char* str)
{

  for (int i = 0; i < *count; i++) {

    bool equiv_set_found = false;
    for (int i = 0 ; i < equiv_set_count;i++)
    {
      equiv_set_found = equiv_set_found || (strcmp((*table)[i].string,equiv_set[i].stringa) == 0 && strcmp(str,equiv_set[i].stringb) == 0 ) || (strcmp(str,equiv_set[i].stringa) == 0 && strcmp((*table)[i].string,equiv_set[i].stringb) == 0 );
    }

    bool is_equiv = (strcmp((*table)[i].string,str) == 0) || equiv_set_found;

    if (is_equiv)
    {
      (*table)[i].count = (*table)[i].count + 1;
      return (*table)[i].count;
    }
  }


  *table = realloc(*table,sizeof(StringSetEntry)*(*count + 1));
  (*table)[*count].count = 0;
  (*table)[*count].string = str;
  *count = *count + 1;
  return 0;
}



int compare_names(const void* a, const void* b)
{
    th_SpawnsetPair arg1 = *(const th_SpawnsetPair*)a;
    th_SpawnsetPair arg2 = *(const th_SpawnsetPair*)b;

    bool equiv_set_found = false;
    for (int i = 0 ; i < equiv_set_count;i++)
    {
      equiv_set_found = equiv_set_found || (strcmp(arg1.name,equiv_set[i].stringa) == 0 && strcmp(arg2.name,equiv_set[i].stringb) == 0 ) || (strcmp(arg2.name,equiv_set[i].stringa) == 0 && strcmp(arg1.name,equiv_set[i].stringb) == 0 );
    }

    bool is_equiv = (strcmp(arg1.name,arg2.name) == 0) || equiv_set_found;

    if (is_equiv)
    {
      if (arg1.index < arg2.index) return -1;
      if (arg1.index > arg2.index) return 1;
      return 0;
    }

    for (int i = 0 ; i < equiv_set_count;i++)
    {
      if (strcmp(arg1.name,equiv_set[i].stringa) == 0 )
      {
        return strcmp(equiv_set[i].stringb,arg2.name);
      }
      else if (strcmp(arg2.name,equiv_set[i].stringa) == 0 )
      {
        return strcmp(arg1.name,equiv_set[i].stringb);
      }

    }
    return strcmp(arg1.name,arg2.name);

}


th_SpawnsetPair* th_spawnSetLoad(th_Allocator* allocator,const char* filename,int* entries)
{

  equiv_set = malloc(sizeof(EquivSetEntry)*1);
  equiv_set_count = 1;
  equiv_set[0].stringa = "horse";
  equiv_set[0].stringb = "horse_easy";

  FILE* fp = th_fopen(filename,"r");
  if (fp == NULL)
  {
    printf("%s\n","Cannot open spawnset");
    return NULL;
  }
  int token_count = 0;
  Token* tokens = tokenize(fp,&token_count);

  int chunk_index = 0;
  th_SpawnsetPair spawn;
  bool grammar_2string = false;
  spawn.duration = 0.0;

  *entries = 0;
  th_SpawnsetPair* spawns = NULL;

  StringSetEntry* string_set = NULL;
  int string_set_count = 0;

  for (int i = 0; i < token_count; i++) {
    bool terminal = false;
    if (tokens[i].type == TOKEN_NEWLINE && chunk_index == 0)
    {
      chunk_index = 0;
      grammar_2string = false;
      spawn.duration = 0;
      continue;
    }

    if (chunk_index == 0 && tokens[i].type != TOKEN_STRING)
    {
      printf("%s\n","Token sequence doenst start with a string" );
      fclose(fp);
      return NULL;
    }
    else if (chunk_index == 0 && tokens[i].type == TOKEN_STRING)
    {
      //spawn.name = safe_strdup(tokens[i].value);
      strcpy(spawn.name,tokens[i].value);
    }
    else if (chunk_index == 1 && tokens[i].type == TOKEN_FLOAT)
    {
      strcpy(spawn.courseName,"");
      spawn.position.x = tokens[i].float_value;
      grammar_2string = false;
    }
    else if (chunk_index == 1 && tokens[i].type == TOKEN_STRING)
    {
      //spawn.courseName = safe_strdup(tokens[i].value);
      strcpy(spawn.courseName,tokens[i].value);
      grammar_2string = true;
    }
    else if (chunk_index == 2 && tokens[i].type == TOKEN_FLOAT )
    {
        if(grammar_2string)
        {
          spawn.position.x = tokens[i].float_value;
        }
        else
        {
          spawn.position.y = tokens[i].float_value;
        }
    }
    else if (chunk_index == 2 && tokens[i].type == TOKEN_STRING )
    {
      printf("%s\n","Token sequence has too many strings" );
      fclose(fp);
      return NULL;
    }
    else if (chunk_index == 3 && tokens[i].type == TOKEN_FLOAT )
    {
        if(grammar_2string)
        {
          spawn.position.y = tokens[i].float_value;
        }
        else
        {
          spawn.position.z = tokens[i].float_value;
        }
    }
    else if (chunk_index == 3 && tokens[i].type == TOKEN_STRING )
    {
      printf("%s\n","Token sequence has too many strings" );
      fclose(fp);
      return NULL;
    }
    else if (chunk_index == 4 && tokens[i].type == TOKEN_FLOAT )
    {
        if(grammar_2string)
        {
          spawn.position.z = tokens[i].float_value;
        }
        else
        {
          spawn.time = tokens[i].float_value;
          //terminal = true;
        }
    }
    else if (chunk_index == 4 && tokens[i].type == TOKEN_STRING )
    {
      printf("%s\n","Token sequence has too many strings" );
      fclose(fp);
      return NULL;
    }
    else if (chunk_index == 4 && tokens[i].type == TOKEN_NEWLINE )
    {
      spawn.time = 0.0;
      spawn.duration = 0.0;
      terminal = true;
    }
    else if (chunk_index == 5 && tokens[i].type == TOKEN_FLOAT && grammar_2string )
    {
      spawn.time = tokens[i].float_value;
      //terminal = true;
    }
    else if (chunk_index == 5 && tokens[i].type == TOKEN_FLOAT && !grammar_2string )
    {
      spawn.duration = tokens[i].float_value;
      terminal = true;
    }
    else if (chunk_index == 6 && tokens[i].type == TOKEN_FLOAT && grammar_2string )
    {
      spawn.duration = tokens[i].float_value;
      terminal = true;
    }
    else if (chunk_index == 6 && tokens[i].type == TOKEN_NEWLINE && grammar_2string )
    {
      spawn.duration = 0.0;
      terminal = true;
    }
    else if (chunk_index == 5 && tokens[i].type == TOKEN_NEWLINE && !grammar_2string )
    {
      spawn.duration = 0.0;
      terminal = true;
    }
    else
    {
      printf("%s\n","Token sequence has too many strings" );
      fclose(fp);
      return NULL;
    }


    chunk_index++;
    if (terminal)
    {
      chunk_index = 0;
      grammar_2string = false;
      spawn.index = addToSet(&string_set,&string_set_count,spawn.name);
      spawns = realloc(spawns,sizeof(th_SpawnsetPair)*(*entries + 1));
      spawns[*entries] = spawn;
      *entries = *entries + 1;

      spawn.duration = 0.0;
      chunk_index = 0;
      grammar_2string = false;
      spawn.duration = 0;
    }
  }

  if (string_set != NULL)
  {
    free(string_set);
  }

  if (tokens != NULL)
  {
    free(tokens);
  }




  fclose(fp);

  if (spawns != NULL)
  {
    qsort(spawns,*entries,sizeof(th_SpawnsetPair),compare_names);
  }

  // for (int i = 0 ; i < *entries;i++)
  // {
  //   printf("%s \n",spawns[i].name);
  // }


  th_SpawnsetPair* ret = th_alloc(allocator,sizeof(th_SpawnsetPair)*(*entries));
  if (spawns != NULL)
  {
      memcpy(ret,spawns,sizeof(th_SpawnsetPair)*(*entries));
  }

  free(spawns);

  free(equiv_set);
  equiv_set = NULL;
  equiv_set_count = 0;

  return ret;
}

th_SpawnsetPair* th_spawnSetFind(th_SpawnsetPair* spawnset,int count,const char* name,int* num_spawns)
{
  bool found = false;
  *num_spawns = 0;

  th_SpawnsetPair* ret = NULL;
  for (int i = 0; i < count; i++) {
    if (strcmp(spawnset[i].name,name) == 0)
    {
      if (!found)
      {
        ret = &spawnset[i];
      }
      found = true;
      *num_spawns = *num_spawns + 1;

    }
    else if (found)
    {
      break;
    }

  }


  return ret;
}

//grammar
//string anything anything anything .... newline

static th_KeyValuePair* getBlankKeyVals(th_Allocator* allocator,int* entries)
{
  th_KeyValuePair* ret = th_alloc(allocator,sizeof(th_KeyValuePair)*(1));
  strcpy(ret[0].key,"");
  for (int i = 0; i < 16; i++) {
    ret[0].values[i].flt_value = 0.0;
    strcpy(ret[0].values[i].str_value,"false");
  }
  ret[0].num_values = 16;
  ret[0].token_index = 0;


  *entries = 1;
  return ret;
}

th_KeyValuePair* th_keyValueLoad(th_Allocator* allocator,const char* filename,int* entries)
{
  FILE* fp = th_fopen(filename,"r");
  if (fp == NULL)
  {
    printf("%s\n","Cannot open keyvalues");
    return getBlankKeyVals(allocator,entries);
  }
  int token_count = 0;
  Token* tokens = tokenize(fp,&token_count);

  int chunk_index = 0;
  th_KeyValuePair keyval;
  keyval.num_values = 0;

  *entries = 0;
  th_KeyValuePair* keyvals = NULL;

  StringSetEntry* string_set = NULL;
  int string_set_count = 0;

  for (int i = 0; i < token_count; i++) {

    bool terminal = false;

    if (tokens[i].type == TOKEN_NEWLINE && chunk_index >= 2)
    {
      terminal = true;
    }
    else if (chunk_index == 0 && tokens[i].type == TOKEN_NEWLINE)
    {
      continue;
    }
    else if (chunk_index == 0 && tokens[i].type != TOKEN_STRING)
    {
      printf("%s %i %i\n","Key value sequence doenst start with a string",tokens[i].type,i);
      fclose(fp);
      return getBlankKeyVals(allocator,entries);
    }
    else if (chunk_index == 0 && tokens[i].type == TOKEN_STRING)
    {
      keyval.token_index = i;
      strcpy(keyval.key,tokens[i].value);
    }
    else if (chunk_index >= 1 && keyval.num_values < 16 && (tokens[i].type == TOKEN_STRING || tokens[i].type == TOKEN_FLOAT))
    {
      if (tokens[i].type == TOKEN_STRING)
      {
        keyval.values[keyval.num_values].flt_value = 0.0;
        strcpy(keyval.values[keyval.num_values].str_value,tokens[i].value);
        keyval.num_values++;
      }
      else if (tokens[i].type == TOKEN_FLOAT)
      {
        keyval.values[keyval.num_values].flt_value = tokens[i].float_value;
        strcpy(keyval.values[keyval.num_values].str_value,"");
        keyval.num_values++;
      }

    }
    else
    {
      printf("%s\n","Token sequence irregular" );
      fclose(fp);
      return getBlankKeyVals(allocator,entries);
    }


    chunk_index++;
    if (terminal)
    {
      chunk_index = 0;

      int set_num = addToSet(&string_set,&string_set_count,tokens[keyval.token_index].value);

      if (set_num == 0)
      {
        keyvals = realloc(keyvals,sizeof(th_KeyValuePair)*(*entries + 1));
        keyvals[*entries] = keyval;
        *entries = *entries + 1;
      }

      keyval.num_values = 0;

    }
  }

  if (string_set != NULL)
  {
    free(string_set);
  }

  if (tokens != NULL)
  {
    free(tokens);
  }




  fclose(fp);

  //add in null token
  th_KeyValuePair* ret = th_alloc(allocator,sizeof(th_KeyValuePair)*(*entries + 1));
  strcpy(ret[0].key,"");
  for (int i = 0; i < 16; i++) {
    ret[0].values[i].flt_value = 0.0;
    strcpy(ret[0].values[i].str_value,"false");
  }
  ret[0].num_values = 16;
  ret[0].token_index = 0;

  memcpy(&ret[1],keyvals,sizeof(th_KeyValuePair)*(*entries));
  free(keyvals);
  *entries = *entries + 1;
  return ret;
}


th_KeyValuePair* th_keyValueFind(th_KeyValuePair* keyvalues,int count,const char* name)
{
  bool found = false;

  //default is null token
  th_KeyValuePair* ret = &keyvalues[0];
  for (int i = 0; i < count; i++) {
    if (strcmp(keyvalues[i].key,name) == 0)
    {
      if (!found)
      {
        ret = &keyvalues[i];
      }
      found = true;
      return ret;

    }
    else if (found)
    {
      break;
    }

  }


  return ret;
}

th_KeyValuePair* th_keyValueFindOrNull(th_KeyValuePair* keyvalues,int count,const char* name)
{
  bool found = false;

  //default is null token
  th_KeyValuePair* ret = NULL;
  for (int i = 0; i < count; i++) {
    if (strcmp(keyvalues[i].key,name) == 0)
    {
      if (!found)
      {
        ret = &keyvalues[i];
      }
      found = true;
      return ret;

    }
    else if (found)
    {
      break;
    }

  }


  return ret;
}

fn_vec3 th_keyValueGetVec3(th_KeyValuePair* keyvalues,int count,const char* name)
{
  float x =  th_keyValueFind(keyvalues,count,name)->values[0].flt_value;
  float y =  th_keyValueFind(keyvalues,count,name)->values[1].flt_value;
  float z =  th_keyValueFind(keyvalues,count,name)->values[2].flt_value;
  return fn_createVec3(x,y,z);
}

fn_vec4 th_keyValueGetVec4(th_KeyValuePair* keyvalues,int count,const char* name)
{
  float x =  th_keyValueFind(keyvalues,count,name)->values[0].flt_value;
  float y =  th_keyValueFind(keyvalues,count,name)->values[1].flt_value;
  float z =  th_keyValueFind(keyvalues,count,name)->values[2].flt_value;
  float w =  th_keyValueFind(keyvalues,count,name)->values[3].flt_value;
  return fn_createVec4(x,y,z,w);
}

float th_keyValueGetFloat(th_KeyValuePair* keyvalues,int count,const char* name)
{
  return th_keyValueFind(keyvalues,count,name)->values[0].flt_value;
}

float th_keyValueGetFloatDefault(th_KeyValuePair* keyvalues,int count,const char* name,float def)
{
  th_KeyValuePair* ptr = th_keyValueFindOrNull(keyvalues,count,name);
  if (ptr == NULL)
  {
    return def;
  }

  return ptr->values[0].flt_value;
}

fn_vec3 th_keyValueGetVec3Default(th_KeyValuePair* keyvalues,int count,const char* name,fn_vec3 def)
{
  th_KeyValuePair* ptr = th_keyValueFindOrNull(keyvalues,count,name);
  if (ptr == NULL)
  {
    return def;
  }

  float x =  ptr->values[0].flt_value;
  float y =  ptr->values[1].flt_value;
  float z =  ptr->values[2].flt_value;
  return fn_createVec3(x,y,z);
}

bool th_keyValueGetBool(th_KeyValuePair* keyvalues,int count,const char* name)
{
  return strcmp(th_keyValueFind(keyvalues,count,name)->values[0].str_value,"true") == 0;
}

const char* th_keyValueGetStrDefault(th_KeyValuePair* keyvalues,int count,const char* name,const char* def)
{
  th_KeyValuePair* ptr = th_keyValueFindOrNull(keyvalues,count,name);
  if (ptr == NULL)
  {
    return def;
  }

  return ptr->values[0].str_value;
}




char* level_query(int index) {
  // Format of the final string:
  // "#define TH_Z_SLICES <slices>\n#define TH_Z_MEMORY <memory>\n"

  const char* format = "level_%i";
  int buffer_size = snprintf(NULL, 0, format, index) + 1;

  // Allocate space for the final string, including the prefix length.
  char* result = malloc(buffer_size);
  if (result == NULL) {
    return NULL; // Memory allocation failed
  }

  sprintf(result, format, index);

  return result;
}

th_LevelManifest th_getLevelManifest(const char* filename)
{

  StringSetEntry* string_set = NULL;
  int string_set_count = 0;

  printf("Level Manifest %s\n",filename);
  th_Allocator allocator;
  th_createAllocator(&allocator);

  int n_keyvals;
  th_KeyValuePair* kvals =  th_keyValueLoad(&allocator,filename,&n_keyvals);

  th_LevelManifest manifest;
  manifest.count = n_keyvals - 1;
  manifest.levelfiles = malloc(sizeof(th_LevelFile)*(n_keyvals - 1));

  for (int idx = 1 ; idx < n_keyvals;idx++)
  {
    int i = idx - 1;
    char* lq = level_query(i);
    th_KeyValuePair* p = th_keyValueFind(kvals,n_keyvals,lq);
    if (p->num_values >= 4 )
    {
      manifest.levelfiles[i].name = th_strdup(p->values[0].str_value);
      manifest.levelfiles[i].spawnset_name = th_strdup(p->values[1].str_value);
      manifest.levelfiles[i].display_name = th_strdup(p->values[2].str_value);
      manifest.levelfiles[i].difficulty_name = th_strdup(p->values[3].str_value);

      char* thumb_str = malloc(1024);

      int thumb_err = 0;


      int set_num = addToSet(&string_set,&string_set_count,manifest.levelfiles[i].name);

      if (set_num == 0)
      {
        thumb_err = snprintf(thumb_str,1024,"th1/models/%s/thumbnail.png",manifest.levelfiles[i].name);
      }
      else
      {

        thumb_err = snprintf(thumb_str,1024,"th1/models/%s/thumbnail%i.png",manifest.levelfiles[i].name,set_num + 1);

        //fallback
        if (!th_fileExists(thumb_str))
        {
          thumb_err = snprintf(thumb_str,1024,"th1/models/%s/thumbnail.png",manifest.levelfiles[i].name);
        }
      }

      printf("%s\n",thumb_str);

      if (thumb_err >= 1024 || thumb_err < 0)
      {
        printf("thumbnail path too long\n");
      }

      manifest.levelfiles[i].thumbnail_path = thumb_str;
    }
    else
    {
      printf("Error Parsing Level List\n");
    }

  }


  if (string_set != NULL)
  {
    free(string_set);
  }

  return manifest;
}

th_VictoryManifest th_getVictoryManifest(const char* filename,int minimum_count)
{
  printf("Victory Manifest %s\n",filename);
  th_Allocator allocator;
  th_createAllocator(&allocator);

  int n_keyvals;
  th_KeyValuePair* kvals =  th_keyValueLoad(&allocator,filename,&n_keyvals);



  th_VictoryManifest manifest;
  manifest.count = minimum_count;
  manifest.victorystates = malloc(sizeof(th_LevelVictoryState)*(minimum_count));

  for (int i = 0 ; i < minimum_count ;i++)
  {
    //int i = idx - 1;

    if (n_keyvals - 1 < minimum_count && i >= n_keyvals - 1)
    {
      //not enough victory states defined for each level
      manifest.victorystates[i].level_speed = 0;
      manifest.victorystates[i].level_airtime = 0;
      manifest.victorystates[i].level_damagetaken = 0;
      manifest.victorystates[i].flawless = 0;
    }
    else
    {
      char* lq = level_query(i);
      th_KeyValuePair* p = th_keyValueFind(kvals,n_keyvals,lq);
      if (p->num_values >= 4 )
      {
        manifest.victorystates[i].level_speed = p->values[0].flt_value;
        manifest.victorystates[i].level_airtime = p->values[1].flt_value;
        manifest.victorystates[i].level_damagetaken = p->values[2].flt_value;
        manifest.victorystates[i].flawless = p->values[3].flt_value;
      }
      else
      {
        printf("Error Parsing Victory List\n");
      }
    }



  }

  return manifest;
}
