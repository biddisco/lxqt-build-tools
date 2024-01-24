#params="--delete"
params="--exclude '*.pack'"

echo "-----------------------------------"
echo "Updating to Raspberry Pi"
echo "-----------------------------------"
$HOME/update-rsync.sh $HOME/src/grox/ pi:/home/pi/src/grox $params

echo
echo "-----------------------------------"
echo "Updating to old laptop (cable)"
echo "-----------------------------------"
# RJ45 network cable
$HOME/update-rsync.sh $HOME/src/grox/ oryx:/home/biddisco/src/grox $params

echo
echo "-----------------------------------"
echo "Updating to old laptop (wifi)"
echo "-----------------------------------"
# Wifi connection
$HOME/update-rsync.sh $HOME/src/xrp/grox/ biddisco@192.168.1.112:/home/biddisco/src/grox $params

