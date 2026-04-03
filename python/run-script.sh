#!/bin/bash

# ---------------
# This script should not be needed when calling *.py from inside a venv
# but is needed when executed from some application that has not initialized
# python in the expected way.
# It exists here to allow grox to call python scripts from a shell and collect output
# ---------------

# this is the directory of the script, regardless of where it's called from
if [[ $0 != $BASH_SOURCE ]]; then
  # this script was sourced from somewhere, expend name if a symlink
  SCRIPT_DIR=$(dirname $(readlink -f $BASH_SOURCE))
else
  # this was executed directly
  SCRIPT_DIR="$(readlink -f $(dirname $0))"
fi

unset PYTHONPATH

# activate python env
source $SCRIPT_DIR/.venv/bin/activate

unset SESSION_MANAGER
export LC_ALL=C.UTF-8
# QT_VER should be set already on system
if [ -z "$QT_VER" ]; then
  echo "Setting QT_VER"
  export QT_VER=6.10.1
fi
export LD_LIBRARY_PATH=/opt/Qt/$QT_VER/gcc_64/lib:$LD_LIBRARY_PATH
export QT_PLUGIN_PATH=/opt/Qt/$QT_VER/gcc_64/plugins
export QML2_IMPORT_PATH=/opt/Qt/$QT_VER/gcc_64//qml

echo "Running" $@
$@
