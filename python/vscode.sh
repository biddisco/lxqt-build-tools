#!/bin/bash

# this is the directory of the script, regardless of where it's called from
if [[ $0 != $BASH_SOURCE ]]; then
  # this script was sourced from somewhere, expand name if a symlink
  SCRIPT_DIR=$(dirname $(readlink -f $BASH_SOURCE))
else
  # this was executed directly
  SCRIPT_DIR="$(readlink -f $(dirname $0))"
fi

# activate python env
source $SCRIPT_DIR/.venv/bin/activate

# launch vscode in our dir
code $SCRIPT_DIR
