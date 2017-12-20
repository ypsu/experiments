#define _GNU_SOURCE
#include <netinet/ip.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

void *nil = (void *)0;
#define check(cond) checkimpl(cond, #cond, __FILE__, __LINE__)
void checkimpl(bool cond, const char *str, const char *file, int line) {
  if (cond) return;
  printf("checkfail: %s failed at %s:%d (errno: %m)\n", str, file, line);
  exit(1);
}

void SHA1(char *hash_out, const char *str, int len);
int Base64encode(char *encoded, const char *string, int len);

// maxgames is the number of games (aka rooms) that can be ongoing.
enum { maxgames = 10 };

// maxgameplayers is the number of players that can play a single game.
enum { maxgameplayers = 10 };

// maxplayers is the number of players that can simultaneously join the server.
enum { maxplayers = maxgames * maxgameplayers };

// maxplayerwords is the maximum number of words a player can see at any given
// moment.
enum { maxplayerwords = 9 };

// maxidlen is maximum length of an identifier, e.g. room or player name.
enum { maxidlen = 15 };

// maxwordlen is the maximum length of a word in bytes.
enum { maxwordlen = 31 };

// maxwords is the maximum number of words the server can hold in memory.
enum { maxwords = 1000 * 1000 };

// maxwordbuf is the maximum bytes the input can be.
enum { maxwordbuf = 10 * 1000 * 1000 };

// maxsocketbuf is maximum number of unprocessed bytes in a socket's stream.
enum { maxsocketbuf = 4095 };

// maxinputbuf is the maximum bytes a single client can send in a message.
enum { maxinputbuf = 127 };

// maxinputs is the maximum number of inputs the sockets code can buffer up
// before the processing logic takes over.
enum { maxinputs = 32 };

enum playerstate {
  playerstateunconnected,
  playerstatewaitingforhttpget,
  playerstatewaitingforname,
  playerstatewaitingforroom,
  playerstateplaying,
  playerstateclosed,
};

struct game;
struct player {
  char name[maxidlen + 1];
  enum playerstate playerstate;
  int socketfd;
  char socketbuf[maxsocketbuf + 1];
  int socketbufsize;
  // status is the message that the web client prints on the first line.
  char status[maxinputbuf + 1];
  // game is the game the player joined.
  struct game *game;
  // ready marks the player ready to start in a game's waiting room. the game
  // only starts when all waiting players are ready.
  bool ready;
  // words is the list of words that the player needs to get other players to
  // type. words[i] is an index into s.words and -1 represents an empty slot.
  int words[maxplayerwords];
  // wordstarts represent the starting time for each word. it is deciseconds
  // since the application's startup time.
  int wordstarts[maxplayerwords];
  // wordtimeouts are the timeouts in deciseconds for each word.
  int wordtimeouts[maxplayerwords];
  // points is the number of words the player typed from others.
  int points;
};

enum gamestate {
  gamestateempty,
  gamestatesetup,
  gamestaterunning,
  gamestateended,
};

// game represents a single ongoing game or lobby if the players didn't start
// the game yet.
struct game {
  // name of the room that appears in the lobby.
  char name[maxidlen + 1];
  enum gamestate gamestate;
  // players are the players in this room. empty slots are nils and can be
  // interspersed with the players in the array.
  struct player *players[maxgameplayers];
  // words is the number of words that each player sees on their interface. must
  // be between 2 and 9.
  int wordscount;
  // minsecs and maxsecs represent the minimum and maximum timeout for each
  // word. the game picks the actual time for each word randomly between these
  // two numers. the players must enter the word within the allotted time.
  int minsecs, maxsecs;
  // totalwords is the total number of words the players have to type in.
  int totalwords;
  // these variables point to the word and player due which the game is over.
  struct player *losingplayer;
  int losingword;
};

// s contains all global state. note that empty player and game structures are
// all zeros.
struct state {

  // wordbuf contains the whole input with 0 terminators instead of newlines.
  char wordbuf[maxwordbuf];

  // words points into wordbuf that contains the 0 terminated words.
  int wordscount;
  const char *words[maxwords];

  struct player players[maxplayers];
  struct game games[maxgames];

  // these structures hold the input data read from sockets. it's has its
  // separate buffers so that the socket code doesn't need to interpret these.
  int inputscount;
  // inputplayers tells which player sent the specific input.
  struct player *inputplayers[maxinputs];
  char inputbufs[maxinputs][maxinputbuf + 1];

  char typeshoutresponse[maxsocketbuf + 1];
  int typeshoutresponselen;
} s;

const char websocketresponse[] =
  "HTTP/1.1 101 Switching Protocols\r\n"
  "Upgrade: websocket\r\n"
  "Connection: Upgrade\r\n"
  "Sec-WebSocket-Accept: ";
const char websocketkeyheader[] = "\r\nSec-WebSocket-Key: ";
const char notfoundresponse[] =
  "HTTP/1.1 404 Not Found\r\n"
  "Content-Type: text/plain; charset=utf-8\r\n"
  "\r\n"
  "404 Not Found\n";
const char overloadedresponse[] =
  "HTTP/1.1 503 Service Unavailable\r\n"
  "Content-Type: text/plain; charset=utf-8\r\n"
  "\r\n"
  "503 service overloaded\n";

int main(int argc, char **argv) {
  // single responsibility variables below.
  int epollfd, serverfd;
  int eventscount;
  struct epoll_event events[20];
  // starttimeval is the application's start time.
  struct timeval starttimeval;
  // ctimeds is the current time in deciseconds since application startup.
  int ctimeds;

  // helper variables below.
  int i, j, k, fd, r, len, opcode, id, remtime, opt;
  bool ok, wantsid;
  char *pbuf, *pbuf2;
  int rembuf;
  int lastframeend;
  int framelen;
  int maskingkey;
  struct timeval timeval;
  struct epoll_event ev, *evp;
  struct player *player, *playerit;
  struct game *game, *gameit;
  char buf[maxsocketbuf];
  char websocketkey[100];
  char websocketkeyhash[100];
  FILE *file;
  int serverport;

  // initialize a few variables.
  srand(time(nil));
  gettimeofday(&starttimeval, nil);
  file = stdin;
  serverport = 9080;
  while ((opt = getopt(argc, argv, "w:p:")) != -1) {
    switch (opt) {
    case 'w':
      file = fopen(optarg, "r");
      check(file != NULL);
      break;
    case 'p':
      serverport = atoi(optarg);
      check(0 < serverport && serverport <= 65535);
      break;
    default:
      printf("usage: typeshout -p portnum -w wordfile\n");
      exit(1);
    }
  }

  // read the words.
  if (file == stdin) puts("reading the words from stdin");
  else puts("reading the words from the input file");
  pbuf = s.wordbuf;
  rembuf = maxwordbuf;
  while (fgets(pbuf, rembuf, file) != nil) {
    if (s.wordscount == maxwords) {
      printf("error: too many words on input, max is %d\n", maxwords);
      exit(1);
    }
    len = strlen(pbuf);
    if (pbuf[0] == 0 || pbuf[0] == '\n') {
      printf("error: the input cannot contain empty lines.\n");
      exit(1);
    }
    if (pbuf[len - 1] == '\n') {
      pbuf[--len] = 0;
    }
    if (len > maxwordlen) {
      printf("error: word %s is too long, max len is %d\n", pbuf, maxwordlen);
      exit(1);
    }
    s.words[s.wordscount++] = pbuf;
    pbuf += len + 1;
    rembuf -= len + 1;
  }
  if (s.wordscount == 0) {
    puts("error: couldn't read a single word from input");
    exit(1);
  }
  if (file != stdin) check(fclose(file) == 0);

  // read typeshout.html.
  pbuf = s.typeshoutresponse;
  pbuf += sprintf(pbuf, "HTTP/1.1 200 OK\r\n");
  pbuf += sprintf(pbuf, "Content-Type: text/html; charset=utf-8\r\n");
  pbuf += sprintf(pbuf, "\r\n");
  file = fopen("typeshout.html", "r");
  check(file != nil);
  len = fread(pbuf, 1, maxsocketbuf - 100, file);
  pbuf += len;
  len = fread(pbuf, 1, 1, file);
  check(len == 0);
  *pbuf = 0;
  s.typeshoutresponselen = pbuf - s.typeshoutresponse;
  check(fclose(file) == 0);

  // start the server.
  epollfd = epoll_create1(0);
  check(epollfd != -1);
  serverfd = socket(AF_INET, SOCK_STREAM, 0);
  check(serverfd != -1);
  i = 1;
  check(setsockopt(serverfd, SOL_SOCKET, SO_REUSEADDR, &i, sizeof(int)) == 0);
  struct sockaddr_in serveraddr;
  serveraddr.sin_family = AF_INET;
  serveraddr.sin_port = htons(serverport);
  serveraddr.sin_addr.s_addr = INADDR_ANY;
  check(bind(serverfd, &serveraddr, sizeof(serveraddr)) == 0);
  check(listen(serverfd, 100) == 0);
  ev.events = EPOLLIN;
  ev.data.fd = serverfd;
  check(epoll_ctl(epollfd, EPOLL_CTL_ADD, serverfd, &ev) == 0);
  printf("started server on port %d\n", serverport);

  // run the event loop.
  while (true) {
    s.inputscount = 0;
    eventscount = sizeof(events) / sizeof(events[0]);
    eventscount = epoll_wait(epollfd, events, eventscount, 100);
    check(eventscount >= 0);
    gettimeofday(&timeval, nil);
    timersub(&timeval, &starttimeval, &timeval);
    ctimeds = timeval.tv_sec * 10 + timeval.tv_usec / 100000;
    for (int ei = 0; s.inputscount < maxinputs && ei < eventscount; ei++) {
      evp = &events[ei];

      // process new connections.
      if (evp->data.fd == serverfd) {
        fd = accept4(serverfd, nil, nil, SOCK_NONBLOCK);
        check(fd > 0);
        for (i = 0; i < maxplayers; i++) {
          player = &s.players[i];
          if (player->playerstate != playerstateunconnected) continue;
          player->playerstate = playerstatewaitingforhttpget;
          player->socketfd = fd;
          ev.events = EPOLLIN;
          ev.data.ptr = player;
          check(epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &ev) == 0);
          break;
        }
        if (i != maxplayers) continue;
        len = strlen(overloadedresponse);
        check(write(fd, overloadedresponse, len) == len);
        check(close(fd) == 0);
        continue;
      }

      // process input from players.
      player = evp->data.ptr;
      rembuf = maxsocketbuf - player->socketbufsize;
      pbuf = player->socketbuf;
      r = read(player->socketfd, pbuf + player->socketbufsize, rembuf);
      if (r <= 0) {
        r = 0;
        player->playerstate = playerstateclosed;
      }
      player->socketbufsize += r;
      if (player->playerstate == playerstatewaitingforhttpget) {
        // accept a new websocket connection by doing the handshake dance.
        if (player->socketbufsize < 4) continue;
        ok = true;
        if (memcmp(pbuf + player->socketbufsize - 4, "\r\n\r\n", 4) != 0) {
          if (player->socketbufsize != maxsocketbuf) continue;
          ok = false;
        }
        // serve 404 for unknown pages.
        if (ok && memcmp(pbuf, "GET ", 4) != 0) ok = false;
        if (ok && sscanf(pbuf + 4, "%100s", buf) != 1) ok = false;
        if (ok && strcmp(buf, "/typeshout") == 0) {
          len = s.typeshoutresponselen;
          check(write(player->socketfd, s.typeshoutresponse, len) == len);
          check(close(player->socketfd) == 0);
          memset(player, 0, sizeof(*player));
          continue;
        }
        if (ok && strcmp(buf, "/typeshoutclient") != 0) ok = false;
        // compute the silly websocketkey and respond. people should not be
        // allowed to design protocols under the influence of drugs.
        if (ok) {
          len = strlen(websocketkeyheader);
          pbuf = memmem(pbuf, player->socketbufsize, websocketkeyheader, len);
          if (pbuf == nil) {
            ok = false;
          }
        }
        if (ok) {
          pbuf += len;
          len = player->socketbuf + player->socketbufsize - pbuf;
          pbuf2 = memchr(pbuf, '\r', len);
          if (pbuf == nil) {
            ok = false;
          }
        }
        if (ok) {
          *pbuf2 = 0;
          if (pbuf2 - pbuf >= 30) {
            ok = false;
          }
        }
        if (!ok) {
          len = strlen(notfoundresponse);
          check(write(player->socketfd, notfoundresponse, len) == len);
          check(close(player->socketfd) == 0);
          memset(player, 0, sizeof(*player));
          continue;
        }
        strcpy(websocketkey, pbuf);
        strcat(websocketkey, "258EAFA5-E914-47DA-95CA-C5AB0DC85B11");
        SHA1(websocketkeyhash, websocketkey, strlen(websocketkey));
        Base64encode(websocketkey, websocketkeyhash, 20);

        pbuf = (char *)websocketresponse;
        pbuf2 = websocketkey;
        len = snprintf(buf, maxsocketbuf, "%s%s\r\n\r\n", pbuf, pbuf2);
        check(len < maxsocketbuf - 1);
        r = write(player->socketfd, buf, len);
        check(r == len);
        player->playerstate = playerstatewaitingforname;
        player->socketbufsize = 0;
        strcpy(player->status, "Please enter your name!");
        continue;
      }

      // process all buffered websocket frames. buffer at most maxinputs number
      // of frames.
      lastframeend = 0;
      while (s.inputscount < maxinputs) {
        len = player->socketbufsize - lastframeend;
        if (len < 2) break;
        pbuf = player->socketbuf + lastframeend;
        r = (unsigned char)pbuf[0];
        if ((r & 0x80) == 0) {
          player->playerstate = playerstateclosed;
          continue;
        }
        if ((r & 0x70) != 0) {
          player->playerstate = playerstateclosed;
          continue;
        }
        opcode = r & 0x0f;
        if (opcode == 1) {
        } else if (opcode == 8) {
          player->playerstate = playerstateclosed;
        } else {
          player->playerstate = playerstateclosed;
          continue;
        }
        if (len < 6) break;
        framelen = (unsigned char)pbuf[1];
        if ((framelen & 0x80) == 0) {
          player->playerstate = playerstateclosed;
          continue;
        }
        framelen &= 0x7f;
        if (framelen >= 126) {
          player->playerstate = playerstateclosed;
          continue;
        }
        memcpy(&maskingkey, pbuf + 2, 4);
        if (len < 6 + framelen) break;
        memcpy(buf, pbuf + 6, framelen);
        for (i = 0; i < framelen; i += 4) {
          *(int *)(buf + i) ^= maskingkey;
        }
        buf[framelen] = 0;
        s.inputplayers[s.inputscount] = player;
        strcpy(s.inputbufs[s.inputscount], buf);
        s.inputscount++;
        lastframeend += 6 + framelen;
      }
      // eat all the processed bytes from the socketbuf.
      len = player->socketbufsize - lastframeend;
      if (lastframeend > 0 && len != 0) {
        memmove(player->socketbuf, player->socketbuf + lastframeend, len);
      }
      player->socketbufsize -= lastframeend;
    }

    // advance the game's state based on the player inputs.
    for (i = 0; i < s.inputscount; i++) {
      player = s.inputplayers[i];
      pbuf = s.inputbufs[i];
      len = strlen(pbuf);
      // on initial connection the player needs to enter a name and select a
      // room. verify that they choose sane identifiers.
      wantsid = player->playerstate == playerstatewaitingforname;
      if (!wantsid) {
        wantsid = player->playerstate == playerstatewaitingforroom;
      }
      if (wantsid) {
        if (len < 2) {
          strcpy(player->status, "Please use a longer name, at least 2 chars.");
          continue;
        }
        if (len > maxidlen) {
          const char fmt[] = "Please use shorter name! Max length is %d.";
          sprintf(player->status, fmt, maxidlen);
          continue;
        }
        ok = true;
        for (j = 0; ok && j < len; j++) {
          int ch = pbuf[j];
          ok = ('a' <= ch && ch <= 'z') || ('0' <= ch && ch <= '9');
        }
        if (!ok) {
          sprintf(player->status, "Use only [a-z0-9]*!");
          continue;
        }
      }
      if (player->playerstate == playerstatewaitingforname) {
        // record the player's name.
        ok = true;
        for (j = 0; ok && j < maxplayers; j++) {
          playerit = &s.players[j];
          if (player == playerit) continue;
          ok = strcmp(playerit->name, pbuf) != 0;
        }
        if (!ok) {
          strcpy(player->status, "Name taken, choose another.");
          continue;
        }
        strcpy(player->name, pbuf);
        const char fmt[] = "Welcome %s! Select or create a room!";
        sprintf(player->status, fmt, player->name);
        player->playerstate = playerstatewaitingforroom;
        continue;
      }
      if (player->playerstate == playerstatewaitingforroom) {
        // put the player into the room they selected.
        game = nil;
        for (j = 0; j < maxgames; j++) {
          gameit = &s.games[j];
          if (gameit->gamestate == gamestateempty) {
            if (game == nil) {
              game = gameit;
            }
            continue;
          }
          if (strcmp(gameit->name, pbuf) != 0) continue;
          game = gameit;
          break;
        }
        if (game == nil) {
          strcpy(player->status, "Sorry, no space for a new room. ");
          strcat(player->status, "Please come back later.");
          continue;
        }
        if (game->gamestate == gamestateempty) {
          // create the room for the player if it doesn't exist yet.
          strcpy(game->name, pbuf);
          game->gamestate = gamestatesetup;
          game->wordscount = 2;
          game->minsecs = 15;
          game->maxsecs = 25;
          game->totalwords = 10;
        }
        if (game->gamestate == gamestaterunning) {
          strcpy(player->status, "That game is ongoing. Choose another.");
          continue;
        }
        if (game->gamestate == gamestateended) {
          strcpy(player->status, "That game is over. Choose another.");
          continue;
        }
        check(game->gamestate == gamestatesetup);
        for (j = 0; j < maxgameplayers && game->players[j] != 0; j++);
        if (j == maxgameplayers) {
          strcpy(player->status, "Sorry, that room is full. Choose another.");
          continue;
        }
        player->playerstate = playerstateplaying;
        player->game = game;
        player->ready = false;
        player->points = 0;
        game->players[j] = player;
        sprintf(player->status, "Joined %s.", game->name);
        continue;
      }
      if (player->playerstate != playerstateplaying) continue;
      game = player->game;
      if (game->gamestate == gamestatesetup) {
        // process the commands in the waiting room.
        if (strcmp(pbuf, "back") == 0) {
          sprintf(player->status, "Left %s.", game->name);
          for (j = 0; j < maxgameplayers && game->players[j] != player; j++);
          check(j != maxgameplayers);
          game->players[j] = nil;
          player->game = nil;
          player->playerstate = playerstatewaitingforroom;
          continue;
        }
        if (strcmp(pbuf, "ready") == 0) {
          strcpy(player->status, "Ready for action.");
          player->ready = true;
          continue;
        }
        if (strcmp(pbuf, "wait") == 0) {
          strcpy(player->status, "Holding off the game's start.");
          player->ready = false;
          continue;
        }
        if (sscanf(pbuf, "words %d", &r) == 1) {
          if (r <= 0 || r > maxplayerwords) {
            const char fmt[] = "words must be between 1 and %d.";
            sprintf(player->status, fmt, maxplayerwords);
            continue;
          }
          sprintf(player->status, "Changed words to %d.", r);
          if (r == game->wordscount) continue;
          game->wordscount = r;
          goto notifyplayersaboutmodification;
        }
        if (sscanf(pbuf, "minsecs %d", &r) == 1) {
          if (r <= 1 || r > game->maxsecs) {
            strcpy(player->status, "minsecs must be between 2 and maxsecs.");
            continue;
          }
          sprintf(player->status, "Changed minsecs to %d.", r);
          if (r == game->minsecs) continue;
          game->minsecs = r;
          goto notifyplayersaboutmodification;
        }
        if (sscanf(pbuf, "maxsecs %d", &r) == 1) {
          if (r < game->minsecs || r > 100) {
            strcpy(player->status, "maxsecs must be between minsecs and 100.");
            continue;
          }
          sprintf(player->status, "Changed maxsecs to %d.", r);
          if (r == game->maxsecs) continue;
          game->maxsecs = r;
          goto notifyplayersaboutmodification;
        }
        if (sscanf(pbuf, "total %d", &r) == 1) {
          if (r < 0 || r > 1000) {
            strcpy(player->status, "total must be between 0 and 1000.");
            continue;
          }
          sprintf(player->status, "Changed total to %d.", r);
          if (r == game->totalwords) continue;
          game->totalwords = r;
          goto notifyplayersaboutmodification;
        }
        strcpy(player->status, "Command error.");
        continue;
notifyplayersaboutmodification:
        for (j = 0; j < maxgameplayers; j++) {
          playerit = game->players[j];
          if (playerit == nil) continue;
          playerit->ready = false;
          if (playerit != player) {
            sprintf(playerit->status, "%s changed the rules.", player->name);
          }
        }
        continue;
      }
      if (game->gamestate == gamestateended) {
        // after a match the game shows the scoreboard. to go back to the room
        // selection screen they have to type in back.
        if (strcmp(pbuf, "back") != 0) continue;
        sprintf(player->status, "Choose the next room!");
        for (j = 0; j < maxgameplayers && game->players[j] != player; j++);
        check(j != maxgameplayers);
        game->players[j] = nil;
        player->game = nil;
        player->playerstate = playerstatewaitingforroom;
        continue;
      }
      check(game->gamestate == gamestaterunning);
      // here comes the game's heart. advance the game's state based on player
      // input. see if the typed word matches words of other players and award a
      // point if so.
      ok = false;
      for (j = 0; j < maxgameplayers; j++) {
        playerit = game->players[j];
        if (playerit == nil || playerit == player) continue;
        for (k = 0; k < maxplayerwords; k++) {
          id = playerit->words[k];
          if (id == -1) continue;
          if (strcmp(s.words[id], pbuf) != 0) continue;
          player->points++;
          sprintf(player->status, "Typed %s's word.", playerit->name);
          ok = true;
          playerit->words[k] = -1;
          if (game->totalwords == 0) continue;
          playerit->words[k] = rand() % s.wordscount;
          playerit->wordstarts[k] = ctimeds;
          len = game->maxsecs - game->minsecs + 1;
          playerit->wordtimeouts[k] = (rand() % len + game->minsecs) * 10;
          game->totalwords--;
        }
      }
      if (!ok) {
        strcpy(player->status, "No match for that word.");
      }
    }
    s.inputscount = 0;

    // deal with disconnected players.
    for (i = 0; i < maxplayers; i++) {
      player = &s.players[i];
      if (player->playerstate != playerstateclosed) continue;
      // send back a close frame on the websocket as the protocol requires.
      buf[0] = 0x88;
      buf[1] = 0;
      write(player->socketfd, buf, 2);
      check(close(player->socketfd) == 0);
      // clean up the player from the game.
      game = player->game;
      if (game == nil) goto playercleanup;
      for (j = 0; j < maxplayers && game->players[j] != player; j++);
      check(j != maxplayers);
      game->players[j] = nil;
      if (game->gamestate == gamestatesetup) {
        goto playercleanup;
      }
      // stop the game if an actual player disconnected.
      game->gamestate = gamestateempty;
      for (j = 0; j < maxgameplayers; j++) {
        playerit = game->players[j];
        if (playerit == nil) continue;
        const char fmt[] = "%s disconnected, game stopped.";
        sprintf(playerit->status, fmt, player->name);
        playerit->game = nil;
        if (playerit->playerstate != playerstateclosed) {
          playerit->playerstate = playerstatewaitingforroom;
        }
      }
      memset(game, 0, sizeof(*game));
playercleanup:
      memset(player, 0, sizeof(*player));
    }

    // garbage collect empty rooms. these appear after all players leave the
    // room.
    for (i = 0; i < maxgames; i++) {
      game = &s.games[i];
      if (game->gamestate == gamestateempty) continue;
      ok = false;
      for (j = 0; j < maxgameplayers; j++) {
        if (game->players[j] != nil) {
          ok = true;
          break;
        }
      }
      if (!ok) {
        memset(game, 0, sizeof(*game));
      }
    }

    // start a game if all players are ready and the game is not running yet.
    for (i = 0; i < maxgames; i++) {
      game = &s.games[i];
      if (game->gamestate != gamestatesetup) continue;
      ok = true;
      r = 0;
      for (j = 0; ok && j < maxgameplayers; j++) {
        player = game->players[j];
        if (player == nil) continue;
        ok = player->ready;
        r += 1;
      }
      if (!ok || r <= 1) continue;
      game->gamestate = gamestaterunning;
      for (j = 0; j < maxgameplayers; j++) {
        player = game->players[j];
        if (player == nil) continue;
        memset(player->words, -1, sizeof(player->words));
        strcpy(player->status, "Game started. ");
        strcat(player->status, "Ask other players to type the words below.");
        for (k = 0; k < game->wordscount; k++) {
          if (game->totalwords == 0) continue;
          game->totalwords--;
          player->words[k] = rand() % s.wordscount;
          player->wordstarts[k] = ctimeds;
          player->wordtimeouts[k] = game->maxsecs * 10;
        }
      }
    }

    // end the game if there is a stale word or there are no words left.
    for (i = 0; i < maxgames; i++) {
      game = &s.games[i];
      if (game->gamestate != gamestaterunning) continue;
      // ok will be true if there's at least on player with a word active.
      ok = false;
      for (j = 0; j < maxgameplayers; j++) {
        player = game->players[j];
        if (player == nil) continue;
        for (k = 0; k < maxplayerwords; k++) {
          id = player->words[k];
          if (id == -1) continue;
          ok = true;
          remtime = player->wordstarts[k] + player->wordtimeouts[k] - ctimeds;
          if (remtime >= 0) continue;
          game->losingplayer = player;
          game->losingword = id;
          goto lostgame;
        }
      }
      if (ok) continue;
      game->gamestate = gamestateended;
      continue;
lostgame:
      game->gamestate = gamestateended;
      for (j = 0; j < maxgameplayers; j++) {
        player = game->players[j];
        if (player == nil) continue;
        strcpy(player->status, "Game ended. Type 'back' to start another.");
      }
    }

    // update the players' screen.
    for (i = 0; i < maxplayers; i++) {
      player = &s.players[i];
      if (player->playerstate == playerstateunconnected) continue;
      if (player->playerstate == playerstatewaitingforhttpget) continue;
      // reserve 4 bytes for the header because the frame's size is unknown at
      // this point.
      pbuf = buf + 4;
      pbuf += sprintf(pbuf, "%s\n", player->status);
      if (player->playerstate == playerstatewaitingforroom) {
        pbuf += sprintf(pbuf, "Available rooms:\n");
        for (j = 0; j < maxgames; j++) {
          game = &s.games[j];
          if (game->gamestate == gamestateempty) continue;
          pbuf += sprintf(pbuf, "  %s ", game->name);
          if (game->gamestate == gamestatesetup) {
            pbuf += sprintf(pbuf, " (setup)\n");
          } else if (game->gamestate == gamestaterunning) {
            pbuf += sprintf(pbuf, " (running)\n");
          } else if (game->gamestate == gamestateended) {
            pbuf += sprintf(pbuf, " (ended)\n");
          }
        }
      } else if (player->playerstate == playerstateplaying) {
        game = player->game;
        check(game != nil);
        if (game->gamestate == gamestatesetup) {
          pbuf += sprintf(pbuf, "Players:\n");
          for (j = 0; j < maxgameplayers; j++) {
            playerit = game->players[j];
            if (playerit == nil) continue;
            pbuf2 = playerit->ready ? "" : "not ";
            pbuf += sprintf(pbuf, "  %s (%sready)\n", playerit->name, pbuf2);
          }
          pbuf += sprintf(pbuf, "Available commands:\n");
          pbuf += sprintf(pbuf, "  back: go back to room selection\n");
          pbuf += sprintf(pbuf, "  ready: mark readiness\n");
          pbuf += sprintf(pbuf, "  wait: unmark readiness\n");
          const char fmt1[] = "  words x: number of words to display (%d)\n";
          pbuf += sprintf(pbuf, fmt1, game->wordscount);
          const char fmt2[] = "  minsecs x: minimum time for entry (%d)\n";
          pbuf += sprintf(pbuf, fmt2, game->minsecs);
          const char fmt3[] = "  maxsecs x: maximum time for entry (%d)\n";
          pbuf += sprintf(pbuf, fmt3, game->maxsecs);
          const char fmt4[] = "  total: total number of words to win (%d)\n";
          pbuf += sprintf(pbuf, fmt4, game->totalwords);
        } else if (game->gamestate == gamestaterunning) {
          pbuf += sprintf(pbuf, "%d words remaining:\n", game->totalwords);
          for (j = 0; j < maxplayerwords; j++) {
            id = player->words[j];
            if (id == -1) continue;
            remtime = player->wordstarts[j] + player->wordtimeouts[j] - ctimeds;
            pbuf += sprintf(pbuf, "  %2dds %s\n", remtime, s.words[id]);
          }
        } else if (game->gamestate == gamestateended) {
          if (game->losingplayer != nil) {
            const char fmt[] = "Game lost because nobody typed %s's '%s'.";
            pbuf2 = (char *)s.words[game->losingword];
            pbuf += sprintf(pbuf, fmt, game->losingplayer->name, pbuf2);
            goto sendsocketframe;
          }
          pbuf += sprintf(pbuf, "Your team won the game. Congrats! Scores:\n");
          for (j = 0; j < maxgameplayers; j++) {
            playerit = game->players[j];
            if (playerit == nil) continue;
            const char fmt[] = "  %3d %s\n";
            pbuf += sprintf(pbuf, fmt, playerit->points, playerit->name);
          }
          pbuf += sprintf(pbuf, "The score is the number of words typed.\n");
        } else {
          check(false);
        }
      }
sendsocketframe:
      len = pbuf - buf - 4;
      pbuf = buf + 3;
      if (len >= 126) {
        check(len < 30000);
        *pbuf-- = len & 0xff;
        *pbuf-- = len >> 8;
        *pbuf-- = 126;
        len += 4;
      } else {
        *pbuf-- = len;
        len += 2;
      }
      *pbuf = 0x81;
      r = write(player->socketfd, pbuf, len);
      if (r != len) {
        player->playerstate = playerstateclosed;
        continue;
      }
    }
  }
  return 0;
}
