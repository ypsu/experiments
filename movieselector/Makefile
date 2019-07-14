all: selector poweroff-on-inactivity display-poster

selector: selector.c
	gcc -g -std=c99 -Wall -Wextra selector.c -o selector

poweroff-on-inactivity: poweroff-on-inactivity.c
	gcc -g -std=c99 -Wall -Wextra poweroff-on-inactivity.c -o poweroff-on-inactivity -lX11 -lXss

display-poster: display-poster.c
	gcc -g -std=c99 -Wall -Wextra display-poster.c -o display-poster -lSDL
