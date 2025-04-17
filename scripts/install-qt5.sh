#!/bin/bash

# this is the directory of the script, regardless of where it's called from
if [[ $0 != $BASH_SOURCE ]]; then
  # this script was sourced from somewhere, expend name if a symlink
  SCRIPT_DIR=$(dirname $(readlink -f $BASH_SOURCE))
else
  # this was executed directly
  SCRIPT_DIR="$(readlink -f $(dirname $0))"
fi

# https://github.com/miurahr/aqtinstall

export AQT_VERSION="aqtinstall"
export QT_VERSION=5.15.2
export QT_PATH=/opt/Qt
export QT_GCC=${QT_PATH}/${QT_VERSION}/gcc_64 \

# create a virtual env
python3 -m venv aqt-env --prompt aqt-env
source aqt-env/bin/activate

pip install "$AQT_VERSION"

# display all Qt6 versions available
aqt list-qt linux desktop

# display all Qt6 modules available
aqt list-qt linux desktop --long-modules $QT_VERSION linux_gcc_64

# install "all" qt modules
sudo aqt-env/bin/aqt install-qt -O "$QT_PATH" linux desktop "$QT_VERSION" linux_gcc_64 -m all

deactivate

#sudo aqt install-tool -O "$QT_PATH" linux desktop tools_cmake
#sudo aqt install-tool -O "$QT_PATH" linux desktop tools_ninja

