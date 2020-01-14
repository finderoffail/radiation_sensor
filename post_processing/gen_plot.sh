# convert data log to CSV, add year to timestamps (should have done this with ts to begin with)
cat datafile | grep geiger1_out | grep -v "_out 0.000000" | sed -e "s/ geiger1_out/,/" -e "s/ uSv\/h +\/-/,/" -e "s/^Dec /2019-12-/" > /tmp/geiger1_out.csv

# convert boot log to CSV
cat datafile | grep geiger1_status | grep booted | sed -e "s/ geiger1_status.*//" -e "s/^Dec /2019-12-/" >  /tmp/geiger1_status_boots.csv

# plot data
gnuplot plot.gp

