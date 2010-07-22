#include <stdint.h>
#include <string>
#include <tr1/unordered_map>

using namespace std;

/* returns 0 when not found */
extern "C" uint32_t get_symbol(const char *name, int len);
extern "C" void set_symbol(const char *name, int len, uint32_t val);

extern "C" void *new_symbol_table();
extern "C" void delete_symbol_table(void *table);
extern "C" uint32_t new_symbol(void *table, const char *name, int len);
extern "C" uint32_t num_symbols(void *table);

tr1::unordered_map<string,uint32_t> table;

string sym_name(const char* name, int len) {
  string str;
  str.assign(name, len);
  return str;
}

uint32_t get_symbol(const char *name, int len) {
  string str = sym_name(name, len);
  if (table.find(str) != table.end())
    return table[str];
  else return 0;
}

void set_symbol(const char *name, int len, uint32_t val) {
  table[sym_name(name, len)] = val;
}

struct symbol_table {
  uint32_t next;
  tr1::unordered_map<string, uint32_t> table;
};

void *new_symbol_table() {
  symbol_table *st = new symbol_table();
  st->next = 1;
  return (void *)st;
}

void delete_symbol_table(void *table) {
  symbol_table *st = (symbol_table *)table;
  delete st;
}

uint32_t new_symbol(void *table, const char *name, int len) {
  string str = sym_name(name, len);
  symbol_table *st = (symbol_table *)table;
  if (st->table.find(str) != st->table.end()) return st->table[str];
  st->table[str] = st->next++;
  return st->next-1;
}

uint32_t num_symbols(void *table) {
  symbol_table *st = (symbol_table *)table;
  return st->next-1;
}

