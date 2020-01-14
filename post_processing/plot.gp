#!/usr/bin/gnuplot
reset
set terminal png size 1000, 600
#set size 0.5, 0.5
set output "chart.png"

set xdata time
set timefmt '%Y-%m-%d %H:%M:%S'
set format x '%Y-%m-%d %H:%M:%S'
set xlabel "Date"
#set xrange ["2019-12-13 10:00:00":"2019-12-13 23:00:00"]
#set yrange [0:1200]
set xtics rotate by 60 right

set ylabel "uSv/h"
set format y "%.0f"

set title "Radiation over time"
set key reverse Left outside
set grid

set style data linespoints

set datafile separator ","

# data format:
#   Dec 06 17:36:17, 1.023884, 0.457895


toffset=strptime("%Y-%m-%d %H:%M:%S","2019-12-06 17:36:17")

plot '/tmp/geiger1_out.csv' using 1:2:3 w points pt 7 pointsize 0.3 lc rgb "red" notitle, \
     '/tmp/geiger1_status_boots.csv' using 1:(2) w points pt 7 pointsize 2 lc rgb "blue" notitle
