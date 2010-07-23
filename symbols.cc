#include <stdint.h>
#include <string>
#include <list>
#include <tr1/unordered_map>

using namespace std;

extern "C" void die(char *msg);

/* returns 0 when not found */
extern "C" uint32_t get_symbol(const char *name, int len);
extern "C" void set_symbol(const char *name, int len, uint32_t val);

extern "C" int find_symbol(const char *name, int len, uint32_t *slot, uint32_t *frame);
extern "C" void add_symbol(const char *name, int len, uint32_t *slot, uint32_t *frame);
extern "C" void add_symbol_table();
extern "C" void delete_symbol_table();
extern "C" uint32_t latest_table_size();

tr1::unordered_map<string,uint32_t> table;

void cpp_die(const char *msg) {
  die(const_cast<char *>(msg));
}

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

list<symbol_table> tables;

void add_symbol_table() {
  symbol_table st;
  st.next = 1;
  tables.push_front(st);
}

void delete_symbol_table() {
  if (tables.size() == 0) cpp_die("no symbol tables, can't delete one");
  tables.pop_front();
}

int find_symbol(const char *name, int len, uint32_t *slot, uint32_t *frame) {
  string str = sym_name(name, len);
  int pos = 0;
  for (list<symbol_table>::iterator it = tables.begin();
       it != tables.end(); ++it, ++pos) {
    if (it->table.find(str) != it->table.end()) {
      *slot = it->table[str];
      *frame = pos;
      return 1;
    }
  }
  return 0;
}

void add_symbol(const char *name, int len, uint32_t *slot, uint32_t *frame) {
  if (tables.size() == 0) cpp_die("no symbol tables, can't add a symbol");
  string str = sym_name(name, len);
  symbol_table& st = tables.front();
  *slot = st.next;
  *frame = 0;
  st.table[str] = st.next++;
}

uint32_t latest_table_size() {
  return tables.front().next-1;
}

