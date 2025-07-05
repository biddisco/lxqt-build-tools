import h5py
import os

def copy_datasets(group, new_group):
    for name, obj in group.items():
        if isinstance(obj, h5py.Dataset):
            print(name, obj.dtype)
            if obj.dtype == 'float64':
                print(f'Making float32 copy of dataset: {name}')
                new_group.create_dataset(
                    name,
                    shape=(0,) + obj.shape[1:],
                    maxshape=(h5py.h5s.UNLIMITED,) + obj.shape[1:],
                    dtype='float32',
                    chunks=(65536,) + obj.shape[1:],
                )
                # Write the data (resize first)
                new_group[name].resize(obj.shape)
                new_group[name][:] = obj[:].astype('float32')
            else:
                print(f'Copying directly dataset: {name}')
                new_group.create_dataset(
                    name,
                    shape=obj.shape,
                    dtype=obj.dtype,
                    data=obj[:],
                    chunks=obj.chunks,
                    compression=obj.compression,
                    compression_opts=obj.compression_opts,
                    shuffle=obj.shuffle,
                    fletcher32=obj.fletcher32,
                    fillvalue=obj.fillvalue,
                    layout=obj.layout if hasattr(obj, 'layout') else None
                )
        elif isinstance(obj, h5py.Group):
            print('Creating group', name)
            new_subgroup = new_group.create_group(name)
            copy_datasets(obj, new_subgroup)

infile = '/home/biddisco/.local/share/grox/grox.hdf5'
base, ext = os.path.splitext(infile)
outfile = base + '_float32' + ext
print(f'Reading from {infile},\nwriting to {outfile}')

h5r=h5py.File(infile, 'r')

with h5py.File(outfile, 'w') as h5w:
    copy_datasets(h5r, h5w)

h5r.close()
