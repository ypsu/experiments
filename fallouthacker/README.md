# Fallout 3/4 hacker tool

The games called Fallout 3 and Fallout 4 have a hacking minigame which is quite
hard for me to solve by hand, so I wrote a solver to alleviate the pain.
Although there do exist solvers on the internet like
http://hackfallout.analogbit.com but at the time of writing this document it did
not solve the challenges in an optimal way. For example the game challenged me
for

```
    arts find prod back grow held take used vats work bids made frag raid
```

and that solver solved it for `raid` in 5 steps although the game only gives you
4 attempts. I was curious whether this was solvable 4 steps and sounded like an
interesting problem so I wrote my own backtracking/brute force solver. This
solver's UI is heavily inspired by the above solver one's modulo nice background
and pictures.

It is available here: [fallouthacker.html][link].

[link]: https://rawgit.com/ypsu/experiments/master/fallouthacker/fallouthacker.html
