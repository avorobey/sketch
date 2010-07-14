#include <stdint.h>
#include <string>
#include <tr1/unordered_map>

using namespace std;

/* returns 0 when not found */
extern "C" uint32_t get_symbol(const char *name, int len);
extern "C" void set_symbol(const char *name, int len, uint32_t val);

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
