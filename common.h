/* max arguments in a function call */
#define MAX_ARGS 256

/* Working memory in sketch is an array of 64-bit cells. Each value
   spans 1 or more cells, and the first cell for the value devotes
   some bits to its type and other important information. */

#define MAX_CELLS 1000000
extern uint64_t cells[];
extern uint32_t next_cell;

/* special index values, pre-filled and always occupied */
#define C_ERROR 0 
#define C_UNSPEC 1
#define C_EMPTY 2
#define C_FALSE 3
#define C_TRUE 4

/* regular values created during normal work start from here */
#define C_STARTFROM 5

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
#define T_RESV   5  /* special value: bool or () */
#define T_FUNC   6  /* function, a.k.a. closure */
#define T_VECT   7  /* vector */
#define T_CHAR   8  /* character */

/* true for builtin, as opposed to lambda-defined, functions */
#define BLTIN_MASK 16

/* the next few defines depend on how the specific types are laid out */
#define CAR(i) (cells[i+1] >> 32)
#define CDR(i) (cells[i+1] & 0xFFFFFFFF)

#define STR_START(i) (char *)(cells+i+1)
#define STR_LEN(i) (cells[i] >> 32)

#define VECTOR_START(i) (uint32_t *)(cells+i+1)
#define VECTOR_LEN(i) (cells[i] >> 32)

#define CHAR_VALUE(i) (unsigned char)(cells[i] >> 32)
#define INT32_VALUE(i) (int32_t)(cells[i] >> 32)

#define LIST_LIKE(i) (TYPE(i) == T_PAIR || i == C_EMPTY)

typedef uint32_t (*builtin_t)(uint32_t);

/* functions in symbols.cc */
uint32_t get_symbol(const char *name, int len);
void set_symbol(const char *name, int len, uint32_t val);

/* functions in builtins.c */
void register_builtins(void);

/* functions in check.c */
int read_value(char **pstr, uint32_t *pindex, int implicit_paren);
void die(char *msg);
int check_list(uint32_t index, int count, int strict);
uint32_t store_pair(uint32_t first, uint32_t second);
uint32_t store_int32(int32_t num);
