#params="--delete"
params="--exclude '*.pack'"

echo "-----------------------------------"
echo "Updating to Raspberry Pi"
echo "-----------------------------------"
$HOME/update-rsync.sh $HOME/src/grox/ pi@192.168.1.15:/home/pi/src/grox $params

echo
echo "-----------------------------------"
echo "Updating to old laptop (cable)"
echo "-----------------------------------"
# RJ45 network cable
$HOME/update-rsync.sh $HOME/src/xrp/grox/ biddisco@192.168.1.165:/home/biddisco/src/grox $params

echo
echo "-----------------------------------"
echo "Updating to old laptop (wifi)"
echo "-----------------------------------"
# Wifi connection
$HOME/update-rsync.sh $HOME/src/xrp/grox/ biddisco@192.168.1.112:/home/biddisco/src/grox $params

echo
echo "-----------------------------------  "
echo "Updating to old laptop (cscs network)"
echo "-----------------------------------  "
# Wifi connection
$HOME/update-rsync.sh $HOME/src/xrp/grox/ biddisco@148.187.133.28:/home/biddisco/src/grox $params

# to copy the settings
# scp /home/biddisco/.config/grox.ini 148.187.133.28:/home/biddisco/.config/grox.ini
# scp /home/biddisco/.ssh/.gkey.sh    148.187.133.28:/home/biddisco/.ssh/.gkey.sh
# scp /home/biddisco/.local/share/grox/grox.hdf5 148.187.133.28:/home/biddisco/.local/share/grox/grox.hdf5
