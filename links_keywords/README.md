# Keywords in links

Keyword search functionality is something I miss in [links][wiki]. I've decided
to change this state of affairs by creating a patch. So now when you press 'g'
to pop up the goto url window, you can enter "w web browser" and you get the web
browser's wikipedia article.

This works by parsing the ~/.links/keywords.txt file and looking for a matching
entry when you enter a new url. Each line in this file must contain two space
separated tokens. One for the keyword and one for the prefix. If beginning of
the goto url matches one of the keywords then the rest of the url will be
prefixed as described in the file.

As an example here's my ~/.links/keywords.txt:

```
gm https://www.gmail.com/
d https://www.duckduckgo.com/lite/?q=
w https://en.wikipedia.org/w/index.php?search=
i https://www.google.com/search?hl=en&tbm=isch&q=
y https://www.youtube.com/results?search_query=
g https://www.google.com/search?hl=en&num=25&q=
t http://szotar.sztaki.hu/search?fromlang=all&tolang=all&searchWord=
```

The patch is against version 2.7.

[wiki]: https://en.wikipedia.org/wiki/Links_(web_browser)
