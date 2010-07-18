/* max arguments in a function call */
#define MAX_ARGS 256

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

#define BLTIN_MASK 16

#define CAR(i) (cells[i+1] >> 32)
#define CDR(i) (cells[i+1] & 0xFFFFFFFF)

#define SYMBOL_NAME(i) (char *)(cells+i+1)
#define SYMBOL_LEN(i) (cells[i] >> 32)

#define VECTOR_START(i) (uint32_t *)(cells+i+1)
#define VECTOR_LEN(i) (cells[i] >> 32)

#define LIST_LIKE(i) (TYPE(i) == T_PAIR || i == C_EMPTY)

#define MAX_CELLS 1000000
extern uint64_t cells[];
extern uint32_t next_cell;

/* special index values */
#define C_ERROR 0 
#define C_UNDEFINED 1
#define C_EMPTY 2
#define C_FALSE 3
#define C_TRUE 4

#define C_STARTFROM 5

#define CHECK_CELLS(i) do { if (next_cell + i >= MAX_CELLS) \
  die("out of cells"); } while(0)


uint32_t get_symbol(const char *name, int len);
void set_symbol(const char *name, int len, uint32_t val);
int read_value(char **pstr, uint32_t *pindex, int implicit_paren);


