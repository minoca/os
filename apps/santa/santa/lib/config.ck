/*++

Copyright (c) 2017 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    config.ck

Abstract:

    This module contains a class used to manage configuration files.

Author:

    Evan Green 24-May-2017

Environment:

    Chalk

--*/

//
// ------------------------------------------------------------------- Includes
//

from json import dumps, loads;
from io import open;
from iobase import IoError;
from os import ENOENT;

//
// --------------------------------------------------------------------- Macros
//

//
// ---------------------------------------------------------------- Definitions
//

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

//
// -------------------------------------------------------------------- Globals
//

//
// ------------------------------------------------------------------ Functions
//

//
// This class encapsulates a configuration backed by a configuration file. It
// can be used like an ordinary dictionary, but is loaded from a file and can
// be saved to a file at will.
//

class ConfigFile {
    var _data;
    var _metadata;

    function
    __init (
        )

    /*++

    Routine Description:

        This routine initializes a blank configuration file.

    Arguments:

        None.

    Return Value:

        Returns the initialized instance.

    --*/

    {

        _data = {};
        _metadata = {};
        return this;
    }

    function
    __init (
        filepath,
        defaults
        )

    /*++

    Routine Description:

        This routine initializes and loads a configuration file.

    Arguments:

        filepath - Supplies the path of the file backing this configuration.

        defaults - Supplies a default configuration in case the configuration
            file does not exist. If the file can be loaded, this is ignored.

    Return Value:

        Returns the initialized instance.

    --*/

    {

        this.__init();
        _data = defaults;
        _metadata["filepath"] = filepath;
        _metadata["remainder"] = "";
        _metadata["loaded"] = false;
        if (filepath != null) {
            try {
                this.load(filepath);

            } except IoError as e {
                if (e.errno != ENOENT) {
                    Core.raise(e);
                }
            }
        }

        return this;
    }

    function
    load (
        filepath
        )

    /*++

    Routine Description:

        This routine loads the configuration file from disk. It discards any
        data that was previously in this instance.

    Arguments:

        filepath - Supplies the path of the file backing this configuration.

    Return Value:

        null on success.

        Raises an exception if the file does not exist or cannot be loaded.

    --*/

    {

        var data;
        var datalist;
        var file;

        file = open(filepath, "rb");
        data = file.readall();
        file.close();
        datalist = [data];
        _data = loads(datalist);
        _metadata["filepath"] = filepath;
        _metadata["remainder"] = datalist[0];
        _metadata["loaded"] = true;
        return;
    }

    function
    save (
        )

    /*++

    Routine Description:

        This routine saves the configuration to disk.

    Arguments:

        None.

    Return Value:

        null on success.

        Raises an exception if the file cannot be saved.

    --*/

    {

        var data;
        var file;

        data = dumps(_data, 4);
        file = open(_metadata["filepath"], "wb");
        file.write(data);
        data = _metadata.get("remainder");
        if (data) {
            file.write(data);
        }

        file.close();
        return null;
    }

    //
    // Functions to make the instance act like a dictionary.
    //

    function
    __get (
        key
        )

    /*++

    Routine Description:

        This routine performs a "get" operation, returning the value from the
        data dictionary.

    Arguments:

        key - Supplies the key to get.

    Return Value:

        Returns the value for the given key.

    --*/

    {

        if (key == "_meta") {
            return _metadata;
        }

        return _data.__get(key);
    }

    function
    __set (
        key,
        value
        )

    /*++

    Routine Description:

        This routine performs a "set" operation, saving a value into the data
        dictionary.

    Arguments:

        key - Supplies the key to set.

        value - Supplies the value to set.

    Return Value:

        None.

    --*/

    {

        if (key == "_meta") {
            _metadata = value;
            return value;
        }

        return _data.__set(key, value);
    }

    function
    get (
        key
        )

    /*++

    Routine Description:

        This routine performs a "get" operation, returning the value from the
        data dictionary.

    Arguments:

        key - Supplies the key to get.

    Return Value:

        Returns the value for the given key.

        null if no value exists for that key.

    --*/

    {

        if (key == "_meta") {
            return _metadata;
        }

        return _data.get(key);
    }

    function
    set (
        key,
        value
        )

    /*++

    Routine Description:

        This routine performs a "set" operation, saving a value into the data
        dictionary.

    Arguments:

        key - Supplies the key to set.

        value - Supplies the value to set.

    Return Value:

        None.

    --*/

    {

        if (key == "_meta") {
            _metadata = value;
        }

        return _data.set(key, value);
    }

    function
    getKey (
        key
        )

    /*++

    Routine Description:

        This routine reads a config value, or returns null if it is not present.

    Arguments:

        key - Supplies the key to get, in "dot.notation".

    Return Value:

        Returns the value for the given key.

    --*/

    {

        var indices;
        var keys = key.split(".", -1);
        var value;

        value = _data;
        try {

            //
            // Index into each key, where keys are separated by periods.
            //

            for (element in keys) {

                //
                // Split up something like myvar[0][1] on the [.
                //

                indices = element.split("[", -1);
                element = indices[0];
                value = value[element];

                //
                // If there are array dereferences, do those.
                //

                if (indices.length() > 1) {
                    for (index in indices[1...-1]) {
                        if (value is List) {
                            index = Int.fromString(index[0..-1]);
                            value = value[index];

                        } else {
                            value = null;
                        }
                    }
                }
            }

            return value;

        } except KeyError {}

        return null;
    }

    function
    setKey (
        key,
        value
        )

    /*++

    Routine Description:

        This routine writes a config value to the override dictionary,
        creating the intermediate dictionaries if needed.

    Arguments:

        key - Supplies the key to get, in "dot.notation".

        value - Supplies the value to write. Supply null to remove the entry.

    Return Value:

        None.

    --*/

    {

        var currentvalue;
        var finalkey;
        var keys = key.split(".", -1);
        var index;
        var indices;
        var length = keys.length();
        var location = _data;

        //
        // Strip off the last one.
        //

        finalkey = keys[length - 1];
        keys.removeAt(length - 1);

        //
        // Dereference and create all the intermediate elements.
        //

        for (element in keys) {
            indices = element.split("[", -1);
            element = indices[0];

            //
            // Dereference into the named portion.
            //

            currentvalue = location.get(element);
            if (currentvalue == null) {
                if (indices.length() > 1) {
                    location[element] = [];

                } else {
                    location[element] = {};
                }
            }

            location = location[element];

            //
            // Index into a list.
            //

            if (indices.length() > 1) {
                for (index in indices[1...-1]) {
                    index = Int.fromString(index[0..-1]);
                    if (!(location is List)) {
                        Core.raise(ValueError("Error: Cannot index into "
                                              "non-list '%s'" % element));
                    }

                    //
                    // Append an empty list if needed.
                    //

                    if (index == location.length()) {
                        location.append([]);
                    }

                    location = location[index];
                }
            }
        }

        //
        // Do the last key. If there are no array dereferences on the end, then
        // just set the value.
        //

        indices = finalkey.split("[", -1);
        finalkey = indices[0];
        if (indices.length() == 1) {
            if (value == null) {
                location.remove(finalkey);

            } else {
                location[finalkey] = value;
            }

        } else {
            if (location.get(finalkey) == null) {
                location[finalkey] = [];
            }

            location = location[finalkey];

            //
            // Dereference all but the last one.
            //

            for (index in indices[1..-1]) {
                index = Int.fromString(index[0..-1]);
                if (!(location is List)) {
                    Core.raise(ValueError("Error: Cannot index into "
                                          "non-list '%s'" % finalkey));
                }

                //
                // Append an empty list if needed.
                //

                if (index == location.length()) {
                    location.append([]);
                }

                location = location[index];
            }

            //
            // Now do the last one.
            //

            index = Int.fromString(indices[-1][0..-1]);
            if (!(location is List)) {
                Core.raise(ValueError("Error: Cannot index into non-list '%s'" %
                                      finalkey));

            }

            if (value == null) {
                location.removeAt(index);

            } else {
                if (index == location.length()) {
                    location.append(value);

                } else {
                    location[index] = value;
                }
            }
        }

        return;
    }

    function
    __slice (
        key
        )

    /*++

    Routine Description:

        This routine executes the slice operator, which is called when square
        brackets are used.

    Arguments:

        key - Supplies the key to get.

    Return Value:

        Returns the value for the key in the data dictionary.

    --*/

    {

        return _data.__slice(key);
    }

    function
    __sliceAssign (
        key,
        value
        )

    /*++

    Routine Description:

        This routine executes the slice assignment operator, which is called
        when square brackets are used on the left side of an assignment.

    Arguments:

        key - Supplies the key to get.

    Return Value:

        Returns the value for the key in the data dictionary.

    --*/

    {

        return _data.__sliceAssign(key, value);
    }

    function
    __str (
        )

    /*++

    Routine Description:

        This routine converts the object into a string.

    Arguments:

        None.

    Return Value:

        None.

    --*/

    {

        return "<ConfigFile '%s'>" % [_metadata.get("filepath")];
    }

    function
    iterate (
        iterator
        )

    /*++

    Routine Description:

        This routine iterates over the object.

    Arguments:

        iterator - Supplies an iterator context, which is null at first.

    Return Value:

        Returns the new iterator value, or null if iteration should stop.

    --*/

    {

        return _data.iterate(iterator);
    }

    function
    iteratorValue (
        iterator
        )

    /*++

    Routine Description:

        This routine returns the value associated with the iterator.

    Arguments:

        iterator - Supplies an iterator context.

    Return Value:

        Returns the value corresponding to this iterator.

    --*/

    {

        return _data.iteratorValue(iterator);
    }

    function
    keys (
        )

    /*++

    Routine Description:

        This routine returns the keys in the data dictionary.

    Arguments:

        None.

    Return Value:

        Returns a list of keys in the dictionary.

    --*/

    {

        return _data.keys();
    }

    function
    dict (
        )

    /*++

    Routine Description:

        This routine returns a raw dictionary of the data.

    Arguments:

        None.

    Return Value:

        Returns the actual data dictionary.

    --*/

    {

        return _data;
    }
}

//
// --------------------------------------------------------- Internal Functions
//

