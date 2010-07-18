#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>

#include "common.h"

void die(char *str) {
  fprintf(stderr, "dying: %s\n", str);
  exit(1);
}

uint64_t cells[MAX_CELLS];

/* start after all the special values */
uint32_t next_cell = C_STARTFROM;

void init_cells(void) {
  cells[C_EMPTY] = cells[C_FALSE] = cells[C_TRUE] = T_RESV;
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

/* helper func to store a string into cells */
uint32_t pack_string(char *str, char *end, int type) {
  uint32_t len = (end-str+7)/8;
  CHECK_CELLS(len+1);
  uint32_t index = next_cell;
  uint64_t value = type | (uint64_t)len << 16 | (uint64_t)(end-str) << 32;
  cells[next_cell++] = value;
  strncpy(SYMBOL_NAME(index), str, end-str);
  next_cell+=len;
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

  uint32_t len = (cur_index+1)/2;  /* num of extra cells required */
  CHECK_CELLS(1+len);
  uint32_t index = next_cell;
  uint64_t value = T_VECT | (uint64_t)len << 16 | (uint64_t)(cur_index) << 32;
  cells[next_cell++] = value;
  memcpy(VECTOR_START(index), indices, cur_index*sizeof(uint32_t));
  if(malloced) free(indices);
  next_cell+=len;
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
    value = T_PAIR;
    CHECK_CELLS(2);
    index = next_cell;
    cells[next_cell++] = value;
    cells[next_cell++] = ((uint64_t)index1 << 32) | index2;
    *pindex = index; *pstr = str;
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
    *pindex = pack_string(str, end, T_STR);
    *pstr = end+1;
    return 1;
  }

  if (*str == '\'') {
    str++;
    uint32_t indices[2];
    int res = read_value(&str, &indices[1], 0);
    if (!res) return 0;
    char *quote = "quote";
    indices[0] = pack_string(quote, quote+5, T_SYM);
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
    *pindex = pack_string(str, end, T_SYM);
    *pstr = end;
    return 1;
  }

  return 0;
}

void dump_value(uint32_t index, int implicit_paren) {
  uint32_t num, length, i;
  uint32_t index1, index2;
  char *p;
  switch(TYPE(index)) {
    case T_RESV:
      if (index == C_EMPTY) printf("()");
      else if (index == C_FALSE) printf("#f");
      else if (index == C_TRUE) printf("#t");
      else die("unknown RESV value");
      break;
    case T_INT32:
      num = cells[index] >> 32;
      printf("%d", num);
      break;
    case T_STR:
      length = SYMBOL_LEN(index);
      p = SYMBOL_NAME(index);
      putchar('"');
      for (i = 0; i < length; i++) putchar(*p++);
      putchar('"');
      break;
    case T_SYM:
      length = SYMBOL_LEN(index);
      p = SYMBOL_NAME(index);
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
      unsigned char c = (unsigned char)(cells[index] >> 32);
      if (c == ' ') printf("space");
      else if (c == '\n') printf("newline");
      else putchar(c);
      break;
    default:
      break;
  }
}

typedef uint32_t (*builtin_t)(uint32_t);

void register_builtin(char *name, builtin_t func) {
  CHECK_CELLS(2);
  uint64_t value = T_FUNC | BLTIN_MASK;
  uint32_t index = next_cell;
  cells[next_cell++] = value;
  cells[next_cell++] = (uint64_t)(uintptr_t)func;
  set_symbol(name, strlen(name), index);
}

/* for (car '(1 2 3)), we get here ((1 2 3)), not (1 2 3). Same for all
   builtins. */
/* Builtins can assume that they get a well-formed list. */

uint32_t car(uint32_t index) {
  /* check that we have just one argument (the list is of size 1) */
  if (TYPE(index) != T_PAIR || CDR(index) != C_EMPTY) return 0;

  /* pass to this argument - first CAR - and return its first element */
  return CAR(CAR(index));
}
uint32_t cdr(uint32_t index) {
  if (TYPE(index) != T_PAIR || CDR(index) != C_EMPTY) return 0;
  return CDR(CAR(index));
}

uint32_t list(uint32_t args) {
  /* easiest builtin ever. */
  return args;
}

uint32_t plus(uint32_t index) {
  int32_t accum = 0;
  uint32_t val;
  while(index != C_EMPTY) {
    val = CAR(index);
    if (TYPE(val) != T_INT32) return 0;
    int32_t signed_val = (int32_t)(cells[val] >> 32);
    accum += signed_val;
    index = CDR(index);
  }
  CHECK_CELLS(1);
  uint32_t unsigned_val = (uint32_t)accum;
  uint32_t res = next_cell;
  cells[next_cell++] = T_INT32 | (uint64_t)unsigned_val << 32;
  return res;
}

void register_builtins(void) {
  register_builtin("car", car);
  register_builtin("cdr", cdr);
  register_builtin("+", plus);
  register_builtin("list", list);
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
#define IS_SYMBOL(index, name) (SYMBOL_LEN(index) == strlen(name) && \
                                strcmp(SYMBOL_NAME(index), name) == 0)
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
      val = get_symbol(SYMBOL_NAME(index), SYMBOL_LEN(index));
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
          if (is_set && get_symbol(SYMBOL_NAME(sym), SYMBOL_LEN(sym))==0)
            die("set! on an undefined symbol");
          set_symbol(SYMBOL_NAME(sym), SYMBOL_LEN(sym), val);
          return val; /* TODO: actually undefined value */
        }

        if (IS_SYMBOL(func, "quote")) {
          /* syntax is: (quote value) */
          if (!check_list(args, 1, 1)) die ("bad quote syntax");
          return CAR(args);
        }
      }

      /* special forms end here. Now eval the first element and check
         that it's a function. */
      val = eval(func);
      if (val == 0) die("undefined symbol as func name");
      if (TYPE(val) != T_FUNC) die("first element in list not a function");
      if ((cells[val] & BLTIN_MASK) == 0) die("can only call builtins for now");

      uint32_t num_args;
      if (!eval_args(args, arg_array, &num_args)) return 0;
      uint32_t list = make_list(arg_array, num_args);
      builtin_t func = (builtin_t)cells[val+1];

      /* well, there you go */
      return func(list);
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

