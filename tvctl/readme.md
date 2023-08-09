# tvctl

a silly service that starts automatically on my tv
and can start play tv shows if certain challenges are met.

the tv shows must be structured into directories.
the name of the directory is the pass-phrase for starting that show.
the server looks for the next episode to play in a subdirectory in the show's directory.
to mark a show as "seen", it moves the episode to be directly in the show's directory.

suppose you have the following shows:

  tvshows/kidcartoon/next/s01e03.mkv
  tvshows/kidcartoon/next/s01e04.mkv
  tvshows/kidcartoon/s01e01.mkv
  tvshows/kidcartoon/s01e02.mkv

the next episode it will play will be s01e03.mkv after which it will move the episode one directory higher.
