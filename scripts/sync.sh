#!/bin/bash
#params="--delete"
params="--exclude '*.pack'"

echo "-----------------------------------"
echo "Updating to Raspberry Pi"
echo "-----------------------------------"
$HOME/update-rsync.sh $HOME/src/grox/ pi5:/home/pi/src/grox $params

echo
echo "-----------------------------------"
echo "Updating to old laptop (cable ip address)"
echo "-----------------------------------"
$HOME/update-rsync.sh $HOME/src/grox/ oryx:/home/biddisco/src/grox $params
$HOME/update-rsync.sh $HOME/src/grox/  pop:/home/biddisco/src/grox $params

if [[ "$(hostname)" == "elf" ]]; then
    $HOME/update-rsync.sh /home/biddisco/.config/grox.ini            pop:/home/biddisco/.config/grox.ini
    $HOME/update-rsync.sh /home/biddisco/.local/share/grox/grox.hdf5 pop:/home/biddisco/.local/share/grox/grox.hdf5
else
    $HOME/update-rsync.sh /home/biddisco/.config/grox.ini            elf:/home/biddisco/.config/grox.ini
    $HOME/update-rsync.sh /home/biddisco/.local/share/grox/grox.hdf5 elf:/home/biddisco/.local/share/grox/grox.hdf5
fi
