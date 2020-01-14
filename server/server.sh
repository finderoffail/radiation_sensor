#install prerequisites
sudo apt install moreutils mosquitto mosquitto-clients

#start server, logging to file with timestamps
mosquitto_sub -v -t \# |  ts >>  datafile
