import h5py

def copy_datasets(group, new_group):
    for name, obj in group.items():
        if isinstance(obj, h5py.Dataset):
            print(name, obj.dtype)
            if obj.dtype == 'float64':
                print(f'Making float32 copy of dataset: {name}')
                new_group.create_dataset(name, shape=obj.shape, dtype='float32', data=obj[:])
            else:
                print(f'Copying directly dataset: {name}')
                new_group.create_dataset(name, shape=obj.shape, dtype=obj.dtype, data=obj[:])
        elif isinstance(obj, h5py.Group):
            print('Creating group', name)
            new_subgroup = new_group.create_group(name)
            copy_datasets(obj, new_subgroup)

infile = '/home/biddisco/.local/share/grox/grox.hdf5'
h5r=h5py.File(infile, 'r')

with h5py.File("new_file.h5", 'w') as h5w:        
    copy_datasets(h5r, h5w)

h5r.close()
