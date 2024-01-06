import h5py

infile = '/home/biddisco/.local/share/grox/grox.hdf5'

h5r=h5py.File(infile, 'r')

with h5py.File("f2.h5", 'w') as h5w:
    for obj in h5r.keys():
        h5r.copy(obj, h5w )
h5r.close()
