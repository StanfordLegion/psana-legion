#!/usr/bin/env python

# Copyright 2018 Stanford University
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#

from __future__ import print_function
import tables
import warnings

class SmallFile(object):
    """
    An interface to an HDF5 file for saving data using SmallData and MPIDataSource.
    """

    def __init__(self, filename, keys_to_save=[]):
        """
        Parameters
        ----------
        filename : str
            The full path filename to save to.

        keys_to_save : list of strings
            A list of event data keys to save to disk. For example, if you
            add only ['a'] to this list, but call SmallData.event(a=x, b=y),
            only the values for 'a' will be saved. Hierarchical data structures
            using dictionaries can be referenced using '/' for each level, e.g.
            event({'c' : {'d' : z}}) --> keys_to_save=['c/d'].
        """
        self.file_handle = tables.File(filename, 'w')
        self.keys_to_save = keys_to_save
        return


    def _get_node(self, k, dlist_master):
        """
        Retrieve or create (if necessary) the pytables node
        for a specific key.
        """

        try:
            node = self.file_handle.get_node('/'+k)

        except tables.NoSuchNodeError as e: # --> create node

            ex = dlist_master[k][0]

            if num_or_array(ex) == 'array':
                a = tables.Atom.from_dtype(ex.dtype)
                shp = tuple([0] + list(ex.shape))
            elif num_or_array(ex) == 'num':
                a = tables.Atom.from_dtype(np.array(ex).dtype)
                shp = (0,)

            path, _, name = k.rpartition('/')

            if name.startswith(RAGGED_PREFIX):
                node = self.file_handle.create_vlarray(where='/'+path, name=name,
                                                       atom=a,
                                                       createparents=True)
            else:
                node = self.file_handle.create_earray(where='/'+path, name=name,
                                                      shape=shp, atom=a,
                                                      createparents=True)

        return node


    @property
    def nevents_on_disk(self):
        try:
            self.file_handle.get_node('/', 'fiducials')
            return len(self.file_handle.root.fiducials)
        except tables.NoSuchNodeError:
            return 0


    def save(self, *args, **kwargs):
        """
        Save summary data to an HDF5 file (e.g. at the end of an event loop).

            1. Add data using key-value pairs (similar to SmallData.event())
            2. Add data organized in hierarchy using nested dictionaries (similar to 
               SmallData.event())

        These data are then saved to the file specifed in the SmallData
        constructor.

        Parameters
        ----------
        *args : dictionaries
            In direct analogy to the SmallData.event call, you can also pass
            HDF5 group heirarchies using nested dictionaries. Each level
            of the dictionary is a level in the HDF5 group heirarchy.

        **kwargs : datasetname, dataset
            Similar to SmallData.event, it is possible to save arbitrary
            singleton data (e.g. at the end of a run, to save an average over
            some quanitity).

        Examples
        --------
        >>> # save "average_over_run"
        >>> smldata.save(average_over_run=x)
        
        >>> # save "/base/next_group/data"
        >>> smldata.save({'base': {'next_group' : data}})
        """

        to_save = {}
        to_save.update(kwargs) # deals with save(cspad_sum=array_containing_sum)

        # deals with save({'base': {'next_group' : data}}) case
        dictionaries_to_save  = [d for d in args if type(d)==dict]
        for d in dictionaries_to_save:
            to_save.update(_flatten_dictionary(d))


        # save "accumulated" data (e.g. averages)
        for k,v in to_save.items():

            if type(v) is not np.ndarray:
                v = np.array([v])

            path, _, name = k.rpartition('/')

            node = self.file_handle.create_carray(where='/'+path, name=name,
                                                  obj=v,
                                                  createparents=True)

        return



    def save_event_data(self, dlist_master):
        """
        Save (append) all event data in memory to disk.

        NOTE: This function gets called automatically on every gather
              when using a SmallData object.

        Parameters
        ----------
        dlist_master : dict
            The SmallData object's dlist_master, which is a dictionary
            where the keys are the event data keys, the values arrays
            of the event data.
        """

        if self.file_handle is None:
            # we could accept a 'filename' argument here in the save method
            raise IOError('no filename specified in SmallData constructor')

        # if the user has specified which keys to save, just
        # save those; else, save all event data
        if len(self.keys_to_save) > 0:
            keys_to_save = ['event_time', 'fiducials']
            for k in self.keys_to_save:
                if k in dlist_master.keys():
                    keys_to_save.append(k)
                else:
                    warnings.warn('event data key %s has no '
                                  'associated event data and will not '
                                  'be saved' % k)
        else:
            keys_to_save = dlist_master.keys()

        # for each item to save, write to disk
        for k in keys_to_save:
            if len(dlist_master[k]) > 0:
                node = self._get_node(k, dlist_master)
                if type(node) == tables.vlarray.VLArray:
                    for row in dlist_master[k]:
                        node.append(row)
                else:
                    if not all(arr.shape==node.shape[1:] for arr in dlist_master[k]):
                        raise ValueError('Found ragged array named "%s". ' 
                                         'Prepend HDF5 dataset name with '
                                         '"ragged_" to avoid this error.' % k)
                    node.append( dlist_master[k] )
            else:
                pass

        return


    def close(self):
        """
        Close the HDF5 file used for writing.
        """
        self.file_handle.close()
        return



class LegionHDF5(object):
    __slots__ = ['filepath', 'smallFile']
    def __init__(self, filepath):
        self.filepath = filepath
        self.smallFile = SmallFile(self.filepath)

    def append_to_file(self, list):
        print('append_to_file', list)
        for event in list:
            print('event', event)
            self.smallFile.save_event_data(event)


