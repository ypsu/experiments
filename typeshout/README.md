# typeshout

This is a silly, text based multiplayer game over websockets. Spacewar was its
original inspiration but I heavily simplified the gameplay. All players must
reside within shouting distance for the game to make sense. Once the players
connect to a server, they join a common lobby, negotiate the game settings and
once every marks itself as ready, the game commences.

Each player sees a list of words that other players have to type. A player
cannot type its own words. If nobody types a player's word in the alloted time
the whole group loses the game. The game ends when the players type in a given
amount of words correctly.

![screenshot](/sshot.png?raw=true)

## Usage

First build and start the server:

```
./build
./typeshout -w eng.txt -p 8080
```

then ask your players to navigate to http://yourserver:8080/typeshout. There are
no other dependencies for this game other than this single server. To build all
you need is a linux with a clang or gcc. No library dependencies except for
glibc.

## Notes

I did this for a silly little thought experiment about writing code in using C
without using helper functions. All it has is a single massive main function
littered with goto jumps (strictly forward ones, that is). I think the code
turned out quite alright, gotos are not that bad.
