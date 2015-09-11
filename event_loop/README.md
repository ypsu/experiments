# Event handling with long linear functions in C

_Note_: This is rather a braindump. I have not cleaned up this text so my
thoughts are jumping around and I'm repeating the same ideas at a lot of places.
Be warned!

You write a lot of event driven software. These can be web servers, download
scripts, user interfaces, etc. To simplify the discussion, let's use a specific
piece of software to demonstrate my points:

Reddit in a nutshell is a website where users can submit web links to various
content which are upvoted and displayed prominently if other users like the
content. The website itself is divided into subreddits with a specific themes
and users are expected to post content related to that theme. For example
/r/funny contains funny content, /r/games contains video games related content.
A lot of posts are just links to pictures on hosts like imgur.com or other image
sharing websites.

You only care about the images from your favourite subreddits. So you want an
application to which you give the subreddits you are interested in and it will
download the 5 most popular images from the last 24 hours. You'd use the
software like this to download the pictures into the current directory. Example
usage:

```
get_popular_images pics funny gaming
```

Let me give some additional implementation restrictions so that we can
demonstrate a typical complicated event handler code. First: no threads, because
threads are complicated, error-prone and CPU is usually not the bottleneck in
this kind of software anyways. Second: you like to have fast software, you want
that all the content are downloaded simultaneously, so we will definitely need
event handling for the non-blocking IO code.

So how would you write such software in C? The most common pattern is that you
have a bunch of small functions, each doing it's own thing like connecting,
downloading, writing to disk, etc. You then need to manage all this state with
the event manager -- whenever you advance to the next state, you need to update
the event manager's callback function.

My problem with this approach and with a lot of small functions all over the
place that it leads to spaghetti code. When I read such code I need to
erratically jump around -- I can't read the code from top to bottom. The other
thing which makes it hard to understand is the "state". Just try to inspect your
favourite application in a debugger at random points in time. You'll find that
it has huge stack depth and most probably too many wrapper functions which do
nothing just change a few parameters and call a different function.

Unfortunately a lot of functions are single use only and are created to have
"self-documenting code". At each layer you mangle the parameters and call some
other functions. But because bugs are often the complex interactions of random
parts of the system, all this mangling, branching and other erratic jumping
makes the system harder to understand and debug.

I was thinking about these issues for a while now and I'll show my latest idea
on the above example how to tackle this problem. Although note that this is a
toy example, written with absolutely no time pressure so it doesn't really apply
to real world software but rather to an idealistic world. Also, you need to be
very careful with this approach, it very easily degenerates into an
unmaintainable mess. Basically you need to know all the features your
application/system will support *before* you attempt to write code. This
approach is *not* tolerant to last minute changes.

First things first: You need to minimize the global state. Like it or not, your
system has state. You have global variables. You could create them on the main()
function's stack and then pass them around; you could have a singleton pattern
-- a function which always the same statically allocated variable; or you have
constants in the read-only memory. It might ease your conscience if you don't
call them as global variables, but let's face it: they are.

Now that you established you have global state, you have to embrace it. Just
define it at the top of the file where everyone can see it. Now that you can see
that it is global, work hard to keep the information in it minimal.

Next idea: There is nothing wrong with long functions. But still, people find it
painful to read them. Why is that? If you go deep down there's only two main
things the processor can do: compute and jump. The compute lines look like
"x=<expr>" while the jump lines look like "if (x) { y }". I believe the most
pain comes from the jumping part. When you are reading y, you need to know about
x. If you have "if (x){if(y){z}}" now you need about both x and y, also about
the fact that they occur at the same time and read z under those circumstances.
This is a huge cognitive drain for my simple mind. People don't like such drain.
So what is the typical advice? Create a new function and call that:
"f(){if(y){z}} ... if(x){f()}". See? No deep conditionals anymore! I think
that's backwards. That just moves the complexity to a different place and when
you try to read f() it's not entirely clear what's happening and if you do then
you have a similar cognitive strain.

My idea is that you need to linearize your flow! So instead of this:

```c
if (a) {
	if (b) {
		c
	} else {
		d
	}
	e
}
```

You could have this:

```c
ab = a && b;
anb = a && !b;
if (ab) c
if (anb) d
if (a) e
```

Several things happened here. First you created a new name for a&b. In your mind
you can now reference them as ab, which is now less strain. Also, the state is
now very explicit, you don't really have very deep nested conditionals.
Basically you need to give a name to all such blocks and write them out
explicitly. Gladly the 80 line length limit helps enforcing this. You rarely
need to wrap your lines if you use this style.

This can avoid creating all those little functions. I think there are only 2
good reasons to create a function. First is to avoid code duplication in case
you need a similar logic in multiple places. The second is abstraction. But when
is it a good idea to abstract? I don't think just separating the code from one
place to make the logic seemingly simpler is a good reason to abstract. I think
it's good to abstract when your logic/application is used in multiple contexts.
For example a game engine might abstract the graphics calls so it can use both
DirectX and OpenGL. Or a sort function call abstract the comparison part because
you might have multiple sorting criteria.

As mentioned above writing convoluted logic might force you into deep
indentation where you can't write decent code without exceeding the 80 character
limit. This one might prompt you to put the inner logic into its own function.
But before you do so you must think hard and try to "unconvolute" your logic.

All of the above is a bit handwavy and needs a great care when used to create
something palatable.

Now let's get back to the example. I'll keep it as bare bones as possible to
keep the article short, and to keep it readable for experienced C programmers.
This is written for Linux, is not platform independent and I also omit proper
handling other than just exit the application when an error occurs. The HTTP
code just makes too many assumptions about the current state of the world to
make it usable outside this toy example.

First, let's write out all the possible includes we will need:

```c
#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/signalfd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
```

Then let's write some helper functions. I use CHECK() whenever I want to fail in
case a condition fails. I also use this instead of proper error handling. When I
am writing code, I'm often too lazy to actually write the error handling part
because often I don't know yet how to handle the error and I don't want just to
silently ignore it. Note: The usage is similar to assert() but the intent is
different: the checks are always enabled -- even in non-debug mode.

```c
// Utility functions for error handling.
#define CHECK(x) check((x), #x, __FILE__, __LINE__);
void check(bool success, const char *expr, const char *file, int line)
{
	if (!success) {
		const char fmt[] = "%s:%d: Expression \"%s\" failed.\n";
		printf(fmt, file, line, expr);
		printf("errno: %m\n");
		abort();
	}
}
```

As you can see, I also print the errno value. I use CHECK() most often for the
system calls, so printing the current errno is often useful.

I also don't want to care about very rare error conditions, so let's abort if
malloc/read/write returns an error:

```c
void *xmalloc(int size)
{
	void *p = malloc(size);
	CHECK(p != NULL);
	return p;
}
void xread(int fd, void *buf, int count)
{
	CHECK(read(fd, buf, count) == count);
}
void xwrite(int fd, const void *buf, int count)
{
	CHECK(write(fd, buf, count) == count);
}
```

You need to be careful with these functions, use them only if you know that
there is absolutely no good reason for failure or not reading/writing the amount
you wanted.

Note that I've abstracted these functions only when I used the underlying logic
at at least two different places. If a logic is used only in a single place, it
won't appear above.

Now for the event manager we are going to use epoll directly. First we need to
decide about the global state:

```c
enum { MAX_FD = 64 };
typedef void (*event_callback_t)(int fd, void *data);
static struct {
	bool finished;
	int epoll_fd;
	// The descriptor array is indexed by the FD itself.
	struct {
		bool enabled;
		event_callback_t callback;
		void *data;
	} desc[MAX_FD+1];
} events;
```

For our event manager, all global state will be in the events global variable.
So far we need three variables: one to mark to exit from the event loop after we
finished the downloading, one for the epoll's file descriptor and one array for
holding each file descriptor we might use. The file descriptors are usually
small integers, so if we can ensure we don't keep too many of them open, we can
use them directly rather than implementing complicated resizeable arrays or hash
maps.

For each file descriptor we will save three data points. Enabled: To save
syscall overhead, epoll returns multiple events at the same time. It is entirely
possible that one event handler closes another file descriptor which is still in
the queue for example in case of errors. In that case we need to ensure that the
disabled event handler will not be called in the future. Callback: We need to
tell the loop what code to execute when an event occurs on a given file
descriptor. Data: There can be multiple instances of the same event handler for
example we could have multiple subreddit requests ongoing at the same time. The
event handler needs this to distinguish between them and in fact this is usually
pointing to a structure containing all the state for the handler.

Also note, that there are no interface functions for this so far. The manager is
simple enough that you can just use the structure directly rather than creating
an abstraction around it. We will create one helper function though. Whenever
you add/remove an event descriptor you need to flip the enabled flag, set the
callback data, set up the epoll structure and check the epoll_ctl's return value
for errors. This is lot of work to be done at a lot of places so let's put it
into its own function:

```c
void events_ctl(int op, int fd, event_callback_t z_callback, void *z_data)
{
	CHECK(op == EPOLL_CTL_ADD || op == EPOLL_CTL_DEL);
	CHECK(z_callback != NULL || op == EPOLL_CTL_DEL);
	events.desc[fd].enabled = (op == EPOLL_CTL_ADD);
	events.desc[fd].callback = z_callback;
	events.desc[fd].data = z_data;
	struct epoll_event ev;
	ev.events = EPOLLIN;
	ev.data.fd = fd;
	CHECK(epoll_ctl(events.epoll_fd, op, fd, &ev) == 0);
}
```

I'll only allow adding and removing a descriptor but not modifying. Adding
itself only enables the read polling because that's the most common polling. You
will need to manually switch to write polling via epoll_ctl if you need so. I
only needed it at one place thus it I decided to not create a function for it.

Also note the z_ prefix for the pointers. I'm signaling that this pointer can be
NULL, it won't be dereferenced in such case. In fact when you are deleting these
must be NULLs.

By the way one typical use for such interface functions is locking because most
applications are multithreaded. Because this is single threaded, most of the
pain just goes away and an additional benefit is that your application will work
exactly the same way given the same inputs (modulo some entropy coming from
signals, ASLR and other random elements).

Now that we defined the structures for the event loop, let's write the event
loop itself. It isn't very long:

```c
int main(void)
{
	CHECK((events.epoll_fd = epoll_create1(0)) != -1);
	enum { MAX_EVENTS = 64 };
	struct epoll_event epoll_events[MAX_EVENTS];
	while (!events.finished) {
		int cnt;
		cnt = epoll_wait(events.epoll_fd, epoll_events, MAX_EVENTS, -1);
		if (cnt == -1 && errno == EINTR) {
			continue;
		}
		CHECK(cnt > 0);
		for (int i = 0; i < cnt; ++i) {
			int fd = epoll_events[i].data.fd;
			if (!events.desc[fd].enabled)
				continue;
			events.desc[fd].callback(fd, events.desc[fd].data);
		}
	}
	CHECK(close(events.epoll_fd) != -1);
	return 0;
}
```

Let's demonstrate the usage by creating a "wakeup queue". Sometimes you want to
enqueue functions to be called on the event queue instead from the current
point. We will implement this via standard unix pipes. You will be writing a
wakeup_command structure to the one end and on the read side you will read and
execute the callback from this structure. The structure itself looks like this:

```c
typedef void (*wakeup_callback_t)(void *);
struct wakeup_command {
	wakeup_callback_t callback;
	void *data;
};
```

We will need to initialize the pipes first. Because the initialization itself is
called only once, I won't abstract into its own function, we will add it to the
main function itself:

```c
int main(int argc, char **argv)
{
	<event loop initialization>
	int command_fd[2];
	CHECK(pipe2(command_fd, O_NONBLOCK) == 0);
	events.command_fd = command_fd[1];
	events_ctl(EPOLL_CTL_ADD, command_fd[0], wakeup_handler, NULL);

	<event loop>
	CHECK(close(command_fd[0]) == 0);
	CHECK(close(command_fd[1]) == 0);
	<event loop cleanup>
}
```

Now scheduling a callback is simple -- write the callback to the pipe:

```c
void wakeup_schedule(wakeup_callback_t callback, void *z_data)
{
	struct wakeup_command cmd;
	cmd.callback = callback;
	cmd.data = z_data;
	xwrite(events.command_fd, &cmd, sizeof cmd);
}
```

I've chosen to have this in a separate function because I use it throughout the
code.

We referenced the "wakeup_handler" in the main function. Its implementation is
the following -- reading the pipe and calling the callback:

```c
void wakeup_handler(int fd, void *z_data)
{
	(void)z_data;
	struct wakeup_command cmd;
	xread(fd, &cmd, sizeof cmd);
	cmd.callback(cmd.data);
}
```

This is in its own function because this is a necessary abstraction much like
the comparison function in a sorting function. The "sorting function" in this
example is the generic event loop.

The next piece in the big puzzle is the domain lookup. The standard glibc API is
quite convoluted and so I've started writing some wrappers around it. Initially
I wasn't sure I'll be only resolving reddit.com because I had more grandiose
plans. I've cut those plans to keep this discussion shorter. So even though I do
hostname lookups at a single place, I still have an abstracted DNS lookup
resolver. Once I abstracted it out and it turned out that's not necessarily
needed I've decided to not inline it back because I'm lazy. Looking up DNS is a
well known problem, we won't lose any context from our application if we hide
its implementation anyways.

I wanted to have async DNS lookup where I could query a hostname and if the
hostname is in the cache then it returns immediately. In the other case it would
fire up the request in the background and notify me via a callback when the
hostname's entry is put into the cache. Initially I was thinking about an API
like this:

```c
bool host_lookup(const char *host, wakeup_callback_t z_cb, void *z_cb_data,
                 struct addrinfo **ai);
```

The usage would be the following: you pass in a hostname like "www.reddit.com"
in the host parameter. Then if the host is in the cache the result will be in
*ai and host_lookup returns true. If it isn't host_lookup schedules an async
lookup and returns false. When the lookup finishes, z_cb(z_zb_data) will be
scheduled to run unless z_cb is NULL.

But it bugged me that this function takes so many parameters that the line
itself needed to be wrapped. Above I was talking about the fact of not needing
to wrap your functions if you are very careful so I needed to do something. I've
actually decided to split this function into two parts:

```c
void host_lookup(const char *host, wakeup_callback_t z_cb, void *z_cb_data);
bool host_address(const char *host, struct addrinfo **ai);
```

Now host_address only checks the cache and returns false if it's not there,
otherwise returns true with *ai set to the result. host_lookup on the other hand
only enqueues the async lookup. It will only call the callback if the hostname
is not yet in the cache. I suppose it could return via bool whether the hostname
is already cached or not but in my case it's not necessary because I do a
host_address right after a host_lookup.

Note that how nicely we could split the two separate responsibilities into their
own functions. This comes at the cost that you now need to call two functions at
the callsite instead of one. But at least these two functions now need shorter
lines.

I'm not terribly happy with these two functions because they don't necessary
need to be two functions but I kept them as is because I haven't come up with a
nicer way of putting them while still retaining short lines.

I'll spare the implementation details because it is boring boilerplate anyways.

Now we have all the foundations to get to the highlight of the show, the event
handler for downloading the reddit page and starting the image downloads. What
exactly do we need to do that to achieve that goal? Here are the steps:

1. look up the DNS info for www.reddit.com (might block)
2. connect to www.reddit.com (might block)
3. send our query to reddit
4. read the reply (might block)
5. extract the links to images from the body
6. download the images (might block)

We will cheat in step 6 to keep things short: we'll outsource that part of the
logic to wget. But this gives us a nice demonstration that you can use
non-socket types of file descriptors in our event loop.

The above steps are fairly straightforward, you could write a nice long function
doing each step nicely in order. The problem is that some steps can be blocking
and if you block the main thread, no other event handlers can be processed. So
usually in such cases you split up the function into separate parts, one for
each step and whenever you might block, you set the callback to the next
function and configure the event loop to run that callback when we know that the
call won't block.

The problem with this approach that you will have many small functions scattered
throughout the source code making hard to follow your logic. So here's another
approach I propose. Write all your logic into one long function in such way that
it will handle all events. Let's say your logic has 3 states: A, B, C. Instead
of

```c
void A(int fd, void *data)
{
	...
}
void B(int fd, void *data)
{
	...
}
void C(int fd, void *data)
{
	...
}
```

you will have

```c
void F(int fd, void *data)
{
	<code for A>
	<code for B>
	<code for C>
}
```

The problem is that once you are in state B the code in A no longer applies. So
you need to skip it:

```c
void F(int fd, void *data)
{
	if (data->state == A) {
		<code for A>
	}
	if (data->state == B) {
		<code for B>
	}
	if (data->state == C) {
		<code for C>
	}
}
```

Now everything is in its own block and you can fairly easily read from top to
bottom. You don't have to scout the logic, it's all there in a single place.

Note that we are using these ifs like a glorified jump table or gotos forward. C
has another construct just for that, so let's try using that:

```c
void F(int fd, void *data)
{
	switch (data->state) {
	case A:
		<code for A>
	case B:
		<code for B>
	case C:
		<code for C>
	}
}
```

I prefer the latter form so I'll write the event handler in that form.

So let's start implementing the "getpics_handler". You configure it with a
subreddit and it will download the top 5 pictures into the current directory.
First, we will need a structure to hold the state of the handler:

```c
struct getpics_state {
	int state;
	char subreddit[64];
	wakeup_callback_t finish_callback;
	void *finish_callback_data;
};
```

We will add more state to this structure as we add more logic the handler
itself. Whenever you instantiate a getpics_state you will need to zero out its
contents because the code will assume this.

You have two tunables before starting the handler: subreddit and
finish_callback. Copy "funny" into the subreddit array if you want the top
pictures from /r/funny. finish_callback will be called after the handler
finishes. You can make your application quit after it finishes all the
downloading. I will skip the initial setup and cleanup parts of the code and
jump to handler itself. You can always look those up in the full source.

The handler starts simple:

```c
void getpics_handler(int fd, void *data)
{
	struct getpics_state *s = data;
	int r; // Used for return values from syscalls.
	switch (s->state) {
```

Now starts the juicy part:

```c
	case 0: {

		// Look up reddit's address asynchronously.
		host_lookup("www.reddit.com", getpics_wakeup, data);

		s->state += 1;
	} case 1: {

		// If we an ip address then open a connection to reddit.
		struct addrinfo *ai;
		if (!host_address("www.reddit.com", &ai))
			return;
		s->reddit_fd = socket(AF_INET, SOCK_STREAM|SOCK_NONBLOCK, 0);
		CHECK(s->reddit_fd > 0);
		r = connect(s->reddit_fd, ai->ai_addr, ai->ai_addrlen);
		CHECK(r == -1 && errno == EINPROGRESS);
		events_ctl(EPOLL_CTL_ADD, s->reddit_fd, getpics_handler, data);

		// We need to switch the fd to write polling to get notified.
		struct epoll_event e;
		e.events = EPOLLOUT;
		e.data.fd = s->reddit_fd;
		r = epoll_ctl(events.epoll_fd, EPOLL_CTL_MOD, s->reddit_fd, &e);
		CHECK(r == 0);

		s->state += 1;
```

host_lookup and host_address work as described above. We will keep retrying
host_address() until we don't have it in our DNS cache. The host_lookup will
ensure that it will eventually get into the cache. For simplicity we will assume
DNS requests always succeed and we have the corresponding CHECKS in the host_*
functions. In host_lookup you can see that we are passing the getpics_wakeup
function. All we really want is to retry this handler again but its function
prototype is differing so we made a wrapper function:

```c
void getpics_wakeup(void *data)
{
	getpics_handler(-1, data);
}
```

Sometimes we wake up via a callback rather than an event from an fd. We indicate
such cases with -1. It's not a problem because we will always double check the
preconditions as I'll describe later.

Note how simply we flow from state 0 to state 1 in the handler code: we don't
need to break, jump to another function -- all you need to is to read from top
to bottom. We can use comments to summarize what we are doing in each block.

A common practice is that whenever you feel to comment, create a new function
with a name similar to the comment as if the name of this function will
magically be "maintained" and up to date. In reality you will have very long
function names, with long list of arguments and a spaghetti code as explained in
a previous section. The free formness of the comments can be very handy. So I'm
recommending that whenever you feel commenting, comment. Just don't go overboard
with trivialities.

Back to the code: in state 1 we will look up reddit's address and open a
connection to their server. We issue an async connect() call for which the man
page says that you need to wait until the socket gets writable and then use
getsockopt() to read the connect()'s return value. So we will also switch the
socket to write polling. Note that we are using the state's reddit_fd member.
You will need to add this int to the state structure:

```c
struct getpics_state {
	int state;
	char subreddit[64];
	wakeup_callback_t finish_callback;
	void *finish_callback_data;

	int reddit_fd;
};
```

If the DNS lookup fails we quit the handler and we will sleep until somebody
wakes us up to retry. In this cause host_lookup will wake us up when it puts the
DNS result into its cache so the subsequent host_address calls will succeed.

Let's look at the next state:

```c
	} case 2: {

		// Wait until connected.
		if (fd != s->reddit_fd) {
			return;
		}
		int err = -1;
		socklen_t err_sz = sizeof err_sz;
		r = getsockopt(fd, SOL_SOCKET, SO_ERROR, &err, &err_sz);
		CHECK(err_sz == sizeof err && r == 0);
		CHECK(err == 0);

		// Connected, switch back to read polling.
		struct epoll_event e;
		e.events = EPOLLIN;
		e.data.fd = s->reddit_fd;
		r = epoll_ctl(events.epoll_fd, EPOLL_CTL_MOD, s->reddit_fd, &e);
		CHECK(r == 0);

		// Write the query.
		const char *sr = s->subreddit;
		char host[] = "Host: www.reddit.com";
		char user_agent[] = "User-Agent: test_bot/0.1";
		char fmt[] = "GET /r/%s/top.json HTTP/1.0\r\n%s\r\n%s\r\n\r\n";
		char q[1024];
		int len = snprintf(q, sizeof q, fmt, sr, host, user_agent);
		CHECK(len < (int)sizeof q);
		printf("Downloading /r/%s\n", sr);
		xwrite(s->reddit_fd, q, len);

		s->state += 1;
```

Here we demonstrate that in all states the first thing to do is to check is
whether we all have the prerequisites to get to the next state. In this case the
prerequisite is that the connect() call finished. There are several reasons it
might not yet finished: we come from the previous state directly so the
connect() call had no real chance to finish. Or maybe in the previous state the
host_address succeeded before the host_lookup's getpics_wakeup has scheduled so
we are actually woken up by getpics_wakeup which has absolutely no relation to
the connect() call. The main lesson here is that we can wake up for many reasons
so we need to always check the preconditions. In this case we need to check
whether a write to the socket would block or not. If we have been woken up
because of reddit_fd then that means it's ready so that's all we need to check
because in all other cases fd will be -1.

After the connect succeeded we switch back to read polling and we can now send
our HTTP request. The socket is nonblocking so the write always immediately
returns. I'm also assuming that the kernel's socket buffers are big enough to
hold the full request so I skip the error checking part here. I'd actually add
it if I had ever seen a partial write here.

Now we need to read the response:

```c
	} case 3: {

		// Read the response.
		int bufrem = BUFFER_SIZE - s->buffer_count;
		CHECK(bufrem > 0);
		int rby = read(s->reddit_fd, s->buffer+s->buffer_count, bufrem);
		if (rby == -1 && errno == EAGAIN) {
			return;
		}
		if (rby <= 0) {
			printf("Problem reading %s.\n", s->subreddit);
			goto error;
		}
		s->buffer_count += rby;

		// Check whether we have the HTTP header already.
		int header_length = s->buffer_count;
		if (header_length > 1024)
			header_length = 1024;
		char *hdr_end = memmem(s->buffer, header_length, "\r\n\r\n", 4);
		if (hdr_end == NULL) {
			if (header_length < 1024) {
				// We need more data, let's wait.
				return;
			} else {
				printf("HTTP headers too long.\n");
				goto error;
			}
		}
		hdr_end[3] = 0;
		s->http_header_length = hdr_end+4 - s->buffer;

		// Sanity check the headers.
		if (strstr(s->buffer, "HTTP/1.1 200 OK\r\n") != s->buffer) {
			printf("Non-OK response for %s.\n", s->subreddit);
			goto error;
		}
		if (strstr(s->buffer, "Connection: close\r\n") == NULL) {
			printf("Unexpected response for %s.\n", s->subreddit);
			goto error;
		}

		s->state += 1;
```

Note that we extended the getpics_state structure again with these three
members:

```c
	int buffer_count;
	char buffer[BUFFER_SIZE];
	int http_header_length;
```

In this state we will only read until we read the full HTTP headers. This is not
strictly needed anymore. Initially I started sending HTTP/1.1 requests and
thought that I'll need to parse the headers to determine the content's length.
But fortunately we are using HTTP/1.0 so its "Connection: close" makes things
easy to handle. Given that I've already separated the header parsing into its
separate state, I won't merge it back with the next state because I'm a bit lazy
and it's not too bad the way it is. I can actually do some header checks before
I continue reading the full contents: there might be cases I don't want to read
the contents at all so let's keep this possibilty open if it's implemented
already.

Note that I want to read just enough to get the headers and I'll keep reading
until I got it. I'm assuming that the header is at most 1024 bytes long and is
separated by 2 newlines as the HTTP standard requires. This seems to be true in
reddit's case so it's good enough for me.

There is another interesting thing you might have noticed by now. I don't use a
lot of empty lines. And whenever I use them, I'm starting a new
paragraph/thought so it is always followed by a comment to summarize what I'm
doing next. Doing this keeps the skimming pretty easy. It's important for a code
to be easily skimmable. The traditional way of using separate functions instead
of comments make it hard to skim through the code -- it's actually quite hard to
tell what the code really does just from its name so if you are interested in it
you have to follow the function and possibly lose some context while you are
jumping around.

Now let's read the contents part from the socket:

```c
	} case 4: {

		// Fully read the body -- until reddit closes the connection.
		int bufrem = BUFFER_SIZE - s->buffer_count;
		CHECK(bufrem > 0);
		int rby = read(s->reddit_fd, s->buffer+s->buffer_count, bufrem);
		if (rby == -1 && errno == EAGAIN) {
			return;
		}
		if (rby == -1) {
			printf("Problem reading %s.\n", s->subreddit);
			goto error;
		}
		s->buffer_count += rby;
		if (rby > 0) {
			// We haven't seen EOF yet, let's wait a bit.
			return;
		}
```

It's a standard "read until we have data" pattern. You might not get all the
data from a single read so this part might repeat a lot of times before you've
read everything. But when the read is finished, we don't need the socket
anymore:

```c
		// We have everything for offline processing, close the socket.
		events_ctl(EPOLL_CTL_DEL, s->reddit_fd, NULL, NULL);
		CHECK(close(s->reddit_fd) == 0);
		s->reddit_fd = 0;
```

You can see above and in some other places that we handle errors by just jumping
forward to the error handling code which is the following at the very end of the
function:

```c
error:
	if (s->reddit_fd != 0) {
		events_ctl(EPOLL_CTL_DEL, s->reddit_fd, NULL, NULL);
		CHECK(close(s->reddit_fd) == 0);
		s->reddit_fd = 0;
	}
	s->state = -1;
	s->finish_callback(s->finish_callback_data);
```

Now that we the full contents, we need to extract the jpg urls from it. We won't
get fancy here. Let's just look at all the "url": "<foobar.com/blah.jpg>" type
of pairs in the returned json via a simple strstr. And for the first 5 images
let's start up a wget command in the background. Let's take a look at the
current version of the getpics_state structure:

```c
enum { BUFFER_SIZE = 1 * 1024 * 1024 };
enum { MAX_RESULTS = 5 };
struct getpics_state {
	int state;
	char subreddit[64];
	wakeup_callback_t finish_callback;
	void *finish_callback_data;

	int buffer_count;
	char buffer[BUFFER_SIZE];

	int reddit_fd;
	int http_header_length;

	int wget_pipes_count;
	FILE *wget_pipes[MAX_RESULTS];
	char urls[MAX_RESULTS][128];
};
```

Notice the new variables wget_pipes and urls. We will gather the links into the
urls array and we'll open pipes for the wget commands via popen(). Here's how
all looks like:

```c
		// Extract the jpg links from the json and start the the wget
		// commands.
		bufrem = BUFFER_SIZE - s->buffer_count;
		CHECK(bufrem > 0);
		s->buffer[s->buffer_count] = 0;
		char *p = s->buffer + s->http_header_length;
		while ((p = strstr(p, "\"url\": ")) != NULL) {
			p += 8;
			char *e = strchr(p, '"');
			CHECK(e != NULL);
			*e = 0;
			if (e-p < 5 || strcmp(e-4, ".jpg") != 0) {
				p = e+1;
				continue;
			}
			// Disallow malicious input.
			CHECK(strchr(p, '\'') == NULL);
			int sz = sizeof s->urls[0];
			snprintf(s->urls[s->wget_pipes_count], sz, "%s", p);
			char cmd[128];
			r = snprintf(cmd, sizeof cmd, "wget -c -q '%s'", p);
			CHECK(r < (int) sizeof cmd);
			printf("executing: %s\n", cmd);
			FILE *f = popen(cmd, "w");
			CHECK(f != NULL);
			s->wget_pipes[s->wget_pipes_count++] = f;
			int pfd = fileno(f);
			CHECK(pfd > 0);
			int flags = fcntl(pfd, F_GETFL);
			CHECK(flags >= 0);
			CHECK(fcntl(pfd, F_SETFL, flags | O_ASYNC) == 0);
			events_ctl(EPOLL_CTL_ADD, pfd, getpics_handler, data);
			p = e+1;
			if (s->wget_pipes_count == MAX_RESULTS) {
				break;
			}
		}
		if (s->wget_pipes_count == 0) {
			puts("No images found.");
			s->state += 2;
			s->finish_callback(s->finish_callback_data);
			return;
		}

		s->state += 1;
		return;
```

Notice we return from this state. You'll see why in the next state:

```c
	} case 5: {

		// Wait until the wget commands finish.
		if (fd == -1) {
			return;
		}
		int idx;
		for (idx = 0; idx < s->wget_pipes_count; ++idx) {
			if (s->wget_pipes[idx] == NULL)
				continue;
			if (fileno(s->wget_pipes[idx]) == fd)
				break;
		}
		CHECK(idx < s->wget_pipes_count);
		events_ctl(EPOLL_CTL_DEL, fd, NULL, NULL);
		r = pclose(s->wget_pipes[idx]);
		CHECK(r >= 0);
		s->wget_pipes[idx] = NULL;
		printf("%s: ", s->urls[idx]);
		if (r > 0) {
			puts("error.");
		} else {
			puts("download succeeded.");
		}
		int active = 0;
		for (int i = 0; i < s->wget_pipes_count; ++i) {
			active += (s->wget_pipes[i] != NULL);
		}
		if (active > 0) {
			return;
		}

		// We are finished, quit.
		s->state += 1;
		s->finish_callback(s->finish_callback_data);
		return;
```

In this state we do actually care which file descriptor has woken up the event
handler. In state 4 we return before getting to state 5 so only an event on one
of the pipes can wake us up. Note that we can still potentially get woken up by
getpics_wakeup via host_lookup but let's just ignore that -- that's why you see
the "if (fd==-1) return;" logic.

And that's all there is to it. There's one little error handling to be done
though. Let's just abort if for some reason we get into a bad state:

```c
	} default: {
		CHECK(0);
	}
	};
```

And now you have a 225 line length function. It's quite long. See the full
source at the end for the full glory. But now it is your turn to look at that
full listing and determine whether the longer function is easier to read or not.
In fact as a practice I urge you to convert this code into a lot of small
functions and compare the two codes to see which ones you prefer.

Also note that I'm not the first person to think these thoughts. Initially I was
convinced that whenever I want to add a comment I shall make a new function. But
I was never really sure when to do so. Should I do that almost everywhere?
Should all my functions be only several lines of code? After a while a came
across some tips from Braid's creator, [Jonathan Blow on programming][blow].
This is one his tips: "Use straight-line code instead of function calls for
single instances." But the post which pushed me over to try this approach was
[John Carmack's essay][carmack] on inlined code. It's a fun read, go read it!

Now that I've actually tried this method I no longer feel obsessing about long
functions, in fact I'll try to write longer functions elsewhere as well -- if
they make sense of course.

[blow]: http://www.magicaltimebean.com/2011/07/the-specific-solution-thoughts-on-jonathan-blows-how-to-program-independent-games/
[carmack]: http://number-none.com/blow/john_carmack_on_inlined_code.html
