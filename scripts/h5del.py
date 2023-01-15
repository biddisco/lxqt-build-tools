#!/usr/bin/python

import h5py
import sys

def usage():
    print("""
h5del: delete datasets from hdf5 file
Usage: h5del <hdf5 file> <datasets...>
""")

def h5del(fname, datasets):
  with h5py.File(fname, "r+") as f:
    for d in datasets:
      if not d in f:
        raise ValueError("dataset {} does not exist in {}".format(d, fname))

    for d in datasets:
      del f[d]

if __name__ == '__main__':
  if len(sys.argv) < 3:
    usage()
    sys.exit(1)

  fname = sys.argv[1]
  datasets = sys.argv[2:]

  try:
    h5del(fname, datasets)
  except Exception as e:
    sys.exit(e)

# vim: ft=python tw=100 ai et sw=2 ts=2
