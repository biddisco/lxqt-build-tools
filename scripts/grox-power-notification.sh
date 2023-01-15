#!/bin/bash
#
# Copy contents to /lib/systemd/system-sleep/grox.sh
# sudo chmod a+x /lib/systemd/system-sleep/grox.sh
# NB : https://askubuntu.com/questions/226278/run-script-on-wakeup
#
case $1 in
    pre)
        # suspending to RAM
        time=$(date +"%d-%m-%Y %H:%M:%S")
        echo "$time + grox     SLEEP" >> /tmp/grox.txt
        ;;
    post)
        # resume from suspend
        time=$(date +"%d-%m-%Y %H:%M:%S")
        echo "$time + grox     WAKE" >> /tmp/grox.txt
        ;;
esac

