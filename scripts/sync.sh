#!/bin/bash
#params="--delete"
params="--exclude '*.pack' --exclude '.venv'"

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
  $HOME/update-rsync.sh $XDG_CONFIG_HOME/grox.ini                  pi5:/home/pi/.local/aarch64/config/grox.ini
  $HOME/update-rsync.sh $XDG_DATA_HOME/grox/                       pi5:/home/pi/.local/aarch64/share/grox/
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
    $HOME/update-rsync.sh $XDG_CONFIG_HOME/grox.ini     pop:$XDG_CONFIG_HOME/grox.ini
#    $HOME/update-rsync.sh $XDG_DATA_HOME/grox/grox.hdf5 pop:$XDG_DATA_HOME/grox/grox.hdf5
  else
    $HOME/update-rsync.sh $XDG_CONFIG_HOME/grox.ini     elf:$XDG_CONFIG_HOME/grox.ini
#    $HOME/update-rsync.sh $XDG_DATA_HOME/grox/grox.hdf5 elf:$XDG_DATA_HOME/grox/grox.hdf5
  fi
fi
