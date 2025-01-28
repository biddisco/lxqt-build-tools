#!/bin/bash
#params="--delete"
params="--exclude '*.pack'"

machine_on() {
  if nc -z $1 22 2>/dev/null; then
      echo "$1 ✓"
      code=0
  else
      echo "$1 ✗"
      code=1
  fi
  return $code
}

echo "-----------------------------------"
echo "Updating to Raspberry Pi"
echo "-----------------------------------"
if machine_on pi5; then
  $HOME/update-rsync.sh $HOME/src/grox/                            pi5:/home/pi/src/grox $params
  $HOME/update-rsync.sh /home/biddisco/.config/grox.ini            pi5:/home/pi/.config/grox.ini
  $HOME/update-rsync.sh /home/biddisco/.local/share/grox/grox.hdf5 pi5:/home/pi/.local/share/grox/grox.hdf5
fi

echo
echo "-----------------------------------"
echo "Updating to old laptop oryx (cable ip address)"
echo "-----------------------------------"
if machine_on oryx; then
  $HOME/update-rsync.sh $HOME/src/grox/ oryx:/home/biddisco/src/grox $params
fi

echo
echo "-----------------------------------"
echo "Updating to old laptop pop (cable ip address)"
echo "-----------------------------------"
if machine_on pop; then
  $HOME/update-rsync.sh $HOME/src/grox/  pop:/home/biddisco/src/grox $params
  if [[ "$(hostname)" == "elf" ]]; then
    $HOME/update-rsync.sh /home/biddisco/.config/grox.ini            pop:/home/biddisco/.config/grox.ini
    $HOME/update-rsync.sh /home/biddisco/.local/share/grox/grox.hdf5 pop:/home/biddisco/.local/share/grox/grox.hdf5
  else
    $HOME/update-rsync.sh /home/biddisco/.config/grox.ini            elf:/home/biddisco/.config/grox.ini
    $HOME/update-rsync.sh /home/biddisco/.local/share/grox/grox.hdf5 elf:/home/biddisco/.local/share/grox/grox.hdf5
  fi
fi
