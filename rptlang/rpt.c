// read through the implementation notes in the readme before trying to make
// sense of this.

#define _GNU_SOURCE
#include "rpt.h"
#include <limits.h>
#include <setjmp.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

// maxtokens is the maximum number of tokens per line. you are doing something
// very wrong if you need more than this. please split your lines.
enum { maxtokens = 100 };

// the default limit for the various objects is 10M but circumstances dictate
// the actual limit. see rptinit for the logic.
enum { defaultlimit = 10000000 };

// check is like assert but always enabled.
#define check(cond) checkfunc(cond, #cond, __FILE__, __LINE__);
static void checkfunc(bool ok, const char *s, const char *file, int32_t line) {
  if (ok) return;
  printf("checkfail at %s:%d in \"%s\"\n", file, line, s);
  exit(1);
}

// same as malloc but guaranteed to succeed.
static void *alloc(int32_t sz) {
  void *p = malloc(sz);
  check(p != NULL);
  return p;
}

struct str {
  char *ptr;
  int32_t len;
};

// all the critbit tree related code is based on adam langley's paper so
// documentation about it lies there:
// https://www.imperialviolet.org/binary/critbit.pdf
struct strcritbitnode {
  // positive integers mean indices external nodes, they index into the
  // stringlist. negative numbers are internal nodes, they index into
  // strcbnlist.
  int32_t children[2];
  int32_t byte;
  int32_t otherbits;
};
// intcritbittrees are int to int maps. lower 32 bit of the indices are the
// keys, upper 32 bit are the values. since the keys are fixed 31 bit long,
// there is no need for the byte marker.
struct intcritbitnode {
  // negative numbers are internal nodes, they index into intcbnlist. positive
  // numbers work as described above.
  int64_t children[2];
  int32_t otherbits;
  // needed for iteration.
  int32_t parent;
};
struct icbtiterator {
  int64_t node;
  int32_t key, value;
};

enum valuetype {
  valueunevaluated,
  valueunderevaluation,
  valuestring,
  valuetuple,
  valuelist,
  valueerror,
};
enum valuedeftype {
  valuedeferror,
  valuedefstring,
  valuedeftupledef,
  valuedeflist,
  valuedefreference,
  valuedeffunction,
};
enum numtype {
  numtypeunknown,
  numtypenumber,
  numtypenotnumber,
};
struct value {
  enum valuetype type : 4;
  enum valuedeftype deftype : 4;
  enum numtype numtype : 4;
  bool beingprinted : 1;
  // if type is string or error then id refers to stringlist.
  // if type is tuple then id refers to tuplelist.
  // if type is list then id refers to listlist.
  // if type is unevaluated id refers to valuelist. it is the inherited
  // expression in that case. useful in case the evaluation needs the parent's
  // value (e.g. overriding tuple fields in tuples).
  int32_t id;
  // deftype describes what defid means:
  // valuedeferror -> stringlist: the constant string's value.
  // valuedefstring -> stringlist: the constant's value.
  // valuedeflist -> listlist: the list to which to copy the values from.
  // valuedeftupledef -> tupledeflist.
  // valuedefreference -> stringlist: the reference in dot notation.
  // valuedeffunction -> intlist: in the intlist the first integer is the
  // function's name, it is an index into the stringlist. the rest of the
  // integers (depending on the number of arguments the function has) are
  // indices into the valuelist.
  int32_t defid;
  // definition's file, a stringlist value.
  int32_t deffile;
  // definition's line number.
  int32_t defline;
  // field name in the context tuple for this value if this represents the value
  // for a specific field. refers to stringlist.
  int32_t name;
  // context value. refers to valuelist. follow this chain up to the global
  // tuple to find hints how to get to this value while debugging. it can even
  // point to a valuestring value to know for which variable is this value for.
  int32_t context;
  // if numtype is numtypenumber, num holds the numeric value of the number.
  int64_t num;
};

enum tuplefieldmask {
  tuplefieldnoprint = 1,
  tuplefieldcheck = 2,
};

struct tupledef {
  // the map of definitions. stringlist -> valuelist.
  int64_t deficbt;
  // tags on the fields. the key is a stringlist, the value is a tuplefieldmask
  // enum. fields that have no tags are not in this map (icbtlookup defaults
  // them to 0).
  int64_t tagsicbt;
  // parent's expression. 1 means no parent. 0 means undefined: take it from
  // outer's parent or empty if that does not have it.
  int32_t parentval;
};

struct tuple {
  // rpt fills fieldsicbt upon the first dereference of the tuple based on the
  // definition. it is a stringlist to valuelist mapping. it also copies the
  // definitions from parent's fieldsicbt. it also rewrites super references.
  // e.g. it splits "super.a.b.c" into "super.a 'b.c' !lookup2" and replaces the
  // "super.a" part with the definition from the parent's fieldsicbt. this way
  // fieldsicbt does not contain super references.
  int64_t fieldsicbt;
  // tags on the fields. the key is a stringlist, the value is a tuplefieldmask
  // enum. fields that have no tags are not in this map (icbtlookup defaults
  // them to 0).
  int64_t tagsicbt;
  // index into tupledeflist.
  int32_t defid;
};

struct list {
  int32_t len;
  // first is an index into the intlist. the next len integers in that list are
  // the indices into the valuelist.
  int32_t first;
};

enum tokentype {
  tokeneof,
  tokenseparator,  // newline or semicolon
  tokenerror,
  tokenstring,
  tokenreference,
  tokenfunction,
  tokentupleopen,
  tokentupleclose,
  tokentupleempty,
  tokenlistopen,
  tokenlistclose,
  tokenlistempty,
};

// valueid indexes into the valuelist.
typedef void (*builtinfunc)(struct rptimpl *ti, int32_t valueid);
struct builtindef {
  const char name[16];
  builtinfunc func;
};
static void bltncond(struct rptimpl *ti, int32_t vid);
static void bltnlookup(struct rptimpl *ti, int32_t vid);
static void bltnnumadd(struct rptimpl *ti, int32_t vid);
static void bltnnummul(struct rptimpl *ti, int32_t vid);
static void bltnnumsub(struct rptimpl *ti, int32_t vid);
static void bltnnumless(struct rptimpl *ti, int32_t vid);
static void bltnnumdiv(struct rptimpl *ti, int32_t vid);
static void bltnnummod(struct rptimpl *ti, int32_t vid);
static void bltnsysload(struct rptimpl *ti, int32_t vid);
static struct builtindef builtins[] = {
    {"dummy", NULL},         {"cond3", bltncond},
    {"numadd2", bltnnumadd}, {"numadd3", bltnnumadd},
    {"numadd4", bltnnumadd}, {"numadd5", bltnnumadd},
    {"numadd6", bltnnumadd}, {"numadd7", bltnnumadd},
    {"numadd8", bltnnumadd}, {"numadd9", bltnnumadd},
    {"nummul2", bltnnummul}, {"nummul3", bltnnummul},
    {"nummul4", bltnnummul}, {"nummul5", bltnnummul},
    {"nummul6", bltnnummul}, {"nummul7", bltnnummul},
    {"nummul8", bltnnummul}, {"nummul9", bltnnummul},
    {"numsub2", bltnnumsub}, {"numless2", bltnnumless},
    {"numdiv2", bltnnumdiv}, {"nummod2", bltnnummod},
    {"lookup2", bltnlookup}, {"sysload1", bltnsysload},
};
enum { builtinscnt = sizeof(builtins) / sizeof(builtins[0]) };

// these index into the strlist. use them to avoid strcmp.
enum {
  cstrnull,
  cstrempty,         // ""
  cstrnewline,       // "\n"
  cstrquestionmark,  // "?"
  cstrsuper,
  cstrnoprint,
  cstrcheck,
  cstrglobal,
  cstrmain,
  cstrthis,
  cstrup,
  cstrlookup2,
  cstrargs,
  cstrresult,
};

// these index into the valuelist.
enum {
  cvalnull,
  cvalemptytup,
  cvalglobaltup,
  cvalmaintup,
  cvalemptylist,
};

// these index into the listlist.
enum {
  clistnull,
  clistempty,
};

// these index into the tupledeflist.
enum {
  ctupnull,
  ctupempty,
  ctupglobal,
  ctupmain,
};

// note that rpt expects that all of the arrays in this struct except for the
// intstack to be zero initialized.
struct rptimpl {
  // strlist (stringlist from now on) is an array of pointers to zero terminated
  // strings. these strings are constant and valid until the rptfree. all
  // strings used through rpt must exist in stringlist. this way there is no
  // need to pass around pointers and lengths, just indices into this array. use
  // strlistadd to add a string to this array and get its index. strlistadd is
  // smart, it will not readd already existing strings into it, just return the
  // indices them.
  struct str *strlist;
  int32_t strlistcnt;
  // strlistcbt is the critbit tree root for the stringlist. used to quickly
  // look up the id of strings.
  int32_t strlistcbt;
  struct strcritbitnode *strcbnlist;
  int32_t strcbnlistcnt;

  // intcritbitree nodes for the various int to int mappings.
  struct intcritbitnode *intcbnlist;
  int32_t intcbnlistcnt;

  // use intlist for continuous arrays of integers. just store the first index
  // and remember how long the list is.
  int32_t *intlist;
  int32_t intlistcnt;

  int32_t *intstack;
  int32_t intstackcnt;

  char *strstack;
  int32_t strstackcnt;

  struct value *valuelist;
  int32_t valuelistcnt;

  struct tuple *tuplelist;
  int32_t tuplelistcnt;

  struct tupledef *tupledeflist;
  int32_t tupledeflistcnt;

  // listlist[1] is an empty list.
  struct list *listlist;
  int32_t listlistcnt;

  // keys are from stringlist, values index into the builtins array above. this
  // maps something like "numadd2" to the actual bltnnumadd function.
  int64_t builtinsicbt;

  // both keys and values are from stringlist. this maps "+" to "numadd2".
  int64_t operatorsicbt;

  // the size of the above arrays.
  int32_t limit;
  // the maximum number of nested valueeval() functions to avoid segfaults due
  // to unending recursion.
  int32_t evaldepthlimit;
  // length of the context in the error messages. limit to avoid overlong error
  // messages.
  int32_t contexterrorlimit;

  // number of valueeval() functions nested.
  int32_t evaldepth;

  // if true this structure is in bad state. rpt stops all further evaluation.
  // struct rpt's error field contains the error.
  bool inerror;

  // the topmost rpteval function's frame. if rpt reaches an error state, it
  // jumps back to this location. hasevalenv is just a debugging crutch to
  // detect places where i forgot setting up the evalenv.
  bool hasevalenv;
  jmp_buf evalenv;

  // the topmost parsing function's frame for quick exits. if the parser jumps
  // then it will set rpt struct's error field first.
  bool hasparserenv;
  jmp_buf parserenv;

  // helper variables for the parser follow.
  // the next byte in the input.
  const char *ptr;
  // info about the token last token nexttoken parsed.
  enum tokentype tokentype;
  // token is an index into the stringlist.
  int32_t token;
  // curfile is an id into stringlist.
  int32_t curfile;
  int32_t curline;

  // maps absolute paths to file contents, stringlist to stringlist.
  int64_t sysloadicbt;

  // reference to the owning structure.
  struct rpt *rpt;
};

static const char operators[][2][16] = {
    {"dummy", "dummy"}, {"+", "numadd2"}, {"*", "nummul2"}, {"-", "numsub2"},
    {"/", "numdiv2"}, {"%", "nummod2"},
};

static void limiterror(struct rptimpl *ti, const char *msg) {
  fprintf(stderr, "rpterror: %s\n", msg);
  ti->inerror = true;
  free(ti->rpt->error);
  ti->rpt->error = strdup(msg);
  ti->rpt->len = strlen(msg);
  check(ti->hasevalenv);
  longjmp(ti->evalenv, 1);
}

// todo: remove this.
__attribute__((format(printf, 2, 3))) static void parseerror(struct rptimpl *ti,
                                                             const char *fmt,
                                                             ...) {
  free(ti->rpt->error);
  char *p;
  va_list ap;
  va_start(ap, fmt);
  vasprintf(&p, fmt, ap);
  va_end(ap);
  const char *file = ti->strlist[ti->curfile].ptr;
  ti->rpt->len = asprintf(&ti->rpt->error, "%s:%d: %s", file, ti->curline, p);
  check(ti->rpt->len > 0);
}

static int32_t intlistadd(struct rptimpl *ti, int32_t cnt) {
  if (ti->intlistcnt + cnt > ti->limit) {
    limiterror(ti, "intlist limit reached.");
    return -1;
  }
  int32_t id = ti->intlistcnt;
  ti->intlistcnt += cnt;
  return id;
}

static void intstackadd(struct rptimpl *ti, int32_t val) {
  if (ti->intstackcnt + 1 > ti->limit) {
    limiterror(ti, "intstack limit reached.");
    return;
  }
  ti->intstack[ti->intstackcnt++] = val;
}

static char *strstackadd(struct rptimpl *ti, int32_t bytes) {
  if (ti->strstackcnt + bytes > ti->limit) {
    limiterror(ti, "strstack limit reached.");
    return NULL;
  }
  int32_t startidx = ti->strstackcnt;
  ti->strstackcnt += bytes;
  return ti->strstack + startidx;
}

static int32_t valuelistadd(struct rptimpl *ti) {
  if (ti->valuelistcnt + 1 > ti->limit) {
    limiterror(ti, "valuelist limit reached.");
    return -1;
  }
  return ti->valuelistcnt++;
}

static int32_t tuplelistadd(struct rptimpl *ti) {
  if (ti->tuplelistcnt + 1 > ti->limit) {
    limiterror(ti, "tuplelist limit reached.");
    return -1;
  }
  int32_t startid = ti->tuplelistcnt;
  ti->tuplelistcnt += 1;
  return startid;
}

static int32_t tupledeflistadd(struct rptimpl *ti) {
  if (ti->tupledeflistcnt + 1 > ti->limit) {
    limiterror(ti, "tupledeflist limit reached.");
    return -1;
  }
  return ti->tupledeflistcnt++;
}

static int32_t listlistadd(struct rptimpl *ti) {
  if (ti->listlistcnt + 1 > ti->limit) {
    limiterror(ti, "listlist limit reached.");
    return -1;
  }
  return ti->listlistcnt++;
}

// strlistadd makes a copy of str and appends it to the stringlist.  the passed
// in str does not need to be zero terminated but the stringlist will contain it
// as zero terminated. if len is -1 strlistadd assumes zero terminated string
// and calculates the length automatically. strlistadd returns the id of the
// added string. if the string is already in the stringlist, stringlistadd just
// returns the id of the existing string. this means that one can just free
// compare the reference ids instead of the full strings. strlistadd uses
// critbit tree to map the strings to ids.
static int32_t strlistadd(struct rptimpl *ti, const char *str, int32_t len) {
  if (ti->strlistcnt + 1 > ti->limit) {
    limiterror(ti, "strlist limit reached.");
    return -1;
  }
  if (len == -1) len = strlen(str);
  int32_t p = ti->strlistcbt;
  if (p == 0) {
    // this is the empty tree's case. add the string to the stringlist.
    ti->strlistcbt = ti->strlistcnt;
  } else {
    while (p < 0) {
      struct strcritbitnode *q = &ti->strcbnlist[-p];
      int32_t c = 0;
      if (q->byte < len) c = str[q->byte];
      int32_t direction = (1 + (q->otherbits | c)) >> 8;
      p = q->children[direction];
    }
    struct str *pp = &ti->strlist[p];
    int32_t newbyte;
    int32_t newotherbits;
    for (newbyte = 0; newbyte < len; newbyte++) {
      if (pp->ptr[newbyte] != str[newbyte]) {
        newotherbits = pp->ptr[newbyte] ^ str[newbyte];
        break;
      }
    }
    if (newbyte == len) {
      if (pp->len == len) {
        // the best match is the same as str so str is already in the
        // stringlist. return its index.
        return p;
      } else {
        newotherbits = pp->ptr[len];
      }
    }
    while ((newotherbits & (newotherbits - 1)) != 0) {
      newotherbits &= newotherbits - 1;
    }
    newotherbits ^= 0xff;

    // need to add str to the stringlist. check capacities, update the
    // cbtree, and then add the string to the stringlist.
    if (ti->strcbnlistcnt + 1 > ti->limit) {
      limiterror(ti, "strcbnlistcnt limit reached.");
      return -1;
    }

    // update the cbtree.
    int32_t c = pp->ptr[newbyte];
    int32_t newdirection = (1 + (newotherbits | c)) >> 8;
    int32_t newnodeid = ti->strcbnlistcnt++;
    struct strcritbitnode *newnode = &ti->strcbnlist[newnodeid];
    newnode->byte = newbyte;
    newnode->otherbits = newotherbits;
    newnode->children[1 - newdirection] = ti->strlistcnt;
    int32_t *wherep = &ti->strlistcbt;
    while (true) {
      int32_t p = *wherep;
      if (p > 0) break;
      struct strcritbitnode *q = &ti->strcbnlist[-p];
      if (q->byte > newbyte) break;
      if (q->byte == newbyte && q->otherbits > newotherbits) break;
      int32_t c = 0;
      if (q->byte < len) c = str[q->byte];
      int32_t direction = (1 + (q->otherbits | c)) >> 8;
      wherep = &q->children[direction];
    }
    newnode->children[newdirection] = *wherep;
    *wherep = -newnodeid;
  }
  // add the string to the stringlist.
  struct str *s = &ti->strlist[ti->strlistcnt];
  s->ptr = alloc(len + 1);
  s->len = len;
  memcpy(s->ptr, str, len);
  s->ptr[len] = 0;
  return ti->strlistcnt++;
}

// does not change ti->strstackcnt, returns the length of the string.
__attribute__((format(printf, 2, 3))) static int32_t strstackf(
    struct rptimpl *ti, const char *fmt, ...) {
  char *buf = ti->strstack + ti->strstackcnt;
  int32_t bufsz = ti->limit - ti->strstackcnt;
  va_list ap;
  va_start(ap, fmt);
  int32_t size = vsnprintf(buf, bufsz, fmt, ap);
  va_end(ap);
  check(size >= 0);
  if (size >= bufsz - 1) {
    limiterror(ti, "strstack limit reached.");
    return 0;
  }
  return size;
}

// looks up printf generated string in the stringlist.
__attribute__((format(printf, 2, 3))) static int32_t strlistf(
    struct rptimpl *ti, const char *fmt, ...) {
  char *buf = ti->strstack + ti->strstackcnt;
  int32_t bufsz = ti->limit - ti->strstackcnt;
  va_list ap;
  va_start(ap, fmt);
  int32_t size = vsnprintf(buf, bufsz, fmt, ap);
  va_end(ap);
  check(size >= 0);
  if (size >= bufsz - 1) {
    limiterror(ti, "strstack limit reached.");
    return 0;
  }
  int32_t id = strlistadd(ti, buf, size);
  return id;
}

// icbt means an integer to integer mapping critbit tree. icbtlookup returns 0
// if the given integer has no value in the icbt, the key's value otherwise.
// does not allocate anything, no need to check for errors afterwards.
static int32_t icbtlookup(struct rptimpl *ti, int64_t root, int32_t key) {
  int64_t p = root;
  if (p == 0) return 0;
  while (p < 0) {
    struct intcritbitnode *q = &ti->intcbnlist[-p];
    int32_t direction = (1LL + (q->otherbits | key)) >> 31;
    p = q->children[direction];
  }
  if ((p & 0x7fffffff) == key) return p >> 32;
  return 0;
}

// see icbtlookup for intro. only use positive keys and values. it is an error
// if the icbt already has the key.
static void icbtadd(struct rptimpl *ti, int64_t *root, int32_t key, int32_t v) {
  if (ti->intcbnlistcnt + 1 > ti->limit) {
    limiterror(ti, "intcbnlist limit reached.");
    return;
  }
  int64_t p = *root;
  if (p == 0) {
    *root = key | ((int64_t)v << 32);
    return;
  }

  while (p < 0) {
    struct intcritbitnode *q = &ti->intcbnlist[-p];
    int32_t direction = (1LL + (q->otherbits | key)) >> 31;
    p = q->children[direction];
  }
  check((p & 0x7fffffff) != key);
  int64_t newotherbits = (p & 0x7fffffff) ^ key;
  while ((newotherbits & (newotherbits - 1)) != 0) {
    newotherbits &= newotherbits - 1;
  }
  newotherbits ^= 0x7fffffff;
  int32_t newdirection = (1LL + (newotherbits | (p & 0x7fffffff))) >> 31;
  int32_t newnodeid = ti->intcbnlistcnt++;
  struct intcritbitnode *newnode = &ti->intcbnlist[newnodeid];
  newnode->otherbits = newotherbits;
  newnode->children[1 - newdirection] = key | ((int64_t)v << 32);
  int64_t parent = 0;
  int64_t *wherep = root;
  while (true) {
    int64_t p = *wherep;
    if (p > 0) break;
    struct intcritbitnode *q = &ti->intcbnlist[-p];
    if (q->otherbits > newotherbits) break;
    int32_t direction = (1LL + (q->otherbits | key)) >> 31;
    parent = p;
    wherep = &q->children[direction];
  }
  newnode->children[newdirection] = *wherep;
  newnode->parent = -parent;
  if (*wherep < 0) {
    ti->intcbnlist[-*wherep].parent = newnodeid;
  }
  *wherep = -newnodeid;
}

// set the it->node to the desired root, and keep the rest of the fields 0
// before the first iteration and iterate while icbtiterate keeps returning
// true. does not allocate anything, no need to check for errors afterwards.
static bool icbtiterate(struct rptimpl *ti, struct icbtiterator *it) {
  int64_t p = it->node;
  if (p == 0) return false;
  if (p > 0) {
    if (it->key > 0) return false;
    it->key = p & 0x7fffffff;
    it->value = p >> 32;
    return true;
  }
  // keep going upwards while the previous key was on the right side.
  int64_t lastnode = it->key | ((int64_t)it->value << 32);
  while (p != 0 && ti->intcbnlist[-p].children[1] == lastnode) {
    lastnode = p;
    p = -ti->intcbnlist[-p].parent;
  }
  if (p == 0) return false;
  // move or return right if previous key was the left child.
  if (ti->intcbnlist[-p].children[0] == lastnode) {
    it->node = p;
    p = ti->intcbnlist[-p].children[1];
    if (p > 0) {
      it->key = p & 0x7fffffff;
      it->value = p >> 32;
      return true;
    }
  }
  // keep going left to find the first leaf node.
  while (ti->intcbnlist[-p].children[0] < 0) {
    p = ti->intcbnlist[-p].children[0];
  }
  it->node = p;
  p = ti->intcbnlist[-p].children[0];
  it->key = p & 0x7fffffff;
  it->value = p >> 32;
  return true;
}

// extract the number of arguments from the function's name. does not allocate
// anything, no need to check for errors afterwards.
static int32_t argcnt(struct rptimpl *ti, int32_t strlistid) {
  check(ti->strlist[strlistid].len > 0);
  int32_t lastchar = ti->strlist[strlistid].ptr[ti->strlist[strlistid].len - 1];
  // todo: in the parser check that the function ends in a digit.
  check('0' <= lastchar && lastchar <= '9');
  return lastchar - '0';
}

// prints the context into strstack. returns the number of bytes printed.
static int32_t addcontext(struct rptimpl *ti, int32_t vid) {
  if (vid <= cvalmaintup) return 0;
  struct value *v = &ti->valuelist[vid];
  // todo: remove.
  struct value *ctxv = &ti->valuelist[v->context];
  if (ctxv->type != valuetuple && ctxv->type != valuelist) {
    return addcontext(ti, v->context);
  }
  int32_t len = addcontext(ti, v->context);
  if (len >= ti->contexterrorlimit) {
    if (ti->strstack[ti->strstackcnt - 1] == 0) {
      char *p = strstackadd(ti, 3);
      p[-1] = '.';
      p[0] = '.';
      p[1] = '.';
      p[2] = 0;
      ti->strstackcnt--;
    }
    return len;
  }
  if (len == 0) {
    *strstackadd(ti, 1) = 0;
    len++;
  } else {
    char *p = strstackadd(ti, 1);
    p[-1] = '.';
    p[0] = 0;
    len++;
  }
  if (v->name == 0) {
    char *p = strstackadd(ti, 1);
    p[-1] = '?';
    p[0] = 0;
    len++;
  } else {
    int32_t vlen = ti->strlist[v->name].len;
    char *p = strstackadd(ti, vlen);
    memcpy(p - 1, ti->strlist[v->name].ptr, vlen + 1);
    len += vlen;
  }
  return len;
}

static void valerror(struct rptimpl *ti, int32_t vid, const char *str) {
  // todo: remove backticks and newlines.
  int32_t stackstart = ti->strstackcnt;
  char *ctxbuf = ti->strstack + stackstart;
  addcontext(ti, vid);
  *strstackadd(ti, 1) = 0;
  struct value *v = &ti->valuelist[vid];
  check(v->type == valueunevaluated || v->type == valueunderevaluation);
  char *f = ti->strlist[v->deffile].ptr;
  int32_t ln = v->defline;
  v->type = valueerror;
  if (ti->rpt->disablelineno) {
    v->id = strlistf(ti, "%s %s: %s", f, ctxbuf, str);
  } else {
    v->id = strlistf(ti, "%s:%d %s: %s", f, ln, ctxbuf, str);
  }
  ti->strstackcnt = stackstart;
}

__attribute__((format(printf, 3, 4))) static void valerrorf(struct rptimpl *ti,
                                                            int32_t vid,
                                                            const char *fmt,
                                                            ...) {
  int32_t stackstart = ti->strstackcnt;
  char *buf = ti->strstack + ti->strstackcnt;
  int32_t buflen = ti->limit - ti->strstackcnt;
  va_list ap;
  va_start(ap, fmt);
  int32_t len = vsnprintf(buf, buflen, fmt, ap);
  va_end(ap);
  if (len >= buflen) {
    limiterror(ti, "strstack limit reached.");
    return;
  }
  ti->strstackcnt += len + 1;
  valerror(ti, vid, buf);
  ti->strstackcnt = stackstart;
}

// make a deep copy of a value. return the new value's index.
static int32_t copyvalue(struct rptimpl *ti, int32_t vid, int32_t contextvid) {
  struct value *v = &ti->valuelist[vid];
  int32_t nvid = valuelistadd(ti);
  struct value *nv = &ti->valuelist[nvid];
  *nv = *v;
  nv->context = contextvid;
  nv->name = 0;
  if (v->deftype == valuedefstring) {
    // no need to reevaluate constant strings.
    return nvid;
  }
  if (v->deftype == valuedeflist && v->defid == cvalemptylist) {
    // no need to reevaluate the empty list.
    return nvid;
  }
  nv->type = valueunevaluated;
  nv->id = 0;
  nv->numtype = numtypeunknown;
  if (nv->deftype == valuedeflist) {
    nv->type = valuelist;
    nv->id = listlistadd(ti);
    struct list *oldlist = &ti->listlist[v->id];
    struct list *newlist = &ti->listlist[nv->id];
    int32_t len = oldlist->len;
    newlist->len = len;
    newlist->first = intlistadd(ti, len);
    int32_t *oldarray = ti->intlist + oldlist->first;
    int32_t *newarray = ti->intlist + newlist->first;
    for (int32_t i = 0; i < len; i++) {
      newarray[i] = copyvalue(ti, oldarray[i], nvid);
      ti->valuelist[newarray[i]].name = strlistf(ti, "%d", i);
    }
  } else if (nv->deftype == valuedeffunction) {
    int32_t *oldargs = ti->intlist + v->defid;
    int32_t argc = argcnt(ti, oldargs[0]);
    nv->defid = intlistadd(ti, argc + 1);
    int32_t *newargs = ti->intlist + nv->defid;
    newargs[0] = oldargs[0];
    for (int32_t i = 1; i <= argc; i++) {
      newargs[i] = copyvalue(ti, oldargs[i], nvid);
    }
  }
  return nvid;
}

// recursively rewrite every super reference. replace them with the definition
// in ptid (parenttupid).
void rewritesupers(struct rptimpl *ti, int32_t ptid, int32_t vid, int32_t ctx) {
  if (vid == 0 || vid == 1) {
    // this is the no parent tuple case.
    return;
  }
  struct value *v = &ti->valuelist[vid];
  if (v->deftype == valuedeferror || v->deftype == valuedefstring) {
    // no recursion needed in these cases.
    return;
  }
  if (v->deftype == valuedeflist) {
    check(v->id != 0);
    int32_t *array = ti->intlist + ti->listlist[v->id].first;
    int32_t len = ti->listlist[v->id].len;
    for (int32_t i = 0; i < len; i++) {
      rewritesupers(ti, ptid, array[i], vid);
    }
    return;
  }
  if (v->deftype == valuedeftupledef) {
    rewritesupers(ti, ptid, ti->tupledeflist[v->defid].parentval, ctx);
    return;
  }
  if (v->deftype == valuedeffunction) {
    // todo: function's name cannot start with super. check, test and document
    // this.
    int32_t *args = ti->intlist + v->defid;
    int32_t argc = argcnt(ti, args[0]);
    for (int32_t i = 1; i <= argc; i++) {
      rewritesupers(ti, ptid, args[i], ctx);
    }
    return;
  }
  check(v->deftype == valuedefreference);
  // resolve super references here. e.g. in "super.x.y.z" replace "super.x" with
  // the definition from the parent.
  // split off the first part.
  char *refstr = ti->strlist[v->defid].ptr;
  char *sep = refstr;
  while (*sep != 0 && *sep != '.' && *sep != ':') sep++;
  if (strlistadd(ti, refstr, sep - refstr) != cstrsuper) return;
  if (*sep != '.') {
    valerrorf(ti, vid, ". must follow super in %s.", refstr);
    return;
  }
  sep++;
  // we ate "super.". split off the second part.
  if (ptid == 0) {
    valerror(ti, vid, "missing parent context for super.");
    return;
  }
  refstr = sep;
  while (*sep != 0 && *sep != '.' && *sep != ':') sep++;
  int32_t supersid = strlistadd(ti, refstr, sep - refstr);
  struct tuple *parenttup = &ti->tuplelist[ptid];
  int32_t supervid = icbtlookup(ti, parenttup->fieldsicbt, supersid);
  if (supervid == 0) {
    char *superrefstr = ti->strlist[supersid].ptr;
    valerrorf(ti, vid, "%s not found in parent.", superrefstr);
    v->defid = v->id;
    v->deftype = valuedeferror;
    return;
  }
  if (*sep == 0) {
    // no more parts in the reference, replace vid with the found value.
    *v = ti->valuelist[copyvalue(ti, supervid, ctx)];
    return;
  }
  // this is the case where we have something like "super.a.b.c". rewrite this
  // into "parenta '.b.c' !lookup2".
  int32_t arg1id = copyvalue(ti, supervid, vid);
  int32_t arg2id = valuelistadd(ti);
  struct value *arg2 = &ti->valuelist[arg2id];
  arg2->type = valuestring;
  arg2->deftype = valuedefstring;
  arg2->id = strlistadd(ti, sep, -1);
  arg2->defid = arg2->id;
  arg2->context = vid;
  v->deftype = valuedeffunction;
  v->defid = intlistadd(ti, 3);
  int32_t *args = ti->intlist + v->defid;
  args[0] = cstrlookup2;
  args[1] = arg1id;
  args[2] = arg2id;
}

static void copyresult(struct rptimpl *ti, int32_t tovid, int32_t fromvid) {
  struct value *to = &ti->valuelist[tovid];
  struct value *from = &ti->valuelist[fromvid];
  to->type = from->type;
  to->id = from->id;
  to->context = from->context;
  to->name = from->name;
}

// split off the first part of a reference. e.g. "a.b.c" -> ["a", "b.c"]. the
// input is in the tail variable. splitref will update both head and tail to the
// new values. the returning value is the character that preceded head. can be
// either . or :. for the last part *tail will point to the terminating 0 byte.
// if tail is empty, head set head to 0.
static char splitref(struct rptimpl *ti, int32_t *head, char **tail) {
  char *headptr = *tail;
  if (*headptr == 0) {
    *head = 0;
    return 0;
  }
  char sep = ':';
  if (*headptr == '.') {
    sep = '.';
    headptr++;
  } else if (*headptr == ':') {
    headptr++;
  }
  char *ptr = headptr;
  while (*ptr != 0 && *ptr != '.' && *ptr != ':') {
    ptr++;
  }
  *head = strlistadd(ti, headptr, ptr - headptr);
  *tail = ptr;
  return sep;
}

static void valueeval(struct rptimpl *ti, int32_t vid);

struct derefdata {
  // the reference to look up. refers to the stringlist.
  int32_t ref;
  // set to true if deref should parse this, global, up, file references.
  bool dokeywords;
  // the starting point of the reference traversal.
  int32_t startvid;
  // if deref encountered error, it will propagate the error to this value.
  int32_t errorvid;
  // deref puts the resulting value's id into this member.
  int32_t resultvid;
};
// returns true if success or false in case of error. at the end resultvid will
// contain the dereferenced value. in case of error deref propagates the error
// to errorvid.
static bool deref(struct rptimpl *ti, struct derefdata *data) {
  int32_t refhead = 0;
  char *reftail = ti->strlist[data->ref].ptr;
  int32_t refvid = data->startvid;
  char sep = splitref(ti, &refhead, &reftail);
  if (data->dokeywords) {
    if (refhead == cstrthis) {
      // this actually does not do much other than makes deref eat it the first
      // name in the reference. need to make sure the context is a tuple though.
      // skip the debug contexts.
      while (ti->valuelist[refvid].type != valuetuple) {
        refvid = ti->valuelist[refvid].context;
      }
      sep = splitref(ti, &refhead, &reftail);
    } else if (refhead == cstrglobal) {
      refvid = cvalglobaltup;
      sep = splitref(ti, &refhead, &reftail);
    } else {
      while (refhead == cstrup) {
        // this is a loop because need to skip non-tuple context references.
        // those are for debugging and diagnostics, they are not real context
        // tuples.
        while (true) {
          if (refvid == cvalglobaltup) {
            valerror(ti, data->errorvid, "up references go outside global.");
            return false;
          }
          if (ti->valuelist[refvid].type == valuetuple) break;
          refvid = ti->valuelist[refvid].context;
        }
        refvid = ti->valuelist[refvid].context;
        // again, skip the non-tuple stuff.
        while (ti->valuelist[refvid].type != valuetuple) {
          refvid = ti->valuelist[refvid].context;
        }
        sep = splitref(ti, &refhead, &reftail);
      }
    }
  }
  // this loop keeps looking up refhead in refvid. if there are no more
  // dereferencing to be done, refvid will contain the dereferenced value.
  while (refhead != 0) {
    struct value *refv = &ti->valuelist[refvid];
    // if refvid is an error, propagate it upwards.
    if (refv->type == valueerror) {
      copyresult(ti, data->errorvid, refvid);
      return false;
    }
    // if refvid is tuple, look up refhead in it.
    if (refv->type == valuetuple) {
      struct tuple *reftup = &ti->tuplelist[refv->id];
      int32_t r = icbtlookup(ti, reftup->fieldsicbt, refhead);
      if (r != 0) {
        // reference found, go into it.
        refvid = r;
        sep = splitref(ti, &refhead, &reftail);
        if (refhead != 0) valueeval(ti, refvid);
        continue;
      }
      if (sep == '.') {
        char oldc = *reftail;
        *reftail = 0;
        char *prefix = ti->strlist[data->ref].ptr;
        valerrorf(ti, data->errorvid, "%s not found.", prefix);
        *reftail = oldc;
        return false;
      }
    }
    // if the lookup is closed, try looking up stuff in lists.
    if (sep == '.' && refv->type == valuelist) {
      char *endptr = NULL;
      char *numstr = ti->strlist[refhead].ptr;
      long long val = strtoll(numstr, &endptr, 0);
      if (*endptr != 0) {
        valerrorf(ti, data->errorvid, "%s is not a number.", numstr);
        return false;
      }
      if (val < 0 || val >= ti->listlist[refv->id].len) {
        valerrorf(ti, data->errorvid, "index %lld is out of range.", val);
        return false;
      }
      refvid = ti->intlist[ti->listlist[refv->id].first + val];
      sep = splitref(ti, &refhead, &reftail);
      if (refhead != 0) valueeval(ti, refvid);
      continue;
    }
    // if the lookup is closed we should have found something by now.
    if (sep == '.') {
      char oldc = *reftail;
      *reftail = 0;
      char *prefix = ti->strlist[data->ref].ptr;
      valerrorf(ti, data->errorvid, "%s is not a lookup in a tuple.", prefix);
      *reftail = oldc;
      return false;
    }
    // in the case of an open lookup we can go upwards.
    if (refvid == cvalglobaltup) {
      char *refstr = ti->strlist[data->ref].ptr;
      valerrorf(ti, data->errorvid, "%s not found.", refstr);
      return false;
    }
    refvid = refv->context;
  }
  data->resultvid = refvid;
  return true;
}

static void valueeval(struct rptimpl *ti, int32_t vid) {
  struct value *v = &ti->valuelist[vid];
  if (v->type == valueunderevaluation) {
    valerror(ti, vid, "cyclic reference.");
    return;
  }
  if (v->type != valueunevaluated) {
    check(v->id > 0);
    return;
  }
  v->type = valueunderevaluation;
  if (ti->evaldepth >= ti->evaldepthlimit) {
    valerror(ti, vid, "eval depth too large.");
    return;
  }
  ti->evaldepth++;
  if (v->deftype == valuedeferror) {
    valerror(ti, vid, ti->strlist[v->defid].ptr);
  } else if (v->deftype == valuedefreference) {
    struct derefdata derefdata;
    derefdata.ref = v->defid;
    derefdata.dokeywords = true;
    derefdata.startvid = v->context;
    derefdata.errorvid = vid;
    if (deref(ti, &derefdata)) {
      valueeval(ti, derefdata.resultvid);
      copyresult(ti, vid, derefdata.resultvid);
    }
  } else if (v->deftype == valuedeffunction) {
    int32_t func = ti->intlist[v->defid];
    char *funcstr = ti->strlist[func].ptr;
    int32_t builtinid = icbtlookup(ti, ti->builtinsicbt, func);
    if (builtinid != 0) {
      builtins[builtinid].func(ti, vid);
      check(v->type != valueunevaluated);
      check(v->id != 0);
      ti->evaldepth--;
      return;
    }
    struct derefdata derefdata;
    derefdata.ref = func;
    derefdata.dokeywords = true;
    derefdata.startvid = v->context;
    derefdata.errorvid = vid;
    if (!deref(ti, &derefdata)) {
      goto done;
    }
    int32_t ftvid = derefdata.resultvid;
    valueeval(ti, ftvid);
    // ftv = function tuple value.
    struct value *ftv = &ti->valuelist[ftvid];
    if (ftv->type != valuetuple) {
      valerrorf(ti, vid, "function %s not a tuple.", funcstr);
      goto done;
    }
    if (!icbtlookup(ti, ti->tuplelist[ftv->id].fieldsicbt, cstrresult)) {
      valerrorf(ti, vid, "result not found in function %s.", funcstr);
      goto done;
    }
    // nftv = new function tuple value.
    int32_t nftvid = valuelistadd(ti);
    struct value *nftv = &ti->valuelist[nftvid];
    int32_t nftupid = tuplelistadd(ti);
    nftv->type = valuetuple;
    nftv->id = nftupid;
    nftv->deffile = v->deffile;
    nftv->defline = v->defline;
    nftv->context = vid;
    struct tuple *nftup = &ti->tuplelist[nftupid];
    struct icbtiterator it = {ti->tuplelist[ftv->id].fieldsicbt, 0, 0};
    int32_t fresvid = 0;
    while (icbtiterate(ti, &it)) {
      if (it.key == cstrargs) {
        valerrorf(ti, vid, "function %s already has args.", funcstr);
        goto done;
      }
      int32_t newval = copyvalue(ti, it.value, nftvid);
      if (it.key == cstrresult) fresvid = newval;
      icbtadd(ti, &nftup->fieldsicbt, it.key, newval);
    }
    check(fresvid != 0);
    int32_t arglistid = listlistadd(ti);
    struct list *arglist = &ti->listlist[arglistid];
    arglist->len = argcnt(ti, func);
    arglist->first = v->defid + 1;
    int32_t arglistvid = valuelistadd(ti);
    struct value *arglistv = &ti->valuelist[arglistvid];
    arglistv->type = valuelist;
    arglistv->deftype = valuedeflist;
    arglistv->id = arglistid;
    arglistv->defid = arglistid;
    arglistv->deffile = v->deffile;
    arglistv->defline = v->defline;
    arglistv->context = nftvid;
    icbtadd(ti, &nftup->fieldsicbt, cstrargs, arglistvid);
    valueeval(ti, fresvid);
    copyresult(ti, vid, fresvid);
  } else if (v->deftype == valuedeftupledef) {
    struct tupledef *tupdef = &ti->tupledeflist[v->defid];
    int32_t tupid = tuplelistadd(ti);
    struct tuple *tup = &ti->tuplelist[tupid];
    tup->defid = v->defid;
    int32_t nparentval = 0;
    int32_t parentval = tupdef->parentval;
    if (parentval == 0) {
      parentval = v->id;
    }
    int32_t parenttupid = 0;
    if (parentval > 1) {
      nparentval = copyvalue(ti, parentval, vid);
      valueeval(ti, nparentval);
      struct value *pv = &ti->valuelist[nparentval];
      parenttupid = pv->id;
      if (pv->type == valueerror) {
        copyresult(ti, vid, nparentval);
        ti->evaldepth--;
        return;
      }
      if (pv->type != valuetuple) {
        valerror(ti, vid, "parent is not a tuple.");
        ti->evaldepth--;
        return;
      }
    }
    struct icbtiterator it = {tupdef->deficbt, 0, 0};
    while (icbtiterate(ti, &it)) {
      int32_t newvalue = copyvalue(ti, it.value, vid);
      ti->valuelist[newvalue].name = it.key;
      rewritesupers(ti, parenttupid, newvalue, vid);
      icbtadd(ti, &tup->fieldsicbt, it.key, newvalue);
    }
    it = (struct icbtiterator){tupdef->tagsicbt, 0, 0};
    while (icbtiterate(ti, &it)) {
      icbtadd(ti, &tup->tagsicbt, it.key, it.value);
    }
    if (nparentval != 0) {
      struct tuple *ptup = &ti->tuplelist[parenttupid];
      it = (struct icbtiterator){ptup->tagsicbt, 0, 0};
      while (icbtiterate(ti, &it)) {
        if (icbtlookup(ti, tup->fieldsicbt, it.key) == 0) {
          // inherit tags for inherited fields.
          icbtadd(ti, &tup->tagsicbt, it.key, it.value);
        }
      }
      it = (struct icbtiterator){ptup->fieldsicbt, 0, 0};
      while (icbtiterate(ti, &it)) {
        int32_t newfieldvid = icbtlookup(ti, tup->fieldsicbt, it.key);
        if (newfieldvid == 0) {
          // no need to call rewritesupers because it cannot contain super
          // references.
          int32_t newvalue = copyvalue(ti, it.value, vid);
          ti->valuelist[newvalue].name = it.key;
          icbtadd(ti, &tup->fieldsicbt, it.key, newvalue);
        } else {
          struct value *newfieldv = &ti->valuelist[newfieldvid];
          if (newfieldv->type == valueunevaluated) {
            // save the parent's value for future super references.
            newfieldv->id = it.value;
          }
        }
      }
    }
    v->type = valuetuple;
    v->id = tupid;
  } else {
    check(false);
  }
done:
  ti->evaldepth--;
}

void rptinit(struct rpt *t) {
  t->impl = alloc(sizeof(struct rptimpl));
  struct rptimpl *ti = t->impl;
  memset(ti, 0, sizeof(*ti));
  ti->rpt = t;

  ti->limit = defaultlimit;
  // on 32 bit systems the memory is sparse, use smaller limits there.
  if (sizeof(intptr_t) == 4) {
    ti->limit /= 10;
  }
  ti->evaldepthlimit = 1000;
  ti->contexterrorlimit = 100;
  if (t->breaklimits) {
    ti->limit *= 10;
    ti->evaldepthlimit *= 10;
    ti->contexterrorlimit *= 10;
  }
  ti->strlist = calloc(ti->limit, sizeof(ti->strlist[0]));
  ti->strcbnlist = calloc(ti->limit, sizeof(ti->strcbnlist[0]));
  ti->intcbnlist = calloc(ti->limit, sizeof(ti->intcbnlist[0]));
  ti->intlist = calloc(ti->limit, sizeof(ti->intlist[0]));
  ti->intstack = calloc(ti->limit, sizeof(ti->intstack[0]));
  ti->strstack = calloc(ti->limit, sizeof(ti->strstack[0]));
  ti->valuelist = calloc(ti->limit, sizeof(ti->valuelist[0]));
  ti->tuplelist = calloc(ti->limit, sizeof(ti->tuplelist[0]));
  ti->tupledeflist = calloc(ti->limit, sizeof(ti->tupledeflist[0]));
  ti->listlist = calloc(ti->limit, sizeof(ti->listlist[0]));
  check(ti->strlist != NULL);
  check(ti->strcbnlist != NULL);
  check(ti->intcbnlist != NULL);
  check(ti->intlist != NULL);
  check(ti->intstack != NULL);
  check(ti->strstack != NULL);
  check(ti->valuelist != NULL);
  check(ti->tuplelist != NULL);
  check(ti->tupledeflist != NULL);
  check(ti->listlist != NULL);
  // set the starting counts to 1 because 0 means special value in some cases.
  ti->strlistcnt = 1;
  ti->strcbnlistcnt = 1;
  ti->intcbnlistcnt = 1;
  ti->intlistcnt = 1;
  ti->valuelistcnt = 1;
  ti->tuplelistcnt = 1;
  ti->tupledeflistcnt = 1;
  // ti->listlist[1] is an empty list.
  ti->listlistcnt = 2;

  // add the known strings.
  check(strlistadd(ti, "", -1) == cstrempty);
  check(strlistadd(ti, "\n", -1) == cstrnewline);
  check(strlistadd(ti, "?", -1) == cstrquestionmark);
  check(strlistadd(ti, "super", -1) == cstrsuper);
  check(strlistadd(ti, "noprint", -1) == cstrnoprint);
  check(strlistadd(ti, "check", -1) == cstrcheck);
  check(strlistadd(ti, "global", -1) == cstrglobal);
  check(strlistadd(ti, "main", -1) == cstrmain);
  check(strlistadd(ti, "this", -1) == cstrthis);
  check(strlistadd(ti, "up", -1) == cstrup);
  check(strlistadd(ti, "lookup2", -1) == cstrlookup2);
  check(strlistadd(ti, "args", -1) == cstrargs);
  check(strlistadd(ti, "result", -1) == cstrresult);

  // add the operators.
  int32_t operatorscnt = sizeof(operators) / sizeof(operators[0]);
  for (int32_t i = 1; i < operatorscnt; i++) {
    int32_t opid = strlistadd(ti, operators[i][0], -1);
    int32_t funcid = strlistadd(ti, operators[i][1], -1);
    icbtadd(ti, &ti->operatorsicbt, opid, funcid);
  }

  // create the empty, global and main tuples.
  check(tupledeflistadd(ti) == ctupempty);
  check(tupledeflistadd(ti) == ctupglobal);
  check(tupledeflistadd(ti) == ctupmain);
  struct tupledef *emptydef = &ti->tupledeflist[ctupempty];
  emptydef->parentval = cvalemptytup;
  struct tupledef *globaldef = &ti->tupledeflist[ctupglobal];
  globaldef->parentval = cvalemptytup;
  struct tupledef *maindef = &ti->tupledeflist[ctupmain];
  maindef->parentval = cvalemptytup;
  check(valuelistadd(ti) == cvalemptytup);
  struct value *emptyv = &ti->valuelist[cvalemptytup];
  emptyv->deftype = valuedeftupledef;
  emptyv->defid = ctupempty;
  check(valuelistadd(ti) == cvalglobaltup);
  struct value *globalv = &ti->valuelist[cvalglobaltup];
  globalv->deftype = valuedeftupledef;
  globalv->defid = ctupglobal;
  check(valuelistadd(ti) == cvalmaintup);
  struct value *mainv = &ti->valuelist[cvalmaintup];
  mainv->deftype = valuedeftupledef;
  mainv->defid = ctupmain;
  mainv->context = cvalglobaltup;
  mainv->name = cstrmain;
  icbtadd(ti, &globaldef->deficbt, strlistadd(ti, "main", -1), cvalmaintup);

  // add the empty value.
  check(valuelistadd(ti) == cvalemptylist);
  struct value *emptylistval = &ti->valuelist[cvalemptylist];
  emptylistval->type = valuelist;
  emptylistval->id = clistempty;
  emptylistval->deftype = valuedeflist;

  // add the builtins.
  for (int32_t i = 1; i < builtinscnt; i++) {
    int32_t nameid = strlistadd(ti, builtins[i].name, -1);
    icbtadd(ti, &ti->builtinsicbt, nameid, i);
    int32_t vid = valuelistadd(ti);
    struct value *v = &ti->valuelist[vid];
    v->deftype = valuedeftupledef;
    // set the the builtin tuple's to empty.
    v->defid = 1;
    icbtadd(ti, &globaldef->deficbt, nameid, vid);
  }

  // both the empty and global tuple's definitions are ready. instantiate them.
  valueeval(ti, cvalemptytup);
  valueeval(ti, cvalglobaltup);

  ti->curfile = strlistadd(ti, "input", -1);

  // everything above should fit into the limits.
  check(!ti->inerror);
}

void rptfree(struct rpt *t) {
  struct rptimpl *ti = t->impl;
  for (int32_t i = 1; i < ti->strlistcnt; i++) {
    free(ti->strlist[i].ptr);
  }
  free(ti->strlist);
  free(ti->strcbnlist);
  free(ti->intcbnlist);
  free(ti->intlist);
  free(ti->intstack);
  free(ti->strstack);
  free(ti->valuelist);
  free(ti->tuplelist);
  free(ti->tupledeflist);
  free(ti->listlist);
  free(ti);
}

__attribute__((format(printf, 2, 3))) static void parsererror(
    struct rptimpl *ti, const char *fmt, ...) {
  free(ti->rpt->error);
  char *p;
  va_list ap;
  va_start(ap, fmt);
  vasprintf(&p, fmt, ap);
  va_end(ap);
  const char *file = ti->strlist[ti->curfile].ptr;
  ti->rpt->len = asprintf(&ti->rpt->error, "%s:%d: %s", file, ti->curline, p);
  check(ti->rpt->len > 0);
  printf("parsererror: %s\n", ti->rpt->error);
  check(ti->hasparserenv);
  longjmp(ti->parserenv, 1);
}

static bool iswhitespace(int32_t ch) {
  return ch == ' ' || ch == '\t' || ch == '\n' || ch == 0;
}

static bool ishex(int32_t ch) {
  return ('0' <= ch && ch <= '9') || ('a' <= ch && ch <= 'f');
}

// fills the ti->tokentype and ti->token fields. also updates ti->ptr and
// ti->curline.
static void nexttoken(struct rptimpl *ti) {
  if (ti->tokentype == tokenseparator && ti->token == cstrnewline) {
    // last token was a newline, increment the line counter only now.
    ti->curline++;
  }
  // skip whitespace.
  while (true) {
    if (*ti->ptr == ' ' || *ti->ptr == '\t') {
      ti->ptr++;
      continue;
    }
    if (*ti->ptr == '\\') {
      if (*(ti->ptr + 1) == '\n') {
        ti->curline++;
        ti->ptr += 2;
        continue;
      }
      parsererror(ti, "invalid '\\' character.");
      return;
    }
    break;
  }
  // skip comments.
  if (*ti->ptr == '#') {
    while (*ti->ptr != 0 && *ti->ptr != '\n') {
      ti->ptr++;
    }
  }
  const char *start = ti->ptr;
  if (*ti->ptr == 0) {
    ti->tokentype = tokeneof;
    ti->token = strlistadd(ti, start, 0);
    return;
  }
  if (start[0] == '{' && start[1] == '}' && iswhitespace(start[2])) {
    ti->tokentype = tokentupleempty;
    ti->token = strlistadd(ti, start, 2);
    ti->ptr += 2;
    return;
  }
  if (start[0] == '[' && start[1] == ']' && iswhitespace(start[2])) {
    ti->tokentype = tokenlistempty;
    ti->token = strlistadd(ti, start, 2);
    ti->ptr += 2;
    return;
  }
  const char *match;
  const char *typestr = ";{}[]";
  if ((match = strchr(typestr, *ti->ptr)) != NULL) {
    if (iswhitespace(*(ti->ptr + 1))) {
      enum tokentype types[] = {
          tokenseparator, tokentupleopen, tokentupleclose,
          tokenlistopen,  tokenlistclose,
      };
      ti->tokentype = types[match - typestr];
      ti->token = strlistadd(ti, start, 1);
      ti->ptr++;
      return;
    }
    parsererror(ti, "'%c' must be followed by whitespace.", *ti->ptr);
    return;
  }
  if (*ti->ptr == '\n') {
    ti->tokentype = tokenseparator;
    ti->token = strlistadd(ti, start, 1);
    ti->ptr++;
    return;
  }
  if (*ti->ptr == '`') {
    start++;
    ti->ptr++;
    while (*ti->ptr != '`') {
      if (*ti->ptr < 32) {
        parsererror(ti, "unterminated error string.");
        return;
      }
      ti->ptr++;
    }
    ti->tokentype = tokenerror;
    ti->token = strlistadd(ti, start, ti->ptr++ - start);
    return;
  }
  if (*ti->ptr == '"') {
    ti->ptr++;
    int32_t stkcnt = ti->strstackcnt;
    char *buf = ti->strstack + stkcnt;
    int32_t stkstart = stkcnt;
    while (*ti->ptr != '"') {
      if (stkcnt >= ti->limit) {
        limiterror(ti, "strstack full while parsing.");
        return;
      }
      if (*ti->ptr < 32) {
        parsererror(ti, "unterminated double quoted string.");
        return;
      }
      if (*ti->ptr == '\\') {
        ti->ptr++;
        int32_t ch = 0;
        switch (*ti->ptr) {
          case '"':
          case '\\':
            ch = *ti->ptr;
            break;
          case 'n':
            ch = '\n';
            break;
          case 't':
            ch = '\t';
            break;
          case 'x':
            ti->ptr++;
            if (!ishex(ti->ptr[0]) || !ishex(ti->ptr[1])) {
              parsererror(ti, "invalid hex sequence.");
              return;
            }
            ch = *ti->ptr < 'a' ? *ti->ptr - '0' : *ti->ptr - 'a' + 10;
            ti->ptr++;
            ch *= 16;
            ch += *ti->ptr < 'a' ? *ti->ptr - '0' : *ti->ptr - 'a' + 10;
            break;
          default:
            for (int32_t i = 0; i < 3; i++) {
              if (*ti->ptr < '0' || '7' < *ti->ptr) {
                parsererror(ti, "invalid escape sequence.");
                return;
              }
              ch = ch * 8 + *ti->ptr++ - '0';
            }
            ti->ptr--;
            break;
        }
        if (ch == 0) {
          parsererror(ti, "0 byte in string. that is not allowed.");
          return;
        }
        *buf++ = ch;
        stkcnt++;
      } else {
        *buf++ = *ti->ptr;
        stkcnt++;
      }
      ti->ptr++;
    }
    ti->ptr++;
    ti->tokentype = tokenstring;
    ti->token = strlistadd(ti, ti->strstack + stkstart, stkcnt - stkstart);
    return;
  }
  if (*ti->ptr == '\'') {
    start++;
    ti->ptr++;
    while (*ti->ptr != '\'') {
      if (*ti->ptr == '\n') ti->curline++;
      if (*ti->ptr == 0) {
        parsererror(ti, "missing the ending single quote.");
        return;
      }
      ti->ptr++;
    }
    ti->tokentype = tokenstring;
    ti->token = strlistadd(ti, start, ti->ptr++ - start);
    return;
  }
  if ('0' <= *ti->ptr && *ti->ptr <= '9') {
    while (!iswhitespace(*ti->ptr)) {
      ti->ptr++;
    }
    ti->tokentype = tokenstring;
    ti->token = strlistadd(ti, start, ti->ptr - start);
    return;
  }
  if (*ti->ptr == '-' && '0' <= *(ti->ptr + 1) && *(ti->ptr + 1) <= '9') {
    while (!iswhitespace(*ti->ptr)) {
      ti->ptr++;
    }
    ti->tokentype = tokenstring;
    ti->token = strlistadd(ti, start, ti->ptr - start);
    return;
  }
  if (*ti->ptr == '!') {
    if (!iswhitespace(*(ti->ptr + 1))) {
      start++;
      ti->ptr++;
      while (!iswhitespace(*ti->ptr)) {
        ti->ptr++;
      }
      ti->tokentype = tokenfunction;
      ti->token = strlistadd(ti, start, ti->ptr - start);
      return;
    }
    // falling out since this is the unary not.
  }
  while (!iswhitespace(*ti->ptr)) {
    ti->ptr++;
  }
  ti->tokentype = tokenreference;
  ti->token = strlistadd(ti, start, ti->ptr - start);
}

static bool parseexpression(struct rptimpl *ti, int32_t tdparentid);
static void parselist(struct rptimpl *ti);
static void parsetuple(struct rptimpl *ti, bool eofok);

// tdparentid is the value parseexpression should set a tupledef's parentval
// when the stack is empty. should be 1 (empty tuple's value) in list context, 0
// (meaning try to override) in tuple context.
static bool parseexpression(struct rptimpl *ti, int32_t tdparentid) {
  int32_t stksz = 0;
  while (true) {
    if (ti->tokentype == tokeneof) break;
    if (ti->tokentype == tokentupleclose || ti->tokentype == tokenlistclose) {
      break;
    }
    if (ti->tokentype == tokenseparator) {
      break;
    }
    if (ti->tokentype == tokentupleopen) {
      int32_t parentval = 0;
      if (stksz >= 1) {
        parentval = ti->intstack[--ti->intstackcnt];
        stksz--;
      } else {
        parentval = tdparentid;
      }
      nexttoken(ti);
      parsetuple(ti, false);
      stksz++;
      struct value *tupval = &ti->valuelist[ti->intstack[ti->intstackcnt - 1]];
      check(tupval->deftype == valuedeftupledef);
      ti->tupledeflist[tupval->defid].parentval = parentval;
      continue;
    }
    if (ti->tokentype == tokenlistopen) {
      nexttoken(ti);
      parselist(ti);
      stksz++;
      continue;
    }
    // only a string, error, reference or a function's case remain. all of them
    // add a value to the stack.
    int32_t vid = valuelistadd(ti);
    struct value *v = &ti->valuelist[vid];
    v->deffile = ti->curfile;
    v->defline = ti->curline;
    if (ti->tokentype == tokentupleempty) {
      *v = ti->valuelist[cvalemptytup];
      v->deffile = ti->curfile;
      v->defline = ti->curline;
    }
    if (ti->tokentype == tokenlistempty) {
      *v = ti->valuelist[cvalemptylist];
      v->deffile = ti->curfile;
      v->defline = ti->curline;
    }
    if (ti->tokentype == tokenstring) {
      v->type = valuestring;
      v->id = ti->token;
      v->deftype = valuedefstring;
      v->defid = ti->token;
    }
    if (ti->tokentype == tokenerror) {
      v->deftype = valuedeferror;
      v->defid = ti->token;
    }
    if (ti->tokentype == tokenreference) {
      int32_t firstch = ti->strlist[ti->token].ptr[0];
      // check for builtin operators.
      int32_t op = icbtlookup(ti, ti->operatorsicbt, ti->token);
      if (op != 0) {
        // replace operator with its function and fall through to the function
        // case.
        ti->tokentype = tokenfunction;
        ti->token = op;
      } else if (firstch == '.' || firstch == ':') {
        // rewrite "a b .c.d" 'a b .c.d !lookup2".
        if (stksz == 0) {
          parsererror(ti, "reference continuation without argument.");
          return false;
        }
        v->type = valuestring;
        v->id = ti->token;
        v->deftype = valuedefstring;
        v->defid = ti->token;
        intstackadd(ti, vid);
        stksz++;
        vid = valuelistadd(ti);
        v = &ti->valuelist[vid];
        v->deffile = ti->curfile;
        v->defline = ti->curline;
        ti->token = cstrlookup2;
        ti->tokentype = tokenfunction;
        // the function branch below will do the rest.
      } else {
        v->deftype = valuedefreference;
        v->defid = ti->token;
      }
    }
    if (ti->tokentype == tokenfunction) {
      int32_t argc = argcnt(ti, ti->token);
      if (stksz < argc) {
        char *tokenstr = ti->strlist[ti->token].ptr;
        int32_t m = argc - stksz;
        parsererror(ti, "too few arguments for %s (%d missing).", tokenstr, m);
        return false;
      }
      int32_t argsid = intlistadd(ti, argc + 1);
      int32_t *args = ti->intlist + argsid;
      args[0] = ti->token;
      stksz -= argc;
      ti->intstackcnt -= argc;
      for (int32_t i = 1; i <= argc; i++) {
        args[i] = ti->intstack[ti->intstackcnt + i - 1];
      }
      v->deftype = valuedeffunction;
      v->defid = argsid;
    }
    intstackadd(ti, vid);
    stksz++;
    nexttoken(ti);
  }
  if (stksz == 0) return false;
  if (stksz == 1) return true;
  if (stksz >= 2) {
    parsererror(ti, "too many values on stack (%d instead of 1).", stksz);
    return false;
  }
  check(false);
  return false;
}

static void parselist(struct rptimpl *ti) {
  int32_t stksz = 0;
  while (true) {
    while (ti->tokentype == tokenseparator) nexttoken(ti);
    if (parseexpression(ti, 1)) {
      stksz++;
      continue;
    }
    if (ti->tokentype == tokenlistclose) break;
    if (ti->tokentype == tokenseparator) continue;
    parsererror(ti, "list not closed, ] expected.");
    return;
  }
  // eat the closing bracket.
  nexttoken(ti);
  if (stksz == 0) {
    // use the global empty list for empty lists.
    intstackadd(ti, cvalemptylist);
    return;
  }
  int32_t listid = listlistadd(ti);
  struct list *list = &ti->listlist[listid];
  int32_t arraystart = intlistadd(ti, stksz);
  int32_t *array = ti->intlist + arraystart;
  ti->intstackcnt -= stksz;
  for (int32_t i = 0; i < stksz; i++) {
    array[i] = ti->intstack[ti->intstackcnt + i];
  }
  list->first = arraystart;
  list->len = stksz;
  int32_t vid = valuelistadd(ti);
  struct value *v = &ti->valuelist[vid];
  v->deffile = ti->curfile;
  v->defline = ti->curline;
  v->type = valuelist;
  v->id = listid;
  v->deftype = valuedeflist;
  intstackadd(ti, vid);
}

// parses tokens until the closing brace then puts the resulting value onto the
// intstack.
static void parsetuple(struct rptimpl *ti, bool expecteof) {
  int32_t tupdefid = tupledeflistadd(ti);
  struct tupledef *tupdef = &ti->tupledeflist[tupdefid];
  while (ti->tokentype != tokentupleclose) {
    if (ti->tokentype == tokeneof) {
      if (expecteof) break;
      parsererror(ti, "unclosed tuple.");
      return;
    }
    if (ti->tokentype == tokenseparator) {
      nexttoken(ti);
      continue;
    }
    enum tuplefieldmask tagmask = 0;
    if (ti->tokentype == tokenreference) {
      if (ti->token == cstrnoprint) {
        tagmask |= tuplefieldnoprint;
        nexttoken(ti);
      } else if (ti->token == cstrcheck) {
        tagmask |= tuplefieldcheck;
        nexttoken(ti);
      }
    }
    if (ti->tokentype != tokenstring && ti->tokentype != tokenreference) {
      parsererror(ti, "expected a field name for the tuple.");
      return;
    }
    int32_t key = ti->token;
    if (icbtlookup(ti, tupdef->deficbt, key)) {
      parsererror(ti, "duplicate key.");
      return;
    }
    nexttoken(ti);
    if (!parseexpression(ti, 0)) {
      parsererror(ti, "expected an expression.");
      return;
    }
    int32_t valueid = ti->intstack[--ti->intstackcnt];
    icbtadd(ti, &tupdef->deficbt, key, valueid);
    if (tagmask != 0) {
      icbtadd(ti, &tupdef->tagsicbt, key, tagmask);
    }
  }
  if (expecteof && ti->tokentype == tokentupleclose) {
    parsererror(ti, "expected end of file, got }.");
    return;
  }
  if (!expecteof) {
    // consume the closing brace.
    nexttoken(ti);
  }
  int32_t vid = valuelistadd(ti);
  struct value *v = &ti->valuelist[vid];
  v->deffile = ti->curfile;
  v->defline = ti->curline;
  v->deftype = valuedeftupledef;
  v->defid = tupdefid;
  ti->intstack[ti->intstackcnt++] = vid;
}

bool rptparse(struct rpt *t, const char *str) {
  struct rptimpl *ti = t->impl;
  ti->ptr = str;
  ti->curline = 1;
  nexttoken(ti);
  parsetuple(ti, true);
  // add the result to the global.main tuple.
  struct value *resultvalue = &ti->valuelist[ti->intstack[--ti->intstackcnt]];
  struct tupledef *resulttupdef = &ti->tupledeflist[resultvalue->defid];
  struct tupledef *maindef = &ti->tupledeflist[3];
  struct icbtiterator it = {resulttupdef->deficbt, 0, 0};
  while (icbtiterate(ti, &it)) {
    icbtadd(ti, &maindef->deficbt, it.key, it.value);
  }
  it = (struct icbtiterator){resulttupdef->tagsicbt, 0, 0};
  while (icbtiterate(ti, &it)) {
    icbtadd(ti, &maindef->tagsicbt, it.key, it.value);
  }
  return true;
}

bool rptload(struct rpt *t, const char *filename) {
  struct rptimpl *ti = t->impl;
  ti->curfile = strlistadd(ti, filename, -1);
  FILE *f = fopen(filename, "r");
  if (f == NULL) {
    parseerror(ti, "could not load %s.", filename);
    return false;
  }
  check(ti->strstackcnt == 0);
  int32_t bufsz = ti->limit / 2;
  char *buf = ti->strstack;
  ti->strstackcnt = bufsz + 1;
  int32_t sz = fread(buf, 1, bufsz, f);
  if (sz == bufsz) {
    parseerror(ti, "file %s too big.", filename);
    return false;
  }
  check(feof(f) != 0);
  check(ferror(f) == 0);
  check(fclose(f) == 0);
  buf[sz] = 0;
  if (memchr(buf, 0, sz) != NULL) {
    parseerror(ti, "%s contains 0 bytes. rpt does not support that.", filename);
    ti->strstackcnt = 0;
    return false;
  }
  bool result = rptparse(t, buf);
  ti->strstackcnt = 0;
  return result;
}

// fill value's numtype and num fields.
static void tonum(struct rptimpl *ti, int32_t vid) {
  struct value *v = &ti->valuelist[vid];
  if (v->numtype != numtypeunknown) return;
  if (v->type == valueunevaluated) {
    valueeval(ti, vid);
    check(v->type != valueunevaluated);
  }
  if (v->type != valuestring) {
    v->numtype = numtypenotnumber;
    return;
  }
  check(v->id != 0);
  if (ti->strlist[v->id].len == 0) {
    v->numtype = numtypenotnumber;
    return;
  }
  char *ptr = ti->strlist[v->id].ptr;
  v->num = strtoll(ptr, &ptr, 0);
  if (*ptr != 0) {
    v->numtype = numtypenotnumber;
    return;
  }
  v->numtype = numtypenumber;
}

// get a one line, printable representation of a value. it adds the right quotes
// for the strings. the result might point into strstack, make sure to get a
// strlist id before passing the return value any further.
static const char *valuetostr(struct rptimpl *ti, int32_t vid) {
  struct value *v = &ti->valuelist[vid];
  check(v->type != valueunevaluated);
  if (v->type == valuetuple) {
    return "tuple";
  } else if (v->type == valuestring) {
    int32_t strid = ti->valuelist[vid].id;
    char *ptr = ti->strlist[strid].ptr;
    int32_t len = ti->strlist[strid].len;
    // check if string needs double quotes.
    bool needdouble = false;
    bool hasspace = false;
    for (int32_t i = 0; !needdouble && i < len; i++) {
      needdouble = ptr[i] < ' ';
      hasspace |= ptr[i] == ' ';
    }
    if (!needdouble) {
      if (!hasspace) {
        if (len >= 1 && '0' <= *ptr && *ptr <= '9') {
          return ti->strlist[strid].ptr;
        }
        if (len >= 2 && *ptr == '-' && '0' <= *(ptr + 1) && *(ptr + 1) <= '9') {
          return ti->strlist[strid].ptr;
        }
      }
      strstackf(ti, "'%s'", ti->strlist[strid].ptr);
      return ti->strstack + ti->strstackcnt;
    }
    int32_t remlen = ti->limit - ti->strstackcnt;
    char *buf = ti->strstack + ti->strstackcnt;
    *buf = '"';
    remlen--;
    for (int32_t i = 0; i < len; i++) {
      if (remlen <= 8) {
        limiterror(ti, "strstack limit reached.");
        return "error";
      }
      int32_t ch = *ptr++;
      if (ch == '"') {
        *buf++ = '\\';
        *buf++ = '"';
        remlen -= 2;
      } else if (ch == '\n') {
        *buf++ = '\\';
        *buf++ = 'n';
        remlen -= 2;
      } else if (ch == '\t') {
        *buf++ = '\\';
        *buf++ = 't';
        remlen -= 2;
      } else if (ch < 32) {
        *buf++ = '\\';
        *buf++ = '0';
        *buf++ = ch / 8 + '0';
        *buf++ = ch % 8 + '0';
        remlen -= 4;
      } else {
        *buf++ = ch;
        remlen -= 1;
      }
    }
    *buf++ = '"';
    *buf++ = 0;
    return ti->strstack + ti->strstackcnt;
  } else if (v->type == valueerror) {
    return "error";
  } else {
    check(v->type == valuelist);
    return "list";
  }
}

static int32_t valuetostrid(struct rptimpl *ti, int32_t vid) {
  return strlistadd(ti, valuetostr(ti, vid), -1);
}

void printlist(struct rptimpl *ti, int32_t listid, int32_t depth);
void printtuple(struct rptimpl *ti, int32_t tupid, int32_t depth);
void printvalue(struct rptimpl *ti, int32_t vid, int32_t depth);

void printvalue(struct rptimpl *ti, int32_t vid, int32_t depth) {
  valueeval(ti, vid);
  struct value *v = &ti->valuelist[vid];
  if (v->beingprinted) {
    int32_t stackstart = ti->strstackcnt;
    char *ctxbuf = ti->strstack + stackstart;
    addcontext(ti, vid);
    *strstackadd(ti, 1) = 0;
    printf("... (cyclic reference to %s)\n", ctxbuf);
    ti->strstackcnt = stackstart;
    return;
  }
  v->beingprinted = true;
  if (v->type == valuetuple) {
    puts("{");
    printtuple(ti, v->id, depth + 1);
    for (int32_t d = 0; d < 2 * depth; d++) putchar(' ');
    puts("}");
  } else if (v->type == valuelist) {
    puts("[");
    printlist(ti, v->id, depth + 1);
    for (int32_t d = 0; d < 2 * depth; d++) putchar(' ');
    puts("]");
  } else if (v->type == valueerror) {
    printf("`%s`\n", ti->strlist[v->id].ptr);
  } else {
    puts(valuetostr(ti, vid));
  }
  v->beingprinted = false;
}

void printlist(struct rptimpl *ti, int32_t listid, int32_t depth) {
  int32_t len = ti->listlist[listid].len;
  int32_t *vids = ti->intlist + ti->listlist[listid].first;
  for (int32_t i = 0; i < len; i++) {
    for (int32_t d = 0; d < 2 * depth; d++) putchar(' ');
    printvalue(ti, vids[i], depth);
  }
}

// musl does not support qsort_r. use global variables in that case to pass down
// the rptimpl pointer. however only do this when building the rpt tool because
// that is single threaded. only allow the reentrant version when built as a
// library.
#ifdef buildthetool
struct rptimpl *strlistidcmpti;
static int strlistidcmp(const void *aa, const void *bb) {
  struct rptimpl *ti = strlistidcmpti;
#else
static int strlistidcmp(const void *aa, const void *bb, void *titi) {
  struct rptimpl *ti = titi;
#endif
  int32_t a = *(const int32_t *)aa;
  int32_t b = *(const int32_t *)bb;
  return strcmp(ti->strlist[a].ptr, ti->strlist[b].ptr);
}

// print the fields in lexicographical order.
void printtuple(struct rptimpl *ti, int32_t tupid, int32_t depth) {
  struct tuple *tup = &ti->tuplelist[tupid];
  int32_t stkbegin = ti->intstackcnt;
  struct icbtiterator it = {tup->fieldsicbt, 0, 0};
  while (icbtiterate(ti, &it)) {
    intstackadd(ti, it.key);
    intstackadd(ti, it.value);
  }
  int32_t cnt = (ti->intstackcnt - stkbegin) / 2;
  int32_t sz = 2 * sizeof(ti->intstack[0]);
#ifdef buildthetool
  strlistidcmpti = ti;
  qsort(ti->intstack + stkbegin, cnt, sz, strlistidcmp);
#else
  qsort_r(ti->intstack + stkbegin, cnt, sz, strlistidcmp, ti);
#endif
  for (int32_t i = stkbegin; i < ti->intstackcnt; i += 2) {
    int32_t key = ti->intstack[i];
    int32_t value = ti->intstack[i + 1];
    check(value != 0);
    enum tuplefieldmask tags = icbtlookup(ti, tup->tagsicbt, key);
    if ((tags & (tuplefieldnoprint | tuplefieldcheck)) != 0) continue;
    for (int32_t d = 0; d < 2 * depth; d++) putchar(' ');
    // todo: escape if needed.
    printf("%s ", ti->strlist[key].ptr);
    printvalue(ti, value, depth);
  }
  ti->intstackcnt = stkbegin;
}

bool rpteval(struct rpt *t, const char *exprstr) {
  check(exprstr[0] == 0);
  struct rptimpl *ti = t->impl;
  ti->hasevalenv = true;
  if (setjmp(ti->evalenv) == 1) {
    return false;
  }
  ti->token = 0;
  valueeval(ti, cvalmaintup);
  check(ti->valuelist[cvalmaintup].type == valuetuple);
  puts("{");
  printtuple(ti, ti->valuelist[cvalmaintup].id, 1);
  puts("}");
  ti->hasevalenv = false;
  return true;
}

struct fnargs {
  // filled by the function.
  int32_t vid;
  int32_t minargc, maxargc;

  // filled by getfnargs.
  struct value *v;
  int32_t argc;
  int32_t *args;
};
// returns true on success, returns false on error, e.g. invalid number of args.
// the value will contain the error.
static bool getfnargs(struct rptimpl *ti, struct fnargs *fnargs) {
  fnargs->v = &ti->valuelist[fnargs->vid];
  check(fnargs->v->deftype == valuedeffunction);
  fnargs->argc = argcnt(ti, ti->intlist[fnargs->v->defid]);
  fnargs->args = ti->intlist + fnargs->v->defid + 1;
  int32_t got = fnargs->argc;
  int32_t mn = fnargs->minargc;
  int32_t mx = fnargs->maxargc;
  if (got < mn || mx < got) {
    const char fmt[] = "invalid number of arguments (%d vs [%d,%d]).";
    valerrorf(ti, fnargs->vid, fmt, got, mn, mx);
    return false;
  }
  return true;
}
// "make error if value is error". if arg is error, copy it to vid and return
// true.
static bool merrval(struct rptimpl *ti, struct fnargs *fnargs, int32_t arg) {
  check(ti->valuelist[fnargs->args[arg]].type != valueunevaluated);
  if (ti->valuelist[fnargs->args[arg]].type == valueerror) {
    copyresult(ti, fnargs->vid, fnargs->args[arg]);
    return true;
  }
  return false;
}
// "make error if value is not a number". if arg is not a number, make vid into
// an error and return true.
static bool merrnotnum(struct rptimpl *ti, struct fnargs *fnargs, int32_t arg) {
  int32_t argvid = fnargs->args[arg];
  tonum(ti, argvid);
  if (ti->valuelist[argvid].numtype != numtypenumber) {
    const char *avstr = ti->strlist[valuetostrid(ti, argvid)].ptr;
    int32_t vid = fnargs->vid;
    valerrorf(ti, vid, "%s for arg %d is not a number.", avstr, arg + 1);
    return true;
  }
  return false;
}

static void bltnnumadd(struct rptimpl *ti, int32_t vid) {
  struct fnargs fnargs;
  fnargs.vid = vid;
  fnargs.minargc = 2;
  fnargs.maxargc = 9;
  if (!getfnargs(ti, &fnargs)) return;
  int64_t acc = 0;
  for (int32_t i = 0; i < fnargs.argc; i++) {
    valueeval(ti, fnargs.args[i]);
    if (merrval(ti, &fnargs, i) || merrnotnum(ti, &fnargs, i)) return;
    if (__builtin_add_overflow(acc, ti->valuelist[fnargs.args[i]].num, &acc)) {
      valerrorf(ti, vid, "result overflowed.");
      return;
    }
  }
  fnargs.v->type = valuestring;
  fnargs.v->id = strlistf(ti, "%lld", (long long)acc);
  fnargs.v->numtype = numtypenumber;
  fnargs.v->num = acc;
}

static void bltnnummul(struct rptimpl *ti, int32_t vid) {
  struct fnargs fnargs;
  fnargs.vid = vid;
  fnargs.minargc = 2;
  fnargs.maxargc = 9;
  if (!getfnargs(ti, &fnargs)) return;
  int64_t acc = 1;
  for (int32_t i = 0; i < fnargs.argc; i++) {
    valueeval(ti, fnargs.args[i]);
    if (merrval(ti, &fnargs, i) || merrnotnum(ti, &fnargs, i)) return;
    if (__builtin_mul_overflow(acc, ti->valuelist[fnargs.args[i]].num, &acc)) {
      valerrorf(ti, vid, "result overflowed.");
      return;
    }
  }
  fnargs.v->type = valuestring;
  fnargs.v->id = strlistf(ti, "%lld", (long long)acc);
  fnargs.v->numtype = numtypenumber;
  fnargs.v->num = acc;
}

static void bltnnumsub(struct rptimpl *ti, int32_t vid) {
  struct fnargs fnargs;
  fnargs.vid = vid;
  fnargs.minargc = 2;
  fnargs.maxargc = 2;
  if (!getfnargs(ti, &fnargs)) return;
  valueeval(ti, fnargs.args[0]);
  if (merrval(ti, &fnargs, 0) || merrnotnum(ti, &fnargs, 0)) return;
  valueeval(ti, fnargs.args[1]);
  if (merrval(ti, &fnargs, 1) || merrnotnum(ti, &fnargs, 1)) return;
  int64_t v1 = ti->valuelist[fnargs.args[0]].num;
  int64_t v2 = ti->valuelist[fnargs.args[1]].num;
  int64_t r;
  if (__builtin_sub_overflow(v1, v2, &r)) {
    valerrorf(ti, vid, "result overflowed.");
    return;
  }
  fnargs.v->type = valuestring;
  fnargs.v->id = strlistf(ti, "%lld", (long long)r);
  fnargs.v->numtype = numtypenumber;
  fnargs.v->num = r;
}

static void bltnnumdiv(struct rptimpl *ti, int32_t vid) {
  struct fnargs fnargs;
  fnargs.vid = vid;
  fnargs.minargc = 2;
  fnargs.maxargc = 2;
  if (!getfnargs(ti, &fnargs)) return;
  valueeval(ti, fnargs.args[0]);
  if (merrval(ti, &fnargs, 0) || merrnotnum(ti, &fnargs, 0)) return;
  valueeval(ti, fnargs.args[1]);
  if (merrval(ti, &fnargs, 1) || merrnotnum(ti, &fnargs, 1)) return;
  int64_t v1 = ti->valuelist[fnargs.args[0]].num;
  int64_t v2 = ti->valuelist[fnargs.args[1]].num;
  if (v1 == -9223372036854775807LL - 1 && v2 == -1) {
    valerrorf(ti, vid, "result overflowed.");
    return;
  }
  if (v2 == 0) {
    valerrorf(ti, vid, "division by zero.");
    return;
  }
  int64_t r = v1 / v2;
  fnargs.v->type = valuestring;
  fnargs.v->id = strlistf(ti, "%lld", (long long)r);
  fnargs.v->numtype = numtypenumber;
  fnargs.v->num = r;
}

static void bltnnummod(struct rptimpl *ti, int32_t vid) {
  struct fnargs fnargs;
  fnargs.vid = vid;
  fnargs.minargc = 2;
  fnargs.maxargc = 2;
  if (!getfnargs(ti, &fnargs)) return;
  valueeval(ti, fnargs.args[0]);
  if (merrval(ti, &fnargs, 0) || merrnotnum(ti, &fnargs, 0)) return;
  valueeval(ti, fnargs.args[1]);
  if (merrval(ti, &fnargs, 1) || merrnotnum(ti, &fnargs, 1)) return;
  int64_t v1 = ti->valuelist[fnargs.args[0]].num;
  int64_t v2 = ti->valuelist[fnargs.args[1]].num;
  if (v2 == 0) {
    valerrorf(ti, vid, "division by zero.");
    return;
  }
  int64_t r = v1 % v2;
  fnargs.v->type = valuestring;
  fnargs.v->id = strlistf(ti, "%lld", (long long)r);
  fnargs.v->numtype = numtypenumber;
  fnargs.v->num = r;
}

static void bltnnumless(struct rptimpl *ti, int32_t vid) {
  struct fnargs fnargs;
  fnargs.vid = vid;
  fnargs.minargc = 2;
  fnargs.maxargc = 2;
  if (!getfnargs(ti, &fnargs)) return;
  valueeval(ti, fnargs.args[0]);
  if (merrval(ti, &fnargs, 0) || merrnotnum(ti, &fnargs, 0)) return;
  valueeval(ti, fnargs.args[1]);
  if (merrval(ti, &fnargs, 1) || merrnotnum(ti, &fnargs, 1)) return;
  struct value *arg1v = &ti->valuelist[fnargs.args[0]];
  struct value *arg2v = &ti->valuelist[fnargs.args[1]];
  fnargs.v->type = valuestring;
  fnargs.v->num = arg1v->num < arg2v->num;
  fnargs.v->numtype = numtypenumber;
  fnargs.v->id = strlistf(ti, "%lld", (long long)fnargs.v->num);
}

static void bltncond(struct rptimpl *ti, int32_t vid) {
  struct fnargs fnargs;
  fnargs.vid = vid;
  fnargs.minargc = 3;
  fnargs.maxargc = 3;
  if (!getfnargs(ti, &fnargs)) return;
  valueeval(ti, fnargs.args[0]);
  if (merrval(ti, &fnargs, 0) || merrnotnum(ti, &fnargs, 0)) return;
  int64_t cond = ti->valuelist[fnargs.args[0]].num;
  if (cond < 0 || 2 <= cond) {
    valerrorf(ti, vid, "cond must be 0 or 1. it is %lld.", (long long)cond);
    return;
  }
  valueeval(ti, fnargs.args[2 - cond]);
  copyresult(ti, vid, fnargs.args[2 - cond]);
}

static void bltnlookup(struct rptimpl *ti, int32_t vid) {
  struct fnargs fnargs;
  fnargs.vid = vid;
  fnargs.minargc = 2;
  fnargs.maxargc = 2;
  if (!getfnargs(ti, &fnargs)) return;
  valueeval(ti, fnargs.args[0]);
  if (merrval(ti, &fnargs, 0)) return;
  valueeval(ti, fnargs.args[1]);
  if (merrval(ti, &fnargs, 1)) return;
  struct value *arg1 = &ti->valuelist[fnargs.args[0]];
  struct value *arg2 = &ti->valuelist[fnargs.args[1]];
  struct derefdata data;
  data.dokeywords = false;
  data.errorvid = vid;
  if (arg1->type == valuestring) {
    data.startvid = ti->valuelist[vid].context;
    data.ref = arg1->id;
    if (!deref(ti, &data)) return;
    data.startvid = data.resultvid;
  } else {
    data.startvid = fnargs.args[0];
  }
  if (arg2->type != valuestring) {
    const char *typestr = ti->strlist[valuetostrid(ti, fnargs.args[1])].ptr;
    valerrorf(ti, vid, "arg2 must be string, got %s.", typestr);
    return;
  }
  data.ref = arg2->id;
  if (!deref(ti, &data)) return;
  valueeval(ti, data.resultvid);
  copyresult(ti, vid, data.resultvid);
}

static void bltnsysload(struct rptimpl *ti, int32_t vid) {
  struct fnargs fnargs;
  fnargs.vid = vid;
  fnargs.minargc = 1;
  fnargs.maxargc = 1;
  if (!getfnargs(ti, &fnargs)) return;
  valueeval(ti, fnargs.args[0]);
  if (merrval(ti, &fnargs, 0)) return;
  struct value *filev = &ti->valuelist[fnargs.args[0]];
  if (filev->type != valuestring) {
    const char *typestr = ti->strlist[valuetostrid(ti, fnargs.args[0])].ptr;
    valerrorf(ti, vid, "sysload expected string, got %s.", typestr);
    return;
  }
  char *filename = ti->strlist[filev->id].ptr;
  // todo: do directory traversal.
  char fullpath[PATH_MAX];
  if (realpath(filename, fullpath) == NULL) {
    valerrorf(ti, vid, "could not resolve %s: %m.", filename);
    return;
  }
  int32_t fullnameid = strlistadd(ti, fullpath, -1);
  int32_t fsid = icbtlookup(ti, ti->sysloadicbt, fullnameid);
  if (fsid != 0) {
    fnargs.v->type = valuestring;
    fnargs.v->id = fsid;
    return;
  }
  FILE *f = fopen(fullpath, "r");
  if (f == NULL) {
    valerrorf(ti, vid, "could not open %s: %m.", filename);
    return;
  }
  char *buf = ti->strstack + ti->strstackcnt;
  int32_t remlen = ti->limit - ti->strstackcnt;
  int32_t rby = fread(buf, 1, remlen, f);
  if (rby >= remlen) {
    limiterror(ti, "strstack limit reached.");
    return;
  }
  if (ferror(f) != 0 || feof(f) == 0) {
    valerrorf(ti, vid, "could not load %s: %m.", filename);
    goto done;
  }
  fnargs.v->type = valuestring;
  fnargs.v->id = strlistadd(ti, buf, rby);
  icbtadd(ti, &ti->sysloadicbt, fullnameid, fnargs.v->id);
done:
  check(fclose(f) == 0);
}

// rpt tool and debugging related functions below.
#ifdef buildthetool

void dumptuplefieldsicbt(struct rpt *t, int64_t icbt) {
  struct icbtiterator it = {icbt, 0, 0};
  while (icbtiterate(t->impl, &it)) {
    printf("%d (%s) -> %d\n", it.key, t->impl->strlist[it.key].ptr, it.value);
  }
}

void printstat(const char *name, int32_t value, int32_t limit) {
  printf("%15s: %9d (%3lld%%)\n", name, value, value * 100LL / limit);
}

int main(int argc, char **argv) {
  struct rpt t;
  memset(&t, 0, sizeof(t));
  bool printstats = false;
  int opt;
  while ((opt = getopt(argc, argv, "bls")) != -1) {
    switch (opt) {
      case 'b':
        t.breaklimits = true;
        break;
      case 'l':
        t.disablelineno = true;
        break;
      case 's':
        printstats = true;
        break;
      default:
        exit(1);
        break;
    }
  }
  argc -= optind;
  argv += optind;
  if (argc <= 1) {
    puts("usage: rpt eval filename");
    puts("usage: rpt print filename expr");
    exit(1);
  }
  rptinit(&t);
  if (rptload(&t, argv[1])) {
    if (strcmp(argv[0], "eval") == 0) {
      if (argc != 2) {
        puts("usage: rpt eval filename");
        exit(1);
      }
      if (!rpteval(&t, "")) {
        printf("eval error: %s\n", t.error);
      }
    } else if (strcmp(argv[0], "print") == 0) {
      if (argc != 3) {
        puts("usage: rpt print filename expr");
        exit(1);
      }
      struct rptimpl *ti = t.impl;
      ti->ptr = argv[2];
      ti->curfile = strlistf(ti, "cmdlineexpr");
      ti->curline = 1;
      nexttoken(ti);
      check(parseexpression(ti, ctupmain));
      int32_t vid = ti->intstack[--ti->intstackcnt];
      vid = copyvalue(ti, vid, cvalmaintup);
      ti->valuelist[vid].context = cvalmaintup;
      valueeval(ti, cvalmaintup);
      valueeval(ti, vid);
      struct value *v = &ti->valuelist[vid];
      if (v->type != valuestring && v->type != valueerror) {
        puts("result not a string.");
      } else {
        puts(valuetostr(ti, v->id));
      }
    } else {
      puts("not a supported command.");
      return 1;
    }
  } else {
    printf("load error: %s\n", t.error);
  }
  if (printstats) {
    struct rptimpl *ti = t.impl;
    printf("stats:\n");
    printstat("strlistcnt", ti->strlistcnt, ti->limit);
    printstat("strcbnlistcnt", ti->strcbnlistcnt, ti->limit);
    printstat("intcbnlistcnt", ti->intcbnlistcnt, ti->limit);
    printstat("intlistcnt", ti->intlistcnt, ti->limit);
    printstat("valuelistcnt", ti->valuelistcnt, ti->limit);
    printstat("tuplelistcnt", ti->tuplelistcnt, ti->limit);
    printstat("tupledeflistcnt", ti->tupledeflistcnt, ti->limit);
    printstat("listlistcnt", ti->listlistcnt, ti->limit);
  }
  rptfree(&t);
  return 0;
}
#endif  // buildthetool
