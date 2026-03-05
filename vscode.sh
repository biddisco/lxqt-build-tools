#!/bin/bash

# this is the directory of the script, regardless of where it's called from
if [[ $0 != $BASH_SOURCE ]]; then
  # this script was sourced from somewhere, expand name if a symlink
  SCRIPT_DIR=$(dirname $(readlink -f $BASH_SOURCE))
else
  # this was executed directly
  SCRIPT_DIR="$(readlink -f $(dirname $0))"
fi
echo script dir is $SCRIPT_DIR

export SHIBOKEN_PYTHON_SHARED_LIBRARY_OUTPUT_DIR=$QT_PLUGIN_PATH/../lib

# activate python env if it exists
if [[ -f "$SCRIPT_DIR/python/.venv/bin/activate" ]]; then
  echo Activating python environment in $SCRIPT_DIR
  source "$SCRIPT_DIR/python/.venv/bin/activate"
else
  source /home/biddisco/benchmarking-results/.venv/bin/activate
fi

echo checking for opal environment
#if spack env list | grep -q 'opal'; then
  echo Activating spack environment in $SCRIPT_DIR
  spack env activate --prompt opal
#fi

if [[ $1 == "python" ]]; then
  SCRIPT_DIR=$SCRIPT_DIR/python
fi

echo Launching vscode in $SCRIPT_DIR
# launch vscode in our dir
code $SCRIPT_DIR
