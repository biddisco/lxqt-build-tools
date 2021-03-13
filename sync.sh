
echo "-----------------------------------"
echo "Updating to Raspberry Pi"
echo "-----------------------------------"
$HOME/update-rsync.sh $HOME/src/xrp/grox/ pi@192.168.1.15:/home/pi/src/grox --delete

echo 
echo "-----------------------------------"
echo "Updating to old laptop"
echo "-----------------------------------"
$HOME/update-rsync.sh $HOME/src/xrp/grox/ biddisco@192.168.1.112:/home/biddisco/src/grox --delete
