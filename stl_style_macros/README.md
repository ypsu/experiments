# STL-like generic containers and functions in C

*Note*: This is something you don't want to do in C. Just use a modern language
if you need these things. What follows is just an exploration of ideas and
possibilities for fun.

Although I'm not too fond of too much generic programming but there are
definitely cases when it comes in handy. This is best demonstrated by C++'s STL
which is collection of utilities and classes for vectors, heaps, binary search
trees, hash_maps, etc. for arbitrary types. They are quite easy to use. Just
type `vector<int> v;` and you have a vector of integers and every operation on
them is type-safe.

The ease of use of the C++ templates comes with a cost though. In each
compilation unit you use a template function (aka in each .cc file) you will
instantiate it (aka compile it). This will slow down the compilation and
increase the size of the resulting .o files even though the duplicates are
thrown out at the linking stage. This can add up to significant time when you
are using a `vector<int>` throughout 1000+ files and trying to do a clean build
not to mention the fact that even the tiniest change in the implementation will
invalidate those 1000+ files which you then need to recompile.

On the other hand the C language doesn't have templates so there are two common
approaches to solving this problem.

The first approach is to use void* pointers everywhere. This gives us a very
generic code but has three downsides. First, we lose all type-safety checks from
the compiler while using said library. Second, the compiler can't optimize for
the specific type so the code is slower. Third, we get memory fragmentation
because all you can really do is store pointers in the buckets rather than
storing the elements directly. As an upside, you get only one definition for the
functions so the compilation/linking step remains efficient.

The other common approach is to define the functions in macros which then you
have to manually instantiate. Usually all the functionality is defined in
macros. Those macros are a bit awkward to write. For instance you have to
terminate all lines via backslashes and have to be very careful when referencing
macro arguments.

glib is one of the most well known generic library for C. It actually contains
examples for both approaches. GHashTable has pointers everywhere while GArray
has macros everywhere.

Even with the latter example of GArray, you have to pass the type to the macro
even for simple functions like g_array_index. That looks like it isn't very type
safe. Also, it means that each compilation unit contains the definitions of
those functions rather than quarantined into single separate translation unit.

So here comes the next idea which I don't see quite often on the forums. It is a
bit more verbose so maybe that's why. It won't be as easy to use as STL, but
you'll get real functions when you are using the generic containers, you will be
mostly writing real C code instead of macros when you are writing the generic
containers, you will get all type-specific compiler optimizations and each
generic container will be defined only in a single translation unit. In fact it
will be an error to specify it in multiple ones. Note that because of that you
will need to statically link your binaries (which is a good idea just by itself)
and enable link time optimization in your compiler for the optimizations to
fully work. It is a bit non-obvious how to set up proper static linking with
proper symbol visibility and ensuring that the LTO works, but it is doable given
enough research in the deep web archives.

I'll demonstrate my idea with two concepts. The first will be a generic
resizeable array (the vector in C++) and the second will be a generic sort
function. Let's start with the dynamic array which I'll refer to as dynarray
from now on ("vector" sounds a very weird name to use for such a concept).

It is quite hard to avoid macros with complicated definitions but we need to
ensure we keep it to a minimum. Let me demonstrate the usage first. Suppose you
want create a dynarray for int with a custom allocator just like you can do
optionally in STL. You would do the following. First, you create dynarray_int.h:

```c
#ifndef DYNARRAY_INT_H
#define DYNARRAY_INT_H

#define G_TYPE int
#include "dynarray_generic.h"

#endif
```

When you do the above the full expanded file will contain the declarations for
the dynarray_int structure, the dynarray_int_clear function, the
dynarray_int_reserve function etc. And for the definitions we have
dynarray_int.c:

```c
#include "dynarray_int.h"
#include "my_xrealloc.h"

#define G_TYPE int
#define G_REALLOC(oldp, newsz) xrealloc((oldp), (newsz)*sizeof(G_TYPE))
#include "dynarray_generic.c"
```

And now you have to link dynarray_generic.c and you are set. Note that the
G_REALLOC and G_FREE parameters are completely optional. I'm modifying only
G_REALLOC to use my own wrapper around the standard realloc which does error
handling by crashing the binary when realloc fails. I omit changing the optional
G_FREE because the default free() is what I want anyways. I'm demonstrating here
three things. First, any optional parameters can be specified optionally. Not so
in C++. If a template has let's say 3 arguments (e.g. std::set) but you only
want to customize the first and third, you still have to put the default for the
second argument. Second, I demonstrate that we could possibly use realloc() as
the allocator which might give us faster memory reallocations in some cases.
AFAIK this is still unavailable in C++ at this point so you need to new then
delete in all cases. Third, I demonstrate that implementation details like what
allocator we are using are not exported to the interface achieving better
information hiding of the irrelevant details and the header itself will need
less dependencies than your typical C++ typedef.

Before we delve into dynarray_generic internals, let's create a helper macro in generic_helpers.h to concatenate preprocessor tokens because that we are going to do a lot:

```c
#ifndef GENERIC_HELPERS_H
#define GENERIC_HELPERS_H

// CC(x, y): ConCatenate the preprocessor tokens.
// Example: "void CC(x,y)();" will translate to "void xy();". Both x and y can
// be macros themselves.
#define CC(x, y) _CC(x, y)
#define _CC(x, y) x##y

#endif GENERIC_HELPERS_H
```

From the interface point of view, we only need two arguments to create a
dynarray. The type it contains and the name of the typed dynarray. But I don't
like specifying too much arguments so we will autogenerate the name in case it
is omitted. The default name will be in the form dynarray_T where T is the type.
So here's the full dynarray_generic.h:

```c
#ifndef G_TYPE
# error "You need to define G_TYPE to create the dynarray for."
#endif
#ifndef G_NAME
# define G_NAME CC(dynarray_, G_TYPE)
#endif

#include "generic_helpers.h"

struct G_NAME {
	int reservation;
	int size;
	G_TYPE *arr;
};

void CC(G_NAME, _clear)(struct G_NAME *v);
void CC(G_NAME, _reserve)(struct G_NAME *v, int count);
void CC(G_NAME, _pushback)(struct G_NAME *v, G_TYPE elem);

#undef G_TYPE
#undef G_NAME
```

I'll omit the full library functions so I just specify a few example functions
to demonstrate the idea. Notice the error handling here. If you omit a mandatory
argument you get a nice, user friendly compiler error. Also, even if you make a
mistake in a parameter, you'll get a simple compiler error rather than deeply
nested, unreadable error like in C++. Ideally there would be some comments at
the top of this file summarizing all the arguments and definitions this header
consumes/generates but I'll omit this because I'm narrating it here.

Now let's take a look at dynarray_generic.c:

```c
#ifndef G_TYPE
# error "You need to define G_TYPE to create the dynarray for."
#endif
#ifndef G_NAME
# define G_NAME CC(dynarray_, G_TYPE)
#endif
#ifndef G_REALLOC
# define G_REALLOC(oldp, newsz) realloc((oldp), (newsz)*sizeof(G_TYPE))
#endif
#ifndef G_FREE
# define G_FREE(oldp) free((oldp))
#endif

#include <stdlib.h>

#define clear CC(G_NAME, _clear)
#define reserve CC(G_NAME, _reserve)
#define pushback CC(G_NAME, _pushback)

void clear(struct G_NAME *v)
{
	G_FREE(v->arr);
	v->reservation = 0;
	v->size = 0;
}

void reserve(struct G_NAME *v, int count)
{
	if (v->size+count <= v->reservation) {
		return;
	}
	v->arr = G_REALLOC(v->arr, v->size+count);
}

void pushback(struct G_NAME *v, G_TYPE elem)
{
	reserve(v, 1);
	v->arr[v->size++] = elem;
}

#undef G_TYPE
#undef G_NAME
#undef G_REALLOC
#undef G_FREE
#undef clear
#undef reserve
#undef pushback
```

There is some boilerplate at the beginning and the end but the meat of the
library is pure C opposed to macros. You still have to instantiate to see the
actual compiler errors though.

Now whenever you want to use this, you can do the following:

```c
#include <stdio.h>
#include <string.h>
#include "dynarray_int.h"

int main(void)
{
	struct dynarray_int a;
	memset(&a, 0, sizeof a);
	dynarray_int_pushback(&a, 3);
	dynarray_int_pushback(&a, 1);
	dynarray_int_pushback(&a, 4);
	dynarray_int_pushback(&a, 2);
	for (int i = 0; i < a.size; i++) {
		printf("%d\n", a.arr[i]);
	}
	return 0;
}
```

Now let's take a look at the sorting example. Start with the interface,
sort_generic.h:

```c
#ifndef G_TYPE
# error "You need to define G_TYPE to create the sorting function for."
#endif
#ifndef G_NAME
# define G_NAME CC(sort_, G_TYPE)
#endif

void G_NAME(G_TYPE *base, int size);

#undef G_TYPE
#undef G_NAME
```

It's pretty standard: You can configure the type to sort and the new function's
name. Let's see the definition, sort_generic.c:

```c
#ifndef G_TYPE
# error "You need to define G_TYPE to create the sorting function for."
#endif
#ifndef G_NAME
# define G_NAME CC(sort_, G_TYPE)
#endif
#ifndef G_CMP
# define G_CMP(x, y) ((x) < (y))
#endif
#ifndef G_SWAP
# define G_SWAP(x, y) \
	do { \
		G_TYPE t = x; \
		x = y; \
		y = t; \
	} while (0);
#endif

void G_NAME(G_TYPE *base, int size)
{
	// Selection sort algorithm.
	for (int i = 0; i < size; i++) {
		for (int j = i+1; j < size; j++) {
			if (G_CMP(base[j], base[i])) {
				G_SWAP(base[i], base[j]);
			}
		}
	}
}

#undef G_TYPE
#undef G_NAME
#undef G_CMP
#undef G_SWAP
```

Now at the definition stage you have more options. You can tune the comparison
expression or how the swapping occurs. These details don't clutter the interface
of the function in its header file.

Now let's see a simple example where we use both the dynarray and the sort
function without using separate header files (for simplicity):

```c
#include <stdio.h>
#include <string.h>

// Declare & define the generic functions.
#define G_TYPE int
#include "dynarray_generic.h"
#define G_TYPE int
#include "dynarray_generic.c"

#define G_TYPE int
#include "sort_generic.h"
#define G_TYPE int
#include "sort_generic.c"

#define G_TYPE int
#define G_NAME sort_int_desc
#include "sort_generic.h"
#define G_TYPE int
#define G_NAME sort_int_desc
#define G_CMP(x, y) ((y) < (x))
#include "sort_generic.c"

int main(void)
{
	struct dynarray_int a;
	memset(&a, 0, sizeof a);
	dynarray_int_pushback(&a, 3);
	dynarray_int_pushback(&a, 1);
	dynarray_int_pushback(&a, 4);
	dynarray_int_pushback(&a, 2);
	sort_int(a.arr, a.size);
	for (int i = 0; i < a.size; i++) {
		printf("%d\n", a.arr[i]);
	}
	sort_int_desc(a.arr, a.size);
	for (int i = 0; i < a.size; i++) {
		printf("%d\n", a.arr[i]);
	}
	return 0;
}
```

And that's it. The above will print the vector first in ascending then in
descending order. We could add many other optional tunables to these generic
libraries. We could add G_FUNCATTR which we could set to static and then the
above definitions would be local to a particular file. That could be handy if
you want to sort using a special sort function which only makes sense in a
particular file.

Note that if you start using the above approach you have a new problem. You have
to avoid the duplicate definitions. You need to create a .c file for each new
instantiation or gather all instantiations into a single file. Whenever you need
a new instantiation (e.g. sort_double) you just extend this single file with it.
This will ensure that you can have better control over the bloat these libraries
generate compared to C++. It also speeds up the compilation because these
generic functions are expanded only once for each parameterization thus saving
lot of computational cycles during compilation in exchange for some extra
development overhead and source code noise. And let's not forget the fact that
you retain all the runtime performance wins of C++ templates as long as you use
link time optimization.
