
echo "-----------------------------------"
echo "Updating to Raspberry Pi"
echo "-----------------------------------"
$HOME/update-rsync.sh $HOME/src/xrp/grox/ pi@192.168.1.15:/home/pi/src/grox --delete

echo 
echo "-----------------------------------"
echo "Updating to old laptop (cable)"
echo "-----------------------------------"
# RJ45 network cable
$HOME/update-rsync.sh $HOME/src/xrp/grox/ biddisco@192.168.1.165:/home/biddisco/src/grox --delete

echo 
echo "-----------------------------------"
echo "Updating to old laptop (wifi)"
echo "-----------------------------------"
# Wifi connection
$HOME/update-rsync.sh $HOME/src/xrp/grox/ biddisco@192.168.1.112:/home/biddisco/src/grox --delete

echo 
echo "-----------------------------------  "
echo "Updating to old laptop (cscs network)"
echo "-----------------------------------  "
# Wifi connection
$HOME/update-rsync.sh $HOME/src/xrp/grox/ biddisco@148.187.133.20:/home/biddisco/src/grox --delete


