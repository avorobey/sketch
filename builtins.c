#include <stdint.h>
#include <string.h>

#include "common.h"

/* Builtin functions. The code in this file reuses
   lots of internal structure in sketch.c deliberately exposed
   via common.h. */

void register_builtin(char *name, builtin_t func) {
  CHECK_CELLS(2);
  uint64_t value = T_FUNC | BLTIN_MASK;
  uint32_t index = next_cell;
  cells[next_cell++] = value;
  cells[next_cell++] = (uint64_t)(uintptr_t)func;
  set_symbol(name, strlen(name), index);
}

/* Every builtin gets an index to a list of all its arguments. If it
   receives just one, it's still a list of one, e.g. ((1 2)) for (car '(1 2)).
   Return value is the index of the result, or 0 if an error occurs.
   Because the list of arguments is built up as they're evaluated, builtins
   can assume it's a well-formed list. */

/* some handy defines for functions expecting one or two arguments and
   wishing to fail if the number of arguments doesn't match. */

#define ONE_ARG(name) uint32_t name; if (!check_list(args, 1, 1)) \
  return 0; name = CAR(args);

#define TWO_ARGS(name1, name2) uint32_t name1, name2; \
  if (!check_list(args, 2, 1)) return 0; name1 = CAR(args); \
  name2 = CAR(CAR(args));

/* Types. */

#define GEN_TYPE_PREDICATE(func_name, type) \
  uint32_t func_name(uint32_t args) { \
    ONE_ARG(name); \
    if (TYPE(name) == type) return C_TRUE; \
    else return C_FALSE; \
  }

GEN_TYPE_PREDICATE(procedure_p, T_FUNC)
GEN_TYPE_PREDICATE(vector_p, T_VECT)
GEN_TYPE_PREDICATE(string_p, T_STR)
GEN_TYPE_PREDICATE(symbol_p, T_SYM)
GEN_TYPE_PREDICATE(char_p, T_CHAR)
GEN_TYPE_PREDICATE(pair_p, T_PAIR)

/* TODO: richer number types will change this */
GEN_TYPE_PREDICATE(number_p, T_INT32)

uint32_t boolean_p(uint32_t args) {
  ONE_ARG(index);
  if (index == C_TRUE || index == C_FALSE) return C_TRUE;
  else return C_FALSE;
}

uint32_t null_p(uint32_t args) {
  ONE_ARG(index);
  if (index == C_EMPTY) return C_TRUE;
  else return C_FALSE;
}

uint32_t list_p(uint32_t args) {
  ONE_ARG(list);
  if (check_list(list, 0, 0)) return C_TRUE;
  else return C_FALSE;
}


/* Pairs and lists. */

uint32_t car(uint32_t args) {
  ONE_ARG(arg);
  return CAR(arg);
}
uint32_t cdr(uint32_t args) {
  ONE_ARG(arg);
  return CDR(arg);
}
uint32_t list(uint32_t args) {
  /* easiest builtin ever. */
  return args;
}
uint32_t cons(uint32_t args) {
  TWO_ARGS(arg1, arg2);
  return store_pair(arg1, arg2);
}

/* Booleans. */

uint32_t not(uint32_t args) {
  ONE_ARG(arg);
  if (arg == C_FALSE) return C_TRUE;
  else return C_FALSE;
}

/* Numbers. */

/* does either + or *, since the code's so similar */
uint32_t plus_times(uint32_t args, int is_plus) {
  int32_t accum = is_plus ? 0 : 1;
  uint32_t val;
  while(args != C_EMPTY) {
    val = CAR(args);
    if (TYPE(val) != T_INT32) return 0;
    int32_t signed_val = (int32_t)(cells[val] >> 32);
    if (is_plus) accum += signed_val;
    else accum *= signed_val;
    args = CDR(args);
  }
  CHECK_CELLS(1);
  uint32_t unsigned_val = (uint32_t)accum;
  uint32_t res = next_cell;
  cells[next_cell++] = T_INT32 | (uint64_t)unsigned_val << 32;
  return res;
}

uint32_t plus(uint32_t args) {
  return plus_times(args, 1);
}

uint32_t times(uint32_t args) {
  return plus_times(args, 0);
}

void register_builtins(void) {
  register_builtin("car", car);
  register_builtin("cdr", cdr);
  register_builtin("+", plus);
  register_builtin("*", times);
  register_builtin("list", list);
  register_builtin("procedure?", procedure_p);
  register_builtin("vector?", vector_p);
  register_builtin("string?", string_p);
  register_builtin("symbol?", symbol_p);
  register_builtin("char?", char_p);
  register_builtin("pair?", pair_p);
  register_builtin("number?", number_p);
  register_builtin("boolean?", boolean_p);
  register_builtin("null?", null_p);
  register_builtin("list?", list_p);
}

