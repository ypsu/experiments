note: i started hacking on this but as with any other project, i long abandoned
it after i got the basics down. i found a nicer language that can do what i
wanted from this one in a much simpler form. i'll describe it later. but in the
meantime i do not want to throw out the codebase from here so i'll let this
place be the grave for this utility. don't bother looking at it, nothing will
make sense since i haven't cleaned up the readme nor the code, it's full of
random unfinished and unconnected thoughts and comments.

# rptlang


## intro

rpt stands for "reverse polish tuples". pronounce it as "repot". there was a
time in my life where i was working with massive configuration files in a very
expressivem declarative, data tuple templating language. i quite liked the ideas
behind the language. however the language itself was quite complex with many
gotchas so i thought i create my own as an exercise. i aimed to do it in a way
that is the most simple to implement (so i do not give up halfway) but still is
similarly powerful. look at the flabbergast language or jsonnet to see more
mature languages based on the one i am referring to. this one is a toy language
but on the other hand is very small and easy to embed in any situation if one
can get past its weird syntax and strictness.

this language is aimed for configuration:


```
server {
  hostname 'example.com'
  files [
    'index.html'
    'about.html'
    'contact.html'
  ]
  port 80
  queuesize 100
  memorysize 100M
}
```

todo: per above:

```
template server_tmpl {
  # noprint means that the value will not be printed, less output spam.
  noprint params {
    # by default set qps to an error (`` means an error string) so that the
    # inheritors do not forget to override this.
    qps `set expected qps`
  }
  hostname 'example.com'
  files [
    'index.html'
    'about.html'
    'contact.html'
  ]
  port 80
  queuesize qps 100 *
  memorysize qps 1M *
}

myblog server_tmpl {
  noprint params {
    qps 100
  }
}
```

the catch is that the values can be expressions written in rpn, reverse polish
notation. it is an alternative representation to the infix notation. the
advantage of it that it does not require parens to describe an expression. think
of it as a stack based evaluation. in "1 2 +" the evaluator puts 1 and 2 on to
the stack and when it gets to "+" it pops the two values and puts back 3. this
means parsing and evaluating an expression is just simply iterating through the
tokens. it is a bit harder to read especially because everyone is so used to the
infix notation but as an upside it motivates writing simpler expressions which
are then easier to debug. i chose rpn instead of the ordinary infix notation
because it is easier to implement, makes people think harder to avoid writing
overlong expressions, and i was just curious how programming in rpn feels like.
and to keep things simple.

```
infix: a + b
  rpn: a b +

infix: a + b * c
  rpn: a b c * +

infix: (a + b) * 2 + 3 * 4
  rpn: a b + 2 * 3 4 * +

infix: exp(sin(a), 2) + exp(cos(b), 2)
  rpn: a sin 2 exp b cos 2 exp +
```

the only caveat when reading rpn expressions is that the reader needs to know
which tokens are functions and how many arguments the functions take.
conventions help in this case. all operators except ! and ~ (unary not and unary
binary bitwise not) take two arguments. functions start with ! (in which case it
is not the unary not) and end in a digit that represents the number of arguments
they require. a function can take at most 9 arguments. this form of rpn looks
like this:

```
infix: exp(sin(a), 2) + exp(cos(b), 2)
  rpn: a !sin1 2 !exp2 b !cos1 2 !exp2 +
```

rpt supports the operators +, -, *, /, %, &, |, ^, <, <=, ==, >, >=, <<, >>, ~,
! and these mean the same thing as in c. since rpt has strings and lists it also
supports . for string concatenation and : for list concatenation.

```
server {
  hostname 'exam' 'ple.com' .
  _defaultfiles [
    'index.html'
  ]
  _myfiles [
    'about.html'
    'contact.html'
  ]
  files _defaultfiles _myfiles :
  port 30 50 +
```

it also allows importing other config files and inheriting from other tuples:

```
# servertmpl.rpt
servertmpl {
  hostname 'missing hostname for the server' !error1
  files [
    'index.html'
  ]
  port 80
}
# server.rpt
import serverlib 'servertmpl.rpt'
server serverlib.servertmpl {
  hostname 'example.com'
  _myfiles [
    'about.html'
    'contact.html'
  ]
  # files on the right hand side refers to the one from servertmpl.
  files super.files _myfiles .
  # port is inherited.
}
# the cmdline tool would print the following:
$ rpt eval server.rpt
serverlib {
  servertmpl {
    hostname 'missing hostname for the server' !error1
    files [
      'index.html'
    ]
    port 80
  }
}
server {
  hostname 'example.com'
  files [
    'index.html'
    'about.html'
    'contact.html'
  ]
  port 80
}
# to just get the port number, use this:
$ rpt print server.rpt server.port
80
```

it even allows writing custom functions for those very complicated configs.
functions are just special tuples with special fields. field `result` is the
result, `arg1`, `arg2`..., `arg9` contain the arguments. the last digit of the
function contains the number of arguments it receives. rpt evalutes the
function tuple in the invocation's context.

```
# server.rpt
server {
  files [
    'index.html'
    'about.html'
  ]
  root env.HOME
  paths prependroot1 files !map2
}
prependroot {
  filename arg1
  result root filename !pathjoin2
}
# on the cmdline
$ PATH=/home/user rpt eval server.rpt
prependroot1 {
  filename 'error: arg1 not found' !error1
  result 'error: arg1 not found' !error1
}
server {
  files [
    'index.html'
    'about.html'
  ]
  paths [
    '/home/user/index.html'
    '/home/user/about.html'
  ]
  root '/home/user'
}
```


## description

a rpt file is a tuple itself. a tuple consists of a list of key value pairs,
each on a separate line. the key is the first token on the line. it can be an
arbitrary string however if you want to have spaces or anything in it, surround
the key with quotes. always use a single quotes. a value is one of the four
types: string, tuple, list, error. note that there is no number there. however
if a string looks like a number then the numeric operations will convert back
and forth completely transparently. treating numbers as strings has a few
convenience aspects, it allows many ways to represent the same number. see the
numbers section below to see the various ways.

however a raw rpt file has "expressions" rather than raw values on the right
side. the rpt evaluator converts them to values. the evaluator is lazy, it only
evaluates the expressions when needed, e.g. when printing the field or when a
field's value is needed in a calculation. an expression is just a single space
separated list of tokens that ends in a newline.

if the last token is `{` then the expression must be empty or must evaluate to a
token. `{` sets the expression's resulting type to tuple. if there is an
expression on the '{' line, it must evaluate to a tuple since rpt will use that
tuple as the base tuple for the new tuple. this inheritance means that when an
expression refers to a field not present in the current tuple, rpt will look up
the field in the parent tuple and keep going up the parental chain until it
finds the field. if the expression was empty but the parent tuple already has
the expression defined, rpt uses that as the base tuple.

the other special expression is the list expressions. this ends in a `[` token.
the following lines are evaluated specially. they are no longer in "key value"
format only in "value" format. each line represents one entry in the list. as a
convenience if the line is an ordinary expression (does not end in `{` or `[`)
it allows specifying multiple values on the line. you can still have arithmetic
and whatnot expressions though. the rpn allows ending with multiple values on
the stack, the evaluator inserts them all rather than resulting in an error if
the stack size is not one. although lists are not tuples, think of them as
tuples where the keys are _0, _1, ... _(n-1).

on a '[' line rpt first evaluates the list, puts the resulting list onto the
rpn stack and then evaluates rest of the expression:

```
# result: 1 2 3 !list3
nums [
  1 2
  3
]
# result: 6
nums !sum1 [
  1 2
  3
]
# result: 'hello cruel world!'
greeting ' ' !listjoin2 [
  'hello'
  'cruel'
  'world!'
]
```

in an expression each token can be one of these things:

- string constant
- reference to another field
- function call

if a string starts with a number then rpt treats it as a string. otherwise the
string constant must start with `'` (single quote) and end in it. use percent
encoding to put other values into the string:

```
$ rpt print /dev/null "'use %2525 to print %22%27%22.%0aor %250a for newlines.'"
use %25 to print "'"
or %0a for newlines.
```

rpt automatically parses numbers in number contexts. the numbers are 64 bit
signed integers. rpt does not support floating point numbers. the result of an
operation will be a unitless number. use tobytes1, toduration1, tometric1
functions to convert the numbers back to the desired units. time duration is
represented in milliseconds. use tonum function to get the value from a number.
see the numbers section for detailed explanation on how rpt parses numbers.
quick example:

```
$ rpt print /dev/null '1m2s !tonum1'
602000
$ rpt print /dev/null '602000 !toduration1'
1m2s
```

as a convenience rpt also treats constant starting with % as a string:

```
# foo.rpt
fib [
  1 1 2 3 5
]
# bash
$ rpt print foo.rpt 'fib %0a !listjoin1'
1
1
2
3
5
```

the simplest references have the syntax of `fielda.fieldb.[...].fieldz`. rpt
will look for fielda in the current tuple. if it could not find a definition for
it, it will look for a definition in the tuple the current tuple inherited from.
if it could not find a definition there, it will look for a definition in the
enclosing tuples. once it found a definition, it will use that as the context
for resolving the remaining fields. however for the remaining lookups rpt will
not traverse upwards the enclosing tuples with the dot separator. that is what
the `:`, semicolon separator is for. in `fielda.fieldb:fieldc.fieldd` rpt looks
up fielda, looks up fieldb and then do a full lookup for fieldc starting from
fielda.fieldb. rpt will look up fieldd in fieldc again. this should be seldom
needed though. the references cannot contain special characters. to look up
keys, say, with spaces use the lookup function. see the function reference for
an example.

there are a couple of special values for fielda: this, super, up, file, global.
all of these can only be specified as the first field. this refers to the
current tuple. this is useful for passing the current tuple to a function or
look up a key named, say, global. writing `this.global` circumvent the special
case of looking up the global tuple. super skips looking up the reference in the
current context. it is useful when "adjusting" a value. e.g. `path '/home/'
super.path !pathjoin2`. this will override the path field via adjusting the
value from its parent. up goes one context higher, it goes to the enclosing
tuple. suppose there is a path field in both the current, the parent and the
enclosing tuple. to get to the enclosing tuple's field, use `up.path` or
`up:path`. there is one special quirk to the up references. unlike the rest, rpt
allows chaining up references at the front. `up.up.up.path` will go 3 tuples
higher and look for path there. however `this.up` will still try to look up the
`up` field in the current context. `global` refers to the global tuple, `file`
refers to the local file's tuple.

todo: make it possible .foo.bar to "continue" a lookup from stack. the parser
need special support for this. for lists support number lookups. e.g.
foo.bar.3.baz.qux. this is handy for handling pairs and triplets.

```
# sample.rpt
foo {
  a 1
  b 2
  t {
    c 3
    a 4
  }
}
# cmdline
$ rpt print sample.rpt foo.a
1
$ rpt print sample.rpt foo.t.a
4
$ rpt print sample.rpt foo.b
2
$ rpt print sample.rpt foo.t.b
error: foo.t.b not found
# the : will make rpt do full reference lookup for the next field. this should
# be used very sparingly though.
$ rpt print sample.rpt foo.t:b
2
```

when rpt loads a file, it places its content into a field name `main` in the
global tuple. this way it is easy to refer to the file's full tuple. there a
couple of other fields in the global tuple. it contains the tuples for the
builtin functions:

```
$ rpt print opadd3
{
  result arg1 arg2 arg3 !opadd3
}
$ rpt print global.opadd3
{
  result arg1 arg2 arg3 !opadd3
}
```

for now just ignore the fact that this definition is recursive (rpt looks at
builtins at the function invocation time so !opadd3 is indeed a special magic
but opadd3 is just a tuple). the aim of this is to make it easy to pass the
builtin functions to function calls that expect function tuples. e.g. you can
write "tounits1 mynums !map2" to convert all numbers to the unit format. however
something like `file` is easy overridden in a tuple so to access that field use
an absolute reference: `...file`. this one should be used sparingly too.

another field in the global tuple is the `files` tuple. it is a tuple keyed by
filenames that map to the tuples from all the imported files. this field should
not be used but rpt uses this as an implementation clutch to implement field
local references. here is an example to demonstrate the problem:

```
# base.rpt
a 1
b a
# child.rpt
import baselib as baselib
c baselib.b
# cmdline
$ rpt print child.rpt c
error at child.rpt:2. a not found.
```

the problem here is that rpt tries to evaluate `baselib.b` in child's context
where `a` does not exist. the problem is similar to this:

```
foo {
  a
  bar {
    b a
  }
}
baz {
  qux bar
}
```

here `qux.bar.b` is an error because rpt cannot resolve `a`. `baz` should
either inherit from `foo` or `bar` should refer to `foo.a`. however neither of
these options are good match for the former example. if base's `b` intends to
refer base's `a` then it must do so file local references. instead of `b a` the
definition should be `b ...a`. `..` is a shorthand for `....files
<filename_for_the_current_line> !lookup2`.

another field in the global tuple is the env tuple. it contains the environment
variables passed to the rpt tool.

and there is the vars field in the global tuple. use this to pass in some
arguments via the command line. the rpt tool expects a list of "key1 value1 key2
value2 [...]" argument to set the vars up. vars cannot contain expressions,
lists or tuples, just strings to keep things simple. since the list is space
separated, use percent encoding to have spaces there. e.g. `rpt -v "greeting
hello%20world"`.

```
# sample.rpt
defaultvars {
  hostname 'missing: hostname needed' error1!
  port 8080
}
vars defaultvars global.vars !override2
server {
  hostname vars.hostname
  port vars.port
}
# cmdline
# rpt eval sample.rpt -v 'port 80 hostname example.com' server
{
  hostname 'example.com'
  port 80
}
```

rpt handles fields that start with _, underscore specially. only the current
tuple can access those. also the print command does not print those fields
either. this is a convenience functionality for private fields.

```
# sample.rpt
a 1
_b 2
c 3
# cmdline
$ rpt eval sample.rpt
{
  a 1
  c 3
}
```

at the beginning of the file rpt allows for import and load statements. note
that these cannot appear after the first field definition. both statements
require 2 constant string parameters. do not use quotes here, these are parsed
specially. this ensures that the list of dependencies are right at the top of
the file. import loads another .rpt file as a tuple named as the second
argument, load loads the full contents of another file into a string constant.
as a convention end the import fields name in lib, data fields in data.

```
# funcs.rpt
# uptimestring touptime1: parses the uptimestring and converts it to duration.
# e.g. '123.456 123.456' -> '1m'
touptime1 {
  result arg1 '\..*' '' !replace3 1000 / !toduration1
}
# uptime.rpt
import funcslib funcs.rpt
load uptimedata /proc/uptime
uptime uptimedata !funcslib.touptime1
# cmdline
$ rpt print uptime.rpt uptime
2m3s
# list all dependencies of a file:
$ rpt print uptime.rpt 'global.files !keys1 %0a !listjoin2'
/proc/uptime
funcs.rpt
uptime.rpt
```

rpt does not have the concept of "import paths". all imports are either absolute
paths or relative. rpt resolves relative paths similarly to how it resolves
references. it goes upwards for the first path component. once rpt finds it, rpt
looks for the rest of the path from that directory. assume the following files:

```
1: rpt/common/strings.rpt
2: rpt/common/utils.rpt
3: rpt/myproject/common/utils.rpt
4: rpt/myproject/config.rpt
```

then importing the following files in `rpt/myproject/config.rpt`:

- `common/utils.rpt` imports 3.
- `rpt/common/utils.rpt` imports 2
- `common/strings.rpt` is an error
- `rpt/common/strings.rpt` imports 1

expressions are just list of tokens which when evaluated resolve to one value on
the rpn stack. functions pop items from the stack and put back a single value.
functions call starts with a ! and is followed by a reference. the reference
must end in a field that has a digits as its last character. rpt uses that digit
to know how much parameters a function takes. function calls are just syntactic
sugar for tuple instantiations. the only requirement is that the tuple must have
a `result` field. the + and other operators are also tuples. + is a shorthand
for !opadd2.

todo: describe checks. they get evaluated during the first reference to a tuple.
"nochecks" statement disables all checks in the inner tuples but it is not
inherited. it can be used to implement templates.

todo: this needs more thought. if a check fails, the tuple's value should be an
error. if it is in an alt, alt will just take the next alternative. there
should not be fatal errors. on a second thought no: if someone wishes to disable
a tuple's checks, they should instantiate it in a nocheck tuple.

todo: remove the "action" bit. and rename "check" to "assert". it should have no
effect on the evaluation though other than the tool prints it to stderr and
returns non-zero if it was triggered.

todo: s/nocheck/template/

```
foo {
  check ramwarn {
    cond ram 12G <
    action 'near the ram limit' !log1
  }
  check ramerror {
    cond ram 16G <
    action 'exceeded the ram limit' !abort1
  }
  ram 12G
}
bar foo {
  # disable the annoying warning by making it always succeed.
  check ramwarn {
    cond 1
  }
}
```

so with assertions i would have this:

```
assert [optional_name] [optional_msg] condition

foo {
  assert ramwarn 'near the ram limit!' ram 12G <
  # this assert does not have a name, cannot be overriden for disable.
  assert 'exceeded the ram limit!' ram 16G <
  ram 12G
}
bar {
  # disable the smaller assert.
  assert ramwarn 'fix coming at b/1234 for the ram problem.' 1
}
```

todo: remove the nochecks feature, just treat objects starting with _ as
templates.


## numbers

rpt can use values starting with a number in arithmetic operations. all numbers
are 64 bit signed integers. rpt ignores underscores in numbers so you can use it
to make it easier to read large numbers. but note that the numbers still have to
start with digits. rpt allows numbers to have suffixes to make it easier writing
large numbers. for instance rpt will parse `1Mi` as 1_048_576. or `1s` parses as
1000 since rpt treats duration in ms. there can be even multiple suffixes, rpt
just adds up the individual pieces. `1m2s` translates to 62000. underscores are
ignored in perl style fashion.

as for terminology: rpt calls numbers that lack any suffixes "unitless". all
arithmetic operations result in this format (the result never has underscores).
users have to convert them back to unitful representations. the other one is
"metric". these have the K, M, G suffixes meaning 1_000, 1_000_000,
1_000_000_000 multipliers. then there are bytes and durations. see below for the
list of multipliers.

units:

- K 1000
- M 1000 * 1000
- G 1000 * 1000 * 1000
- T 1000 * 1000 * 1000 * 1000
- P 1000 * 1000 * 1000 * 1000 * 1000

bytes:

- Ki 1024
- Mi 1024 * 1024
- Gi 1024 * 1024 * 1024
- Ti 1024 * 1024 * 1024 * 1024
- Pi 1024 * 1024 * 1024 * 1024 * 1024

duration:

- s 1000
- m 60 * 1000
- h 60 * 60 * 1000
- d 24 * 60 * 60 * 1000
- w 7 * 24 * 60 * 60 * 1000

see the functions starting with "to" on how to convert between the various
formats.


## functions

abort0, abort1
log1
find2
ireplace3, ireplaceall3, replace3, replaceall3
listhead1, listhead2, listtail1, listtail2  # split off sublists
tuphaschecks0
tupitems1 # returns tuple items
tuptypeitems2 # returns the items that have a specific type
tupcheckeditems1 # returns all the items from the input tuple that have checks.
tupcheckedtypeitems2
tupdoprint # removes all the "noprint" tags from a tuple.
tojson
fromjson # very rudimentary parser. transforms json to rpt via regex and parses.
s/eval/alt/
s/tupitems/titems/
s/tupcheckeditems/tallitems/
s/tuptypeitems/tallobjects/
s/tupcheckedtypeitems/tobjects/

### extend#

extend merges 2 or more tuples. in the case of two arguments it work as if the
right argument inherits from the left argument. in the case of three arguments
the third argument inherits from the merge of the first two arguments and so on.
in other words for common fields, merge uses the definition of the rightmost
argument.

```
foo {
  a 1
}
bar {
  a 2
}
# x.a will be 2.
x foo bar !inherit2
```

### merge#

merge merges 2 or more tuples. there is no overriding behavior here. where the
common fields have different value, merge# sets them to error. this is a safer
version of the inherit function.

```
foo {
  a 1
}
bar {
  a 2
}
baz {
  a 2
}
# this is ok.
x foo bar !merge2 'a' !lookup2
# this is an error: foo.a is not the same as baz.a.
y foo baz !merge2 'a' !lookup2
```

### override#

override merges 2 or more tuples. the tuples on the right hand side cannot add
new fields to the tuple. in case of a "params" tuple one might want to ensure
that inheritors do not add new fields accidentally (e.g. typo). using override
prevents this behavior. note that upon the first evaluation of the resulting
tuple rpt partially evaluates both sides so that it can verify the affected
keys.

```
foo {
  a 1
}
bar {
  a 2
}
baz {
  b 3
}
# this is ok.
x foo bar !override2
# this is an error: field b not found in foo.
y foo baz !override2
```

### tobytes1
- `1024 !tobytes1` -> `1Ki`

### toduration

### tonum1

tonum evaluates a number to the unitless representation.
- `1K !tonum1` -> `1000`
- `1h2m3s4 !tonum1` -> `3723004`

### tometric1

- `1234567 !tometric1` -> `1M234K567`
- `1m !tometric1` -> `60K`

### tounderscores1

- `1234567 !tounderscores1` -> `1_234_567`


## directives

### import

[todo: fancy file resolver. similar to reference resolving. tries finding the
first component up to the chain and then goes from there. no need to have import
paths.]

### load


## c interface

initialize a rpt structure with `rptload` or `rptparse`. `rptload` loads
from a file, it is just a convenience wrapper around rptparse. these functions
return true or false depending whether the load was success. if there was an
error the rpt structure's error member will contain a string representation of
it. the error field will be valid only until the rpt implementation call with
the same rpt object.

`rpteval` evaluates an expression and then serializes the result into a string.
`rptprint` evaluates an expression which must result in a string. the latter
will avoid having quotes and it will not escape special bytes like newlines. the
result will be put into the rpt structure's res field. on failure the functions
return false and the error field will be set to the appropriate error. although
the result is always zero terminated, there is a reslen field in case the result
might contain 0 bytes itself. note that the result is only valid until the next
call to `rpteval`, `rptprint`, or `rptfree` with the same rpt object.

call `rptfree` once the rpt is no longer needed. call `rptfree` even if
`rptload` or `putparse` failed.

example usage:

```
int getport(void) {
  struct rpt t;
  if (!rptload(&t, "myconfig.rpt")) {
    printf("error loading myconfig.rpt: %s\n", t.error);
    rptfree(&t);
    return -1;
  }
  if (!rptprint(&t, "server.port !tonum1")) {
    printf("evaluation error: %s\n", t.error);
    rptfree(&t);
    return -1;
  }
  printf("port number: %s\n", t.res);
  int port = atoi(t.res);
  rptfree(&t);
  return 
}
```


## debugging

rpt comes with a debugging command too in case the errors are too complex to
understand. a simple example:

```
# sample.rpt
x {
  y {
    a 1 b +
  }
  b 3 4 +
  c y.a b +
}
# cmdline
$ rpt trace sample.rpt x.c
name value location expression
x.c 15 sample.rpt:6 y.a b +
  x.y.a 8 1 b +
    1 constant
    x.b 7 3 4 +
      3 constant
      4 constant
      + builtin
    + builtin
  x.b 7 3 4 +
    3 constant
    4 constant
    + builtin
  + builtin
```

a more complicated example:

```
# sample.rpt
parent {
  x [
    1 foo.bar +
  ]
  foo {
    bar y
  }
}
child parent {
  y x
}
# cmdline
$ rpt eval child.foo.bar
'error: cyclic reference to child.y while evaluating it' !error1
$ rpt trace child.foo.bar
name value expression
child.foo.bar error y
  child.y error x
    child.x list
      child.x._0 error 1 foo.bar +
        1 constant
        child.foo.bar error y
          child.y cycle
```

## example

```
# base/widget.rpt
widget {
  # the coordinates are in character units.
  x 'missing: x coordinate of the top left corner.' !error1
  y 'missing: y coordinate of the top left corner.' !error1
  id !tuplename0
  shown .shown true eval
  type 'widget'
}

label widget {
  text 'missing: text to display.' !error1
}

editbox widget {
  text ''
  # width and height is in character units.
  width 80
  height 1
  # set this to true
  passwordmode false
}

button widget {
  text 'missing: text on the button.' !error1
  width 16
  # set this to false to grey out the button, to make it unclickable.
  enabled true
}
```

now create a login widget:

```
# mywidgets/login.rpt
import widgetlib 'widgets/base.rpt'

login {
  usernamelabel widgetlib.label {
    x .x
    y .y
    text 'username:'
  }
  pwdlabel widgetlib.label {
    x .x
    y .y 1 +
    text 'password:'
  }
  username widgetlib.editbox {
    x .x 10 +
    y .y
    width 16
  }
  password widgetlib.editbox {
    x .x 10 +
    y .y 1 +
    width 16
  }
  login widgetlib.button {
    x .x
    y .y 2 +
    text 'login'
  }
}
```

now it is very easy to add a login widget to my blog site:

```
# blog.rpt
import widgetlib 'widgets/base.rpt'
import loginlib 'mywidgets/login.rpt'

...
greeting label {
  x 0
  y 0
  text 'welcome to my blog!'
}
login loginlib.login {
  x 100
  y 20
}
...
```

and now every widget gets to its proper place. example:

```
$ rpt eval blog.rpt login.login
{
  id 'login.login'
  shown true
  text 'login'
  type 'widget'
  x 100
  y 22
}
```

observe base's shown field. although it defaults to true, it looks for a value
in a higher context. use this to completely disable all login widgets with one
line:

```
# blog.rpt
...
login loginlib.login {
  x 100
  y 20
  shown false
}
...
# cmdline
$ rpt eval blog.rpt login.login
{
  id 'login.login'
  shown false
  text 'login'
  type 'widget'
  x 100
  y 22
}
```

or override just specific fields

```
# blog.rpt
...
login loginlib.login {
  x 100
  y 20
  usernamelabel {
    text 'name:'
  }
}
...
# cmdline
$ rpt eval blog.rpt login.usernamelabel
{
  id 'login.usernamelabel'
  shown true
  text 'name:'
  type 'widget'
  x 100
  y 20
}
```

the expression language also allows working with the tuples. here is an example
that recursively prints the identifiers of all the widgets.

```
$ rpt print - 'result %0a !listjoin2' <<EOF
import widgetslib 'blog.rpt'

# t getwidgets1: get the list of widget ids in the t tuple.
getwidgets1 {
  t arg1
  isthiswidget t.type 'widget' == 0 !eval2
  thisids isthiswidget !emptylist0 t.id !list1 !select3
  childids t !tuplevalues1 getwidgets !listmap2
  result thisids childids :
}

result widgetslib.login !getwidgets1
EOF
widgetslib.login.login
widgetslib.login.password
widgetslib.login.password
widgetslib.login.pwdlabel
widgetslib.login.username
widgetslib.login.usernamelabel
```


## example 2

```
# base/job.rpt
jobtmpl {
  # override these two fields.
  binary 'missing binary name' !error1!
  args {
  }
  # full commandline appears here.
  cmdline binary ' ' _argstr !concat3
  # helper logic.
  # [k, v] _join1 -> 'k v'
  _join1 {
    # . is the string concatenation operator.
    # arg1 is prefilled in a function invocation context.
    result arg1.0 ' ' arg1.1 .
  }
  _arglines _join1 args !items1 !map2
  _argstr _arglines '%0a' !listjoin2
}

# servers.rpt
import joblib base/job.rpt
servertmpl joblib.jobtmpl {
  binary 'thttpd'
  args {
    -D ''  # do not daemonize
    -p vars.port 8000 !eval2  # make the port number configurable.
    -l '/dev/stdout'  # log to stdout.
    -d '.'  # serve the current directory.
    -M 1h30m 1s /  # cache for 1.5 hours
  }
}
websitetmpl servertmpl {
  args {
    -d env.HOME 'websites' !up.context0 !pathjoin3
  }
}
johnblog websitetmpl {
}
bobsite websitetmpl {
}

# on bash's commandline:
$ echo $HOME
/home/myuser
$ echo $(rpt -v 'port 12345' print servers.rpt johnblog.cmdline)
thttpd
-D
-M 5400
-d /home/myuser/websites/johnblog
-l /dev/null
-p 12345
$ $(rpt -v 'port 12345' print servers.rpt blog.cmdline)
127.0.0.1 - - [30/Aug/2018:17:53:56 +0100] "GET / HTTP/1.1" 200
...
```

build example:

```
target {
  type target template
  name `set name` -> !tuplename0
  srcs `list of source files`
  arts `list of artifacts this target generates` -> [name]
  deps `names of dependencies`
  cmd  `command to generate the artifacts`
  subtargets {}  # any additional targets to create.
}
```

usage:
```
buildlib 'build.rpt' !import1
rpt buildlib.target {
  srcs ['rpt.c'] +++ '*.h' !sysglob1
  arts ['rpt']
  deps ['/compilers/clang']
  cmd 'clang -o %s %s' name srcs ' ' !listjoin2 !strfmt3
}
```

then the buildtool can run in an interactive mode to generate list of commands.
one mode is where it outputs everything it can build, one per line. each line is
a number and the command to run.


userdb example:

```
$ cat users.rpt
james {
  fullname 'james james'
  email 'james@example.com'
  note 'my name is james.'
}
john {
  fullname 'john john'
  email 'john@example.com'
  note 'living the dream'
}
$ cat johnblog.rpt
serverslib 'servers.rpt' !import1
userslib 'users.rpt' !import1
johnblog serverslib.userblog {
  user userslib.james
}
```

this works fine but what if you have 1 million users? users.rpt would be
massive and the evaluation might run into limits. a simple solution to this
would be shard the config files. usersj.rpt would only contain the users
starting with j. in fact this is something users.rpt could do and as such hide
the sharding scheme entirely from the users.rpt users:

```
$ cat users.rpt
usershards {
  a 'usersa.rpt' !import1
  ...
  z 'usersz.rpt' !import1
}
getuser1 {
  firstchar arg1 !strhead1
  result usershards firstchar !lookup2 arg1 !lookup2
}
$ cat johnblog.rpt
serverslib 'servers.rpt' !import1
userslib 'users.rpt' !import1
johnblog serverslib.userblog {
  user 'james' !userslib.getuser1
}
$ rpt deps johnblog.rpt johnblog
johnblog.rpt
servers.rpt
users.rpt
usersj.rpt
```

thanks to rpt's lazy evaluation semantics, rpt will only load usersj.rpt.



## constraints

strings cannot contain the 0 byte. this means not even the loaded files can
contain it. this is not a language for processing binary data.


# evaluation

the lookup rules are somewhat straightforward but they are not always obvious.
it is best to explain through an example.

```
# sample.rpt
name 'john'
foo {
  name 'bob'
  baz {
    user name
  }
}
bar {
  qux foo.baz
  quz foo.baz {
  }
}
quux foo {
  name 'james'
}

# cmdline
$ rpt print sample.rpt bar.qux.user bar.quz.user quux.baz.user
bob
john
james
```

at every stage of evaluation rpt has a "context". it is pointing to a tuple
where it will start the evaluation from. by default the starting context is
`global.main` and is open an open context. open and closed means whether further
lookups can go higher or not. in open contexts it can, in closed ones it cannot.
the initial context is always open, further contexts are usually closed ones
unless a reference reopens it via the : separator.

if you look up `name` rpt will first look for name in global.main where it
actually finds the definition and returns `john`.

if you look up `foo.baz.name` rpt will first look for `foo`. when it finds it,
the context changes to `global.main.foo` and it is a closed context. then it
will look for `baz` there. it finds it and changes the context to
`global.main.foo.baz` which is also a closed context. then it tries to look up
name in there. rpt does not find `name` in that tuple. nor does the tuple have a
parent to look further so rpt returns an error for this case.

if you look up `foo.baz:name` rpt will similarly get to `global.main.foo.baz`.
however in this case this will be an open context due to the : separator. rpt
then tries to look up `name` in that tuple but it does not exist, nor has the
tuple a parent. but since the context is open, rpt can look higher up. it will
try to look up `name` in `global.main.foo` and it finds `bob` there, returns
that.

if you look up `foo.baz.user` rpt will get to `global.main.foo.baz` closed
context. it will find `user` there but it is an expression. rpt has to evaluate
it. the expression consists of a reference to `name`. rpt has to look it up.
remember that the current context is `global.main.foo.bar`. whenever rpt starts
a reference lookup, the context starts out as open from the current context. the
closed context concept only exists in the middle of a reference lookup. it
cannot find `name` there nor in its parent but since the context is open, it
will find `name` in `global.main.foo`. the value there is `bob` so the result
will be also `bob`.

if you look up `bar.qux.user` rpt will get to the `global.main.bar` context.
there it sees that `qux` is an expression of a single reference. when rpt starts
to look up the reference it cannot find `foo` in `global.main.bar` nor in
`global.main.bar`'s parent so it goes one context higher, to `global.main`.
there it finds the `foo` tuple, goes into it, finds the `baz` tuple. each tuple
has a context associated with it. the result for `global.main.bar.qux` is a
tuple that has `global.main.foo.baz` context associated with it. "associated
with" means that looking up references in that tuple must start from that
context. so `bar.quz.user` really means `global.main.foo.baz.user`. and the
value for that is `bob`.

if you look up `bar.quz.user` then it will see that `quz` is also an expression.
the result of that expression is a tuple that has `foo.baz` as the parent tuple.
it does not find `name` in it so rpt looks for a definition in the parent tuple.
it finds `global.main.foo.baz.user` but it does not take the result of it, but
its definition which is `name`. it then creates a `user` field in the `quz`
tuple with `name` as its definition. then it evaluates `name` in the context of
`global.main.bar.quz`. in this case `name` resolves to `global.main.name` which
is `john`.

in other terms: when you "copy" or "return" a tuple, rpt will only copy the
handle or the pointer to an already existing tuple. it does not create new
tuples. you need to explicitly instantiate a new tuple with {} to get that.

in the case of `quux.baz.user` rpt does not find `baz` in `quux` but it does
find it in the parent tuple. rpt copies the definition to `quux` and then it
evaluates `baz.user` in the context of `global.main.quux`.

another demonstration of the context construct:

```
a 1
b a
foo {
  a 2
  c a
  d b
}
# foo.c->2 foo.d->1
```

`main.foo.d` resolves to `main.b`. rpt evaluates `main.b` in the context of
`main`. therefore it looks for `a` in `main` rather than `main.foo`. this is why
`foo.d` will resolve to `main.a`.

or take this example:

```
foo {
  a {
    x 'fooval'
  }
  b a {
  }
}
bar {
  a {
    x 'barval'
  }
  b foo.b {
  }
}
# bar.b.x -> fooval
```

to evaluate `bar.b.x` rpt needs to evaluate `foo.b` first. when evaluating
`foo.b` it needs to evaluate `a`. however that evaluation happens in the context
`foo` so that is why the grandparent tuple of `bar.b` will be `foo.a`.

todo: describe super

```
a {
  name 'james'
  user name
}
b a {
  name 'john'
  user super.name
}
c b {
  name 'bob'
}
# rpt: a.user->james b.user->james c.user->john
# gcl: a.user->james b.user->james c.user->james
```

```
a {
  foo '1'
}
b a {
  foo super.foo '2' .
}
c b {
  foo super.foo '3' .
}
# c.foo should be '123', right?
```

another tricky case:

```
foo {
  a {
    foo = 1
  }
  b {
    c = a {
    }
  }
}
bar {
  a {
    bar = 2
  }
  d = foo.b {
  }
}
$ gcl a.gcl print bar.d.c
{
  bar = 2
}
```

rpt evaluates functions in the invocation's context:

```
# sample.rpt
foo {
  multiplier 100
  multiply1 {
    result arg1 multiplier *
  }
}
baz {
  multiplier 10
  qux 1 !foo.multiply1
  # the above line is equivalent to following.
  _tmp multiply1 {
    arg1 1
  }
  qux _tmp.result
}

# cmdline
$ rpt print sample.rpt baz.qux
10
```

in this example _tmp does not see `foo.multiplier` because it evaluates the
`multiply1` tuple in the `baz` context. in fact if you remove the `multiplier
10` line, rpt will print an error. in case you still want the original
multiplier, you need to refer to it as `file.foo.multiplier`.

when inheriting tuples rpt does a full, deep copy definitions of each field in
the parent tuple. beware of this. lists can be particularly expensive. keep
large data at top level and just refer to it via references which are cheap to
copy.

```
$ cat sample.rpt
a {
  x [
    1
    2
    ...
    100_000
  ]
}
b a {
}
c {
  x file.a.x
}
d c {
}

$ rpt -s eval sample.rpt '{}' >/dev/null
some amount of strings and values
$ rpt -s eval sample.rpt a b >/dev/null
lots of strings and values
$ rpt -s eval sample.rpt c d >/dev/null
lots of strings and values
```

for b.x rpt had to fully reevaluate the x field in b's context. a.x has a long
definition, so it is expensive to copy. however c.x's definition is short, that
is quick to copy.


# limits

1M tuples and lists. 10M strings and values. use -b to "break the limit" and
have 10x increase in the limits.

suppose you have an online dictionary service. users set a language and enter a
word. the service writes the definition of the word in english. the word can be
in any language. you configure the service with 'language word meaning'
triplets. you have a config like this:

```
$ cat dict.rpt
utilities {
  # creates this:
  # [
  #   [ 'austrian', {
  #     bar 'barbar'
  #     ...
  #   }],
  #   [ 'swiss', {
  #   ...
  #   }],
  # ]
  langwordtuples file 'langdef' !checkedtypeitems2 !map2
  # { a 'b' ; c 'd' } -> ['a b', 'c d']
  _wordtupletolist1 {
    _fieldtostring1 {
      result arg1.0 ' ' arg1.1 . .
    }
    result _fieldtostring1 arg1 !items1 !map2
  }
  # creates this:
  # [
  #   [ 'austrian', [ 'bar barbar', 'foo foofoo', ... ] ],
  #   [ 'swiss', [ 'bar barbar', ... ] ],
  #   ...
  # }
  # 
  _tolist {
    langwordspair arg1
    return langwordspair.0 !_wordtupletolist1 langwordspair.1 !map2 !list2
  }
  langwordlists _langtupletolist1 _tolist !map2
  # creates this:
  # [
  #   'austrian bar barbar'
  #   'austrian foo foofoo'
  #   'swiss barbar'
  #   ...
  # ]
  _prefixwords1 {
    lang arg1.0
    wordlist arg1.1
    _prefixword1 {
      result arg1 ' ' lang !concat3
    }
    result _prefixword1 wordlist !map2
  }
  wordlist _prefixwords1 langwordlists !map2 !flatten1
  # suitable for printing and sending to production.
  wordlistcfg wordlist "\n" !listjoin2
}
# strip the above and only have this:
import utilitieslib utilities.rpt
meanings {
  bar 'barbar'
  foo 'foofoo'
  qux 'quxqux'
}
langbase {
  type 'langdef'
}
germanbase langbase {
  nochecks
  words {
    germanbar meaning.bar
    germanfoo meaning.foo
  }
}
austrian germanbase {
  words {
    austrianqux meaning.qux
  }
}
swiss germanbase {
  swissquux meaning.qux
}
$ rpt print dict.rpt utilitieslib.wordlistcfg
austrian austrianqux quxqux
austrian germanbar barbar
austrian germanfoo foofoo
swiss germanbar barbar
swiss germanfoo foofoo
swiss swissquux quxqux
$ prodrpt push dict.rpt
```

prodrpt evaluates `utilitieslib.wordlistcfg` and pushes it to the production
dictionary servers. now suppose your service keeps growing and you keep adding
languages and words. eventually you will hit rpt's limits. the rpt tool will
fail. what should happen now? ideally you should refactor the config file! you
should shard the languages. put the english dialects into one, the german
dialects into another, and the rest into a third file. this way you will not
need to work with huge files. your editors, diff tools, version control tools
will be able to handle smaller files with greater ease.

what if you hit the breakage at a bad time? e.g. when you urgently need to add
some clarification to a meaning but the tool is failing? you can use the
breaklimiter in rpt, -b. this way you can inspect the output. the prodrpt tool
can just always use the -b mode so it would always work. the point is that
production can runs with higher limits but at the human level you have low
limits so you notice the problems in time and start the refactoring work. in the
above case you push the urgent change, but when things settle down, you start
refactoring the config file. in the meantime you will annoyingly need to use the
-b flag. this is good because this annoyance will motivate you to refactor
rather than let the config grow unbounded.

example about conditional override: you only want to override a field if it
exist in parent. do something like this:

```
parenta {
  myfield {
    a 1
  }
  boringfield1 {
    b 2
  }
}
parentb {
  boringfield2 {
    b 3
  }
}
# same as tupsilentoverride basically.
addoverrides2 {
  oldtuple args.0
  overrides args.1
  filterfunc1 {
    key args.0.0
    result oldtuple key !tuphaskey2
  }
  filtered overrides !tupitems1 filterfunc1 !filter2
  result oldtuple filtered !totuple !tupexpand2
}
a parenta {} { myfield { a 2 } } !addoverrides2
b parentb {} { myfield { a 2 } } !addoverrides2

# with types this works just as well:
fieldtype {
  type field
}
parenta {
  myfield file.fieldtype {
    a 1
  }
  boringfield1 file.fieldtype {
    b 2
  }
}
parentb {
  boringfield2 file.fieldtype {
    b 3
  }
}
a parenta { myfield { a 2 } } !tuptaggeditems1
b parentb { myfield { a 2 } } !tuptaggeditems1
```


todo re limit: also mention an option to have intermediate evaluated files. e.g.
you have a model config, it generates an rpt file. then you have a production
config. it does not load the model config but it loads the generated rpt file.
this cuts of unnecessary cruft and speeds the evaluation up.


todo: eval should comment closing braces:
a {
  b {
    c 3
  } # a.b
} # a


todo: rewrite with these headers:

intro: show a simple example, introduce rpn, show example 2.

motivation: talk how i wanted gcl, the "great config language". how i hate
superfluous symbols. how i want things easy to use from cmdline. also mention
that primary aim was to keep the evaluator very lazy, things will not error out
unless the error is really unavoidable. e.g: '(foo bar { i 3 }).i' will evaluate
to 3 just fine even though bar does not exist. however evaluating 'j' in that
context would error out since rpt needs to know about bar in order to look up j
in it.

strings
numbers

errors: support `foo` for error.

expressions

references: the eval command does not print references starting with an
underscore. this way you can have two tuples referring to each other and not
have the evaluator enter an infinite loop.

tuples:

tuple fields can have check, noprint tags. noprint tag is useful to avoid
spamming the eval. it allows writing recursive structures and not have eval go
kaboom. the check tag implies noprint (no need to evaluate the exec branch).

```
fib {
  n ...
  result ...
  noprint fibn-1 ...
  noprint fibn-2 ...
}
```

the other benefit of this is that users can have a lot of temporary, helper
tuples without spamming the printout too much. it is nice if someone does not go
full integration with the tool, just relies on print for diffs (e.g. regression
tests like for this project). people could have this pattern:

```
joblib '//rpt/templates/job.rpt' !import1
myjob joblib.job {
  noprint cfg {
    # all sorts of parameters and calculations here.
  }
  # here you would only have the actual fields you need.
  ram cfg.ram
}
```


lists
imports
global

operators:

str: ++, ==, <, <=, ..., stralt (arg1 if nonempty, arg2 if empty)
num: +, -, /, *, ..., .==, .<, .<=, numalt (arg1 if num and nonzer, arg2
     otherwise)
list: +++

functions

checks:

mytuple {
  # must be the first thing to parse.
  nochecks # this is optional
  check name1 tupleref1
  check name2 tupleref2
  check name3 name3
  # no check commands can appear from now on.
  tupleref1 {
    # must evaluate to true.
    result ...
    # this will be the result of the whole tuple if result is false. must
    # evaluate to error.
    fail ...
  }
  tupleref2 {
    result ...
    # alternative to fail. can use log1 or abort1 commands.
    exec ...
  }
  # tuple's name can be the same as the check's name.
  name3 {
    result ...
    fail ...
  }
}

todo: s/nochecks/templates; make sure the tuples can have a "type" tag too.

parser: beware with inline tuples. tuples try to consume a value from stack if
there is something there. if that is unwanted, use empty.

```
# instead of
a [ 1 ; 2 ; 3 ] { result arg1 arg2 + } 0 !listfold3
# write this
b [ 1 ; 2 ; 3 ] emptytuple { result arg1 arg2 + } 0 !listfold3
# or this (???)
b [ 1 ; 2 ; 3 ] {} { result arg1 arg2 + } 0 !listfold3
```

evaluation/context
limits
conventions
builtins

tool: describe commands eval, print, trace, deps, format, usage, interactive.
option -s for stats, -d depth. sending sigint displays the current context. need
to use sigquit (ctrl+\) to quit. -f for filter mode. e.g. "-f
job,alloc,service". usage reports how many tuples, lists and values a reference
entails. for list print prints each element recursively, one per line. for
tuple, print prints tuple.result.

examples

performance: compare size, memory, cputime for parsing and fibonacci with gcl,
flabbergast, jsonnet.

c

implementation: pointers are hard to debug. they even change from run to run.
debugging with integer identifiers is so much easier! setting up watchpoint with
numbers instead of pointers is so much easier.

build: tests are grouped like this:

- 1: rudimentary checks.
- 2: builtins.
- 3: tricky cases.
- 4: failures (stuff where rpt tool returns 1).
- 5: large cases (not testing output, just that it runs in time).
- 6: misc (print command, stats output, log, abort builtins, envvars etc.).

faq:

could rpt have larger limits or not have limits at all? no. yes, having limits
means that rpt will not suit some usecases. however the limits are quite large,
they work for the most of the usecases. the pareto principle applies. it suits
80% of the cases. users of the remaining usecases should either

can rpt support the `local` and `final` tuplefield tags similarly to gcl? no.
they are hard and confusing. they make debugging hard. the need for them can be
avoided with a good naming style. e.g. use "noprint [filename][fieldname]" and
nobody will see or accidentally override your fields. even if they do, if
healthy review process (that reviews evaluated config diffs) should catch errors
stemming from this. a system should not go overboard with the number of safety
nets or moving across the field will be very painful. if you truly want to
encumber yourself by these "features", use a "grownup" language like flabbergast
or jsonnet (link to the feature).

glossary:

bcl: best config language
gcl: generic config language

resources:

- flabbergast: http://flabbergast.org/
- jsonnet:
- gcl: https://gcl.readthedocs.io
- regex:
- critbittree: https://www.imperialviolet.org/binary/critbit.pdf



todo: mention that this a dynamically typed hackery language that allows very
easily just "copy the whole algorithm" and tweak bits and pieces here and there
(todo: add example). there is not many languages like this. however this comes
at a maintainability cost. therefore this language is severely limited: it is
hermetic, the evaluation cannot depend on its environment (other than importing
the source files) and the evaluation is severely limited to ensure that it
always runs fast. with these two constraints the problems stemming from
unmaintainability is somewhat mitigated since it is very easy to see the effects
of the changes. without the limitations people could use this for a much bigger
scope, it would acquire a lot of dependencies and slowness, it would be
impossible to use it interactively. this language has very weird, unintuitive
rules, statelessness and interactivity is required for sanity. example: you have
a server and you want to run it in a development, test, production environment.
most of config is the same, only need minor tweaks here and there. this language
allows you to create the common config, then inherit from it for each
environment and make the necessary tweaks let it be just overriding a value, or
be it a change in logic. for instance you could have multiple production
environments, each with different amount of expected traffic (e.g. queries per
second): you just override the expected traffic in one place and through the rpc
logic this automatically adjust the various memory buffer and queue sizes
without overriding those explicitly.
