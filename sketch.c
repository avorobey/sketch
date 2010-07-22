#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>

#include "common.h"

void die(char *msg) {
  fprintf(stderr, "dying: %s\n", msg);
  exit(1);
}

uint64_t cells[MAX_CELLS];

/* start after all the special values */
uint32_t next_cell = C_STARTFROM;

void init_cells(void) {
  cells[C_UNSPEC] = cells[C_EMPTY] = cells[C_FALSE] = cells[C_TRUE] = T_RESV;
}

#define SKIP_WS(str) do { while(isspace(*str)) ++str; } while(0)

/* helper rules for identifying symbols */
#define INITIAL(c) ((c>='a' && c<='z') || c=='!' || c=='$' || c=='%' || \
                   c=='&' || c=='*' || c=='/' || c==':' || c=='<' || \
                   c=='=' || c=='>' || c=='?' || c=='^' || c=='_' || c=='~')

#define SUBSEQUENT(c) (INITIAL(c) || (c>='0' && c<='9') || c=='+' || \
                       c=='-' || c=='.' || c=='@')


uint32_t make_pair(uint32_t first, uint32_t second) {
  CHECK_CELLS(2);
  uint32_t index = next_cell;
  cells[next_cell++] = T_PAIR;
  cells[next_cell++] = ((uint64_t)first << 32) | second;
  return index;
}

uint32_t make_list(uint32_t *values, uint32_t count) {
  /* Adds its own () at the end, no need to pass it. */
  if (count < 1) die("bad call to make_list");
  uint32_t current = count-1;

  uint32_t pair = C_EMPTY; /* () at first; then pairs in the loop */
  while(1) {
    pair = make_pair(values[current], pair);
    if (current == 0) break;
    current--;
  }
  return pair;
}

uint32_t make_vector(uint32_t size, int zero_it) {
  uint32_t len = (size+1)/2;  /* num of extra cells required */
  CHECK_CELLS(len+1);
  uint32_t index = next_cell;
  uint64_t value = T_VECT | (uint64_t)len << 16 | (uint64_t)(size) << 32;
  cells[next_cell++] = value;
  if (zero_it) memset(VECTOR_START(index), 0, size*sizeof(uint32_t));
  next_cell += len;
  return index;
}
 
uint32_t make_env(uint32_t size, uint32_t prev) {
  uint32_t env = make_vector(size+1, 1);
  *VECTOR_START(env) = prev;
  return env;
}

/* checks that index is a proper ()-terminated list with 
   at least count elements. if strict is true, must be exactly
   count elements. count==0, strict==0 allows any list.
   () itself is a list. */
int check_list(uint32_t index, int count, int strict) {
  while(index != C_EMPTY) {
    if (TYPE(index) != T_PAIR) return 0;
    if (strict && count <= 0) return 0;
    index = CDR(index);
    if (count > 0) --count;
  }
  if (count > 0) return 0;
  else return 1;
}

/* returns the length of the list.
   NOTE, IMPORTANT: returns -1 if not a proper list.
   If this function returns a value >=0, the list has been vetted
   and can be walked w/o further checks. */
int length_list(uint32_t index) {
  int count = 0;
  while(index != C_EMPTY) {
    if (TYPE(index) != T_PAIR) return -1;
    index = CDR(index);
    count++;
  }
  return count;
}


/* helper functions to store stuff into cells */

uint32_t store_string(char *str, char *end, int type) {
  uint32_t len = (end-str+7)/8;
  CHECK_CELLS(len+1);
  uint32_t index = next_cell;
  uint64_t value = type | (uint64_t)len << 16 | (uint64_t)(end-str) << 32;
  cells[next_cell++] = value;
  strncpy(STR_START(index), str, end-str);
  next_cell+=len;
  return index;
}

uint32_t store_pair(uint32_t first, uint32_t second) {
  uint64_t  value = T_PAIR;
  CHECK_CELLS(2);
  uint32_t  index = next_cell;
  cells[next_cell++] = value;
  cells[next_cell++] = ((uint64_t)first << 32) | second;
  return index;
}

uint32_t store_int32(int32_t num) {
  uint64_t value = T_INT32;
  CHECK_CELLS(1);
  uint32_t index = next_cell;
  cells[next_cell++] = value | ((uint64_t)(uint32_t)num) << 32;
  return index;
}

/* helper func to read a #(...) literal vector */
int read_vector(char **pstr, uint32_t *pindex) {
  uint32_t initial_indices[2];
  int max_index = 2, cur_index = 0;
  uint32_t *indices = initial_indices;
  int malloced = 0;
  char *str = *pstr;
  while(1) {
    SKIP_WS(str);
    if (*str == ')') break;
    int res = read_value(&str, &indices[cur_index], 0);
    if (res == 0) { 
      if (malloced) free(indices);
      return 0;
    }
    cur_index++;
    if (cur_index >= max_index) {  /* need to grow */
      max_index *= 2;
      indices = malloced ? realloc(indices, max_index*sizeof(uint32_t))
                         : malloc(max_index*sizeof(uint32_t));
      if (indices == 0) die("couldn't alloc memory for a vector");
      if (!malloced) {
        memcpy(indices, initial_indices, 2*sizeof(uint32_t));
        malloced = 1;
      }
    }
  }
  /* we've seen ')' and all is good. store and cleanup */
  str++; /* one past the ')' */

  uint32_t index = make_vector(cur_index, 0);
  memcpy(VECTOR_START(index), indices, cur_index*sizeof(uint32_t));
  if(malloced) free(indices);
  *pindex = index;
  *pstr = str;
  return 1;
}

int read_value(char **pstr, uint32_t *pindex, int implicit_paren) {
  char *str = *pstr;
  int num, count;
  uint64_t value;
  uint32_t index = next_cell;

  SKIP_WS(str);
  if (implicit_paren || *str == '(') {
    if (!implicit_paren) ++str;
    SKIP_WS(str);
    if (*str == ')') {
      str++;
      *pindex = C_EMPTY;
      *pstr = str;
      return 1;
    }
    uint32_t index1;
    int res = read_value(&str, &index1, 0);
    if (!res) return 0;

    SKIP_WS(str);
    int dot_pair = 0;
    if (*str == '.' && isspace(*(str+1))) {
      dot_pair = 1;
      str++;
    }
    uint32_t index2;
    res = read_value(&str, &index2, !dot_pair);
    if (!res) return 0;

    if (dot_pair) { /* consume the final )  */
      SKIP_WS(str);
      if (*str != ')') return 0;
      str++;
    }

    /* create and store the pair */
    *pindex = store_pair(index1, index2); 
    *pstr = str;
    return 1;
  }

  if (*str == '#' && *(str+1) == '(') {  /* literal vector */
    str+=2;
    int res = read_vector(&str, &index);
    if (res == 0) return 0;
    *pindex = index; *pstr = str;
    return 1;
  }

  if (*str == '#' && *(str+1) == '\\') {  /* character */
    str+=2;
    if (*str == '\0') return 0;
    unsigned char c;
    if (strncmp(str, "space", 5) == 0) {
      str+=5; c = ' ';
    } else if (strncmp(str, "newline", 7) == 0) {
      str+=7; c = '\n';
    } else {
      c = *str; str+=1;
    }
    index = next_cell;
    uint64_t value = T_CHAR | (uint64_t)c << 32;
    cells[next_cell++] = value;
    *pindex = index;
    *pstr = str;
    return 1;
  }

  if (*str == '#' && *(str+1) == 'f') {
    *pstr = str+2; *pindex = C_FALSE; return 1;
  }

  if (*str == '#' && *(str+1) == 't') {
    *pstr = str+2; *pindex = C_TRUE; return 1;
  }
    
  if (sscanf(str, "%d%n", &num, &count) >= 1) {
    str += count;
    value = T_INT32 | ((uint64_t)num << 32);
    CHECK_CELLS(1);
    cells[next_cell++] = value;
    *pindex = index;
    *pstr = str;
    return 1;
  }

  if (*str == '"') {
    char *end = ++str;
    while(*end && *end != '"') ++end;
    if (*end == '\0') return 0;
    *pindex = store_string(str, end, T_STR);
    *pstr = end+1;
    return 1;
  }

  if (*str == '\'') {
    str++;
    uint32_t indices[2];
    int res = read_value(&str, &indices[1], 0);
    if (!res) return 0;
    char *quote = "quote";
    indices[0] = store_string(quote, quote+5, T_SYM);
    *pindex = make_list(indices, 2);
    *pstr = str;
    return 1;
  }

  /* is it a symbol? */
  int symbol = 0;
  char *end; /* points one past the end of the symbol */

  /* special identifiers */
  if (*str == '+' || *str == '-') {
    end = str+1; symbol = 1;
  }
  if (!symbol && strncmp(str, "...", 3) == 0) {
    end = str+3; symbol = 1;
  }

  /* regular identifiers */
  if (!symbol && INITIAL(*str)) {
    symbol = 1;
    end = str+1;
    while(SUBSEQUENT(*end)) end++;
  }

  if (symbol) {
    *pindex = store_string(str, end, T_SYM);
    *pstr = end;
    return 1;
  }

  return 0;
}

void dump_value(uint32_t index, int implicit_paren) {
  uint32_t length, i;
  uint32_t index1, index2;
  int32_t num;
  char *p;
  switch(TYPE(index)) {
    case T_RESV:
      if (index == C_EMPTY) printf("()");
      else if (index == C_FALSE) printf("#f");
      else if (index == C_TRUE) printf("#t");
      else if (index == C_UNSPEC) ;  /* print nothing */
      else die("unknown RESV value");
      break;
    case T_INT32:
      num = INT32_VALUE(index);
      printf("%d", num);
      break;
    case T_STR:
      length = STR_LEN(index);
      p = STR_START(index);
      putchar('"');
      for (i = 0; i < length; i++) putchar(*p++);
      putchar('"');
      break;
    case T_SYM:
      length = STR_LEN(index);
      p = STR_START(index);
      for (i = 0; i < length; i++) putchar(*p++);
      break;
    case T_PAIR:
      index1 = cells[index+1] >> 32;
      index2 = cells[index+1] & 0xFFFFFFFF;
      if (!implicit_paren) putchar('(');
      dump_value(index1, 0);
      if (TYPE(index2) == T_PAIR) {
        putchar(' ');
        dump_value(index2, 1);
      } else {
        if (index2 != C_EMPTY) {
          printf(" . ");
          dump_value(index2, 0);
        }
        putchar(')');
      }
      break;
    case T_FUNC:
      printf("*func*");
      break;
    case T_VECT:
      length = VECTOR_LEN(index);
      uint32_t *start = VECTOR_START(index);
      printf("#(");
      for (i = 0; i < length; i++) {
        dump_value(start[i], 0);
        if (i != length-1) putchar(' ');
      }
      putchar(')');
      break;
    case T_CHAR:
      printf("#\\");
      unsigned char c = CHAR_VALUE(index);
      if (c == ' ') printf("space");
      else if (c == '\n') printf("newline");
      else putchar(c);
      break;
    default:
      break;
  }
}

uint32_t eval(uint32_t index);

/* returns true/false on success/failure */
int eval_args(uint32_t list, uint32_t *args, uint32_t *num_args) {
  uint32_t count = 0;
  while(count < MAX_ARGS && list != C_EMPTY) {
    args[count] = eval(CAR(list));
    if (args[count] == 0) return 0;
    count++;
    list = CDR(list);
    if (!LIST_LIKE(list))
      die("badly formed argument list");
  }
  if (count >= MAX_ARGS) die("more than MAX_ARGS arguments");
  *num_args = count;
  return 1;
}

/* we count on the compiler to precompute constant strlens */
#define IS_SYMBOL(index, name) (STR_LEN(index) == strlen(name) && \
                                strcmp(STR_START(index), name) == 0)
uint32_t eval(uint32_t index) {
  uint32_t sym, val, func, args;
  uint32_t arg_array[MAX_ARGS];
  switch(TYPE(index)) {
    case T_INT32:
    case T_RESV:
    case T_STR:
    case T_CHAR:
      return index;
    case T_SYM:
      val = get_symbol(STR_START(index), STR_LEN(index));
      if (val == 0) die("undefined symbol");
      return val;
    case T_PAIR:
      func = CAR(index);
      args = CDR(index);
      if (!LIST_LIKE(args)) die("args to eval aren't a list");

      /* special-case special forms here. Don't try to eval 'func'
         until we have special forms as proper symbols. */
      if (TYPE(func) == T_SYM) {
        int is_define = IS_SYMBOL(func, "define");
        int is_set = IS_SYMBOL(func, "set!");
        if (is_define || is_set) {
          /* the only allowed syntax here is ([define/set!] symbol value) */
          if (!check_list(args, 2, 1)) die("bad define/set! syntax");
          sym = CAR(args);
          val = CAR(CDR(args));
          val = eval(val);
          if (val == 0) die("couldn't eval the value in define/set!");
          if (TYPE(sym) != T_SYM) die ("define/set! followed by non-symbol");
          if (is_set && get_symbol(STR_START(sym), STR_LEN(sym))==0)
            die("set! on an undefined symbol");
          set_symbol(STR_START(sym), STR_LEN(sym), val);
          return val; /* TODO: actually undefined value */
        }

        if (IS_SYMBOL(func, "quote")) {
          /* syntax is: (quote value) */
          if (!check_list(args, 1, 1)) die ("bad quote syntax");
          return CAR(args);
        }

        if (IS_SYMBOL(func, "if")) {
          int len = length_list(args);
          if (!(len == 2 || len == 3)) die("bad if syntax");
          val = eval(CAR(args)); /* condition */
          if (val != C_FALSE) {  /* only #if is false */
            return eval(CAR(CDR(args)));
          } else {
            if (len == 3) return eval(CAR(CDR(CDR(args))));
            else return C_UNSPEC;
          }
        }

        if (IS_SYMBOL(func, "lambda")) {
          int len = length_list(args);
          if (len == -1 || len < 2) die("bad lambda syntax");
          uint32_t args_list = CAR(args);
          if (!check_list(args_list, 0, 0)) die ("bad lambda argument list");
          /* TODO: check that args_list actually contains only symbols and
             they don't repeat */
          uint64_t value = T_FUNC; 
          CHECK_CELLS(1);
          uint32_t index = next_cell;
          cells[next_cell++] = value | (uint64_t)args << 32;
          return index;
        }
      }

      /* special forms end here. Now eval the first element and check
         that it's a function. */
      val = eval(func);
      if (val == 0) die("undefined symbol as func name");
      if (TYPE(val) != T_FUNC) die("first element in list not a function");

      /* evaluate arguments and call the function */
      uint32_t num_args;
      if (!eval_args(args, arg_array, &num_args)) return 0;
      if (cells[val] & BLTIN_MASK) {   /* builtin function */
         /* TODO: do we really need a list for builtin funcs? Reevaluate the
            interface to them after lexical scoping & tail calls are done. */
         uint32_t list = make_list(arg_array, num_args);
         builtin_t func = (builtin_t)cells[val+1];
        /* well, there you go */
        return func(list);
      } else {  /* lambda function */
        uint32_t params_and_body = (uint32_t)(cells[val] >> 32);
        uint32_t params = CAR(params_and_body);
        if (length_list(params) != num_args) return 0;  /* TODO: informative */
        for (uint32_t i = 0; i < num_args; i++) {
          uint32_t param = CAR(params);
          params = CDR(params);
          if (TYPE(param) != T_SYM) return 0;
          set_symbol(STR_START(param), STR_LEN(param), arg_array[i]);
        }
        uint32_t body = CDR(params_and_body);
        uint32_t retval = C_UNSPEC;
        while (body != C_EMPTY) {
          retval = eval(CAR(body));
          if (retval == 0) return 0;
          body = CDR(body);
        }
        return retval;
      }
    default:
      return 0;
  }
}

int main(int argc, char **argv) {
  char buf[512];
  init_cells();
  register_builtins();
  while(1) {
    printf("%d cells> ", next_cell);
    if (fgets(buf, 512, stdin) == 0) { 
      if (feof(stdin)) return 0;
      else die("fgets() failed");
    }
    char *str = buf;
    uint32_t index;
    int can_read = read_value(&str, &index, 0);
    if (can_read) {
      uint32_t res = eval(index);
      if (res) {
        dump_value(res, 0); printf("\n");
      } else printf("eval failed.\n");
    }
    else printf("failed reading at: %s\n", str);
  }
  return 0;
}

