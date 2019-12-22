#include <stdbool.h>
#include <stdint.h>
struct rptimpl;
struct rpt {
  // results appear in these fields.
  char *error;
  char *res;
  int32_t len;
  // parameters for init.
  bool breaklimits;
  bool disablelineno;
  // internal fields.
  struct rptimpl *impl;
};
void rptinit(struct rpt *t);
void rptfree(struct rpt *t);
bool rptparse(struct rpt *t, const char *str);
bool rptload(struct rpt *t, const char *filename);
bool rpteval(struct rpt *t, const char *exprstr);
bool rptprint(struct rpt *t, const char *exprstr);
void rpttrace(struct rpt *t, const char *exprstr);
void rptsetvars(struct rpt *t, const char **vars, int32_t varscount);
typedef bool (*rptbuiltinf)(struct rpt *t, char **r, const char *n, char **a);
// note: rptsetbuiltins does not copy the data, please retain the arrays until
// rptfree.
void rptsetbuiltins(struct rpt *t, const char **n, rptbuiltinf **f, int32_t c);
typedef bool (*rptloaderfunc)(struct rpt *t, const char *fname, void *userdata);
void rptsetloader(struct rpt *t, rptloaderfunc f, void *userdata);
