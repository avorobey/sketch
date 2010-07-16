#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>

void die(char *str) {
  fprintf(stderr, "dying: %s\n", str);
  exit(1);
}

/* max arguments in a function call */
#define MAX_ARGS 256

#define MAX_CELLS 1000000
uint64_t cells[MAX_CELLS];
/* start from 1, as index 0 is reserved for errors or invalid values */
uint32_t next_cell = 1;

#define CHECK_CELLS(i) do { if (next_cell + i >= MAX_CELLS) \
  die("out of cells"); } while(0)

// 4 lowest-order bits for the type
#define TYPE_MASK 15
#define TYPE(i) (cells[i] & TYPE_MASK)
#define T_NONE   0  /* this cell is unused */
#define T_INT32  1  /* immediate 32-bit value */
#define T_PAIR   2  /* pair, uses next cell */
#define T_STR    3  /* string */
#define T_SYM    4  /* symbol */
#define T_EMPTY  5  /* empty list, a special value */
#define T_BOOL   6  /* boolean */
#define T_FUNC   7  /* function, a.k.a. closure */

#define BLTIN_MASK 16

#define SKIP_WS(str) do { while(isspace(*str)) ++str; } while(0)

/* helper rules for identifying symbols */
#define INITIAL(c) ((c>='a' && c<='z') || c=='!' || c=='$' || c=='%' || \
                   c=='&' || c=='*' || c=='/' || c==':' || c=='<' || \
                   c=='=' || c=='>' || c=='?' || c=='^' || c=='_' || c=='~')

#define SUBSEQUENT(c) (INITIAL(c) || (c>='0' && c<='9') || c=='+' || \
                       c=='-' || c=='.' || c=='@')

#define CAR(i) (cells[i+1] >> 32)
#define CDR(i) (cells[i+1] & 0xFFFFFFFF)

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

  /* TODO: should have a handy universal empty list value */
  CHECK_CELLS(1);
  uint32_t empty = next_cell;
  cells[next_cell++] = T_EMPTY;

  uint32_t pair = empty; /* () at first; then pairs in the loop */
  while(1) {
    pair = make_pair(values[current], pair);
    if (current == 0) break;
    current--;
  }
  return pair;
}

#define SYMBOL_NAME(i) (char *)(cells+i+1)
#define SYMBOL_LEN(i) (cells[i] >> 32)

/* helper func to store a string into cells */
uint32_t pack_string(char *str, char *end, int type) {
  uint32_t len = (end-str+7)/8;
  CHECK_CELLS(len);
  uint32_t index = next_cell;
  uint64_t value = type | (uint64_t)len << 16 | (uint64_t)(end-str) << 32;
  cells[next_cell++] = value;
  strncpy(SYMBOL_NAME(index), str, end-str);
  next_cell+=len;
  return index;
}

int read_value(char **pstr, uint32_t *pindex, int implicit_paren) {
  char *str = *pstr, *p;
  int num, count;
  uint64_t value;
  uint32_t index = next_cell;

  SKIP_WS(str);
  if (implicit_paren || *str == '(') {
    if (!implicit_paren) ++str;
    SKIP_WS(str);
    if (*str == ')') {
      value = T_EMPTY;
      CHECK_CELLS(1);
      cells[next_cell++] = value;
      *pindex = index;
      *pstr = str;
      return 1;
    }
    uint32_t index1;
    int res = read_value(&str, &index1, 0);
    if (!res) return 0;

    SKIP_WS(str);
    if (*str == ')' && !implicit_paren) { /* just one value in the list */
      *pstr = str+1;
      *pindex = index1;
      return 1;
    }

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
    case T_EMPTY:
      printf("()"); break;
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
        if (TYPE(index2) != T_EMPTY) {
          printf(" . ");
          dump_value(index2, 0);
        }
        putchar(')');
      }
      break;
    case T_FUNC:
      printf("*func*");
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
  cells[next_cell++] = (uint64_t)func;
  set_symbol(name, strlen(name), index);
}

/* for (car '(1 2 3)), we get here ((1 2 3)), not (1 2 3). Same for all
   builtins. */
/* Builtins can assume that they get a well-formed list. */

uint32_t car(uint32_t index) {
  /* check that we have just one argument (the list is of size 1) */
  if (TYPE(index) != T_PAIR || TYPE(CDR(index)) != T_EMPTY) return 0;

  /* pass to this argument - first CAR - and return its first element */
  return CAR(CAR(index));
}
uint32_t cdr(uint32_t index) {
  if (TYPE(index) != T_PAIR || TYPE(CDR(index)) != T_EMPTY) return 0;
  return CDR(CAR(index));
}

uint32_t plus(uint32_t index) {
  int32_t accum = 0;
  uint32_t val;
  while(TYPE(index) != T_EMPTY) {
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
}

uint32_t eval(uint32_t index);

/* returns true/false on success/failure */
int eval_args(uint32_t list, uint32_t *args, int *num_args) {
  int count = 0;
  while(count < MAX_ARGS && TYPE(list) != T_EMPTY) {
    args[count] = eval(CAR(list));
    if (args[count] == 0) return 0;
    count++;
    list = CDR(list);
    if (TYPE(list) != T_PAIR && TYPE(list) != T_EMPTY)
      die("badly formed argument list");
  }
  if (count >= MAX_ARGS) die("more than MAX_ARGS arguments");
  *num_args = count;
  return 1;
}

uint32_t eval(uint32_t index) {
  uint32_t sym, val, car, cdr;
  uint32_t args[MAX_ARGS];
  switch(TYPE(index)) {
    case T_EMPTY:
    case T_INT32:
    case T_BOOL:
    case T_STR:
      return index;
    case T_SYM:
      val = get_symbol(SYMBOL_NAME(index), SYMBOL_LEN(index));
      if (val == 0) die("undefined symbol");
      return val;
    case T_PAIR:
      car = CAR(index);
      cdr = CDR(index);
      if (TYPE(car) != T_SYM) die("car of eval'd pair is not a symbol");
      if (TYPE(cdr) != T_PAIR) die("cdr of eval'd pair is not a pair");

      int is_define = strncmp(SYMBOL_NAME(car), "define", 6) == 0;
      int is_set = strncmp(SYMBOL_NAME(car), "set!", 4) == 0;
      if (is_define || is_set) {
        /* the only allowed syntax here is ([define/set!] symbol value) */
        sym = CAR(cdr);
        if (TYPE(sym) != T_SYM) die ("define/set! followed by non-symbol");
        if (TYPE(CDR(cdr)) != T_PAIR) die ("define/set! symbol . smth");
        val = CAR(CDR(cdr));
        if (TYPE(CDR(CDR(cdr))) != T_EMPTY) die ("define/set! too many args");
        if (is_set && get_symbol(SYMBOL_NAME(sym), SYMBOL_LEN(sym))==0)
          die("set! on an undefined symbol");
        set_symbol(SYMBOL_NAME(sym), SYMBOL_LEN(sym), val);
        return val; /* TODO: actually undefined value */
      }

      if (strncmp(SYMBOL_NAME(car), "quote", 5) == 0) {
        /* syntax is: (quote value) */
        val = CAR(cdr);
        if (TYPE(CDR(cdr)) != T_EMPTY) die ("bad quote syntax");
        return val;
      }

      /* special forms end here. Now check if it's a function. */
      val = get_symbol(SYMBOL_NAME(car), SYMBOL_LEN(car));
      if (val == 0) die("undefined symbol as func name");
      if (TYPE(val) != T_FUNC) die("first element in list not a function");
      if (cells[val] & BLTIN_MASK == 0) die("can only call builtins for now");

      uint32_t num_args;
      if (!eval_args(cdr, args, &num_args)) return 0;
      uint32_t list = make_list(args, num_args);
      builtin_t func = (builtin_t)cells[val+1];

      /* well, there you go */
      return func(list);
    default:
      return 0;
  }
}

int main(int argc, char **argv) {
  char buf[512];
  register_builtins();
  while(1) {
    printf("%d cells> ", next_cell);
    if (fgets(buf, 512, stdin) == 0) die("fgets() failed");
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

