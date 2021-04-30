#!/usr/bin/gnuplot

set terminal pngcairo size 1280,720 font "Noto Sans"
set output "plot.png"
set title "focus reach"
set format x "%Y-%b"
set xlabel "date"
set ylabel "millimeters"
set y2label "diopters"
set key below
tfmt = "%Y%m%d-%H%M"
set timefmt tfmt
set xdata time
set autoscale xfixmin
set autoscale xfixmax
set y2tics (\
  "-3.75D" 267, \
  "-4.00D" 250, \
  "-4.25D" 235, \
  "-4.5.D" 222, \
  "-4.75D" 210, \
  "-5.00D" 200, \
  "-5.25D" 190)
set link y2
plot "<awk -f avg data" using 1:2 with lines smooth acsplines lw 5 title "last month's average", \
     "data" using 1:2 pt 7 ps 1 title "measurements", \
     235 notitle, 222 notitle, 210 notitle, 200 notitle
