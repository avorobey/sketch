#include <stdint.h>
#include <string>
#include <list>
#include <tr1/unordered_map>

using namespace std;

extern "C" void die(char *msg);

extern "C" int find_symbol(const char *name, int len, uint32_t *slot, uint32_t *frame);
extern "C" void add_symbol(const char *name, int len, uint32_t *slot, uint32_t *frame);
extern "C" void add_symbol_table();
extern "C" void delete_symbol_table();
extern "C" uint32_t latest_table_size();

void cpp_die(const char *msg) {
  die(const_cast<char *>(msg));
}

string sym_name(const char* name, int len) {
  string str;
  str.assign(name, len);
  return str;
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
  if (st.table.find(str) != st.table.end()) {
    *slot = st.table[str];
  } else {
    *slot = st.next;
    st.table[str] = st.next++;
  }
  *frame = 0;
}

uint32_t latest_table_size() {
  return tables.front().next-1;
}

