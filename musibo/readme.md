# musibo

this is just a simple audio playing tool.
i wrote this for myself out of frustration,
because youtube, spotify and other websites are full of ads,
and it's very hard to get them to play what i want them to play.
those stupid ads can take one out of the mood very quickly,
especially a moody kid when you are trying to play children songs.

i have speakers in the living room,
i want to be able to connect a computer or smartphone to it,
and have it play what i want it to play without interruption.

so i made this little tool:
it lists the music files you have in the computer,
displays them,
lets you filter to the stuff you are in the mood,
optionally shuffle things,
and just plays your selection without interruption.

you need three things:
first, generate the list of music files you care about.
second, host musibo.html and those music files with a http server.
third, set up your router to point the url "music" to your http computer.
this way it will be super easy to play your music from any device you can find.

assuming the music is in the current directory,
i use the following script to start the server (an example):

```
#!/bin/bash
find >musibo.data \
  abba \
  beatles \
  kidsongs \
  -iname '*.mp3' -o -iname '*.flac'
exec bindcap thttpd -d . -p 80 -l /dev/stdout -D
```

basically i list all the music directories that i want to be available.
one could just dump all music files here, but i didn't want that.

then i have index.html symlinked to musibo.html.
and i use thttpd as the web server.
that bindcap tool is just a simple wrapper,
that lets thttpd open port 80.
more details about that at https://github.com/ypsu/notech/blob/master/bindcap.c.
