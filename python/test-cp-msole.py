#!/usr/bin/python3
#
# test-cp-msole.py
#

import sys
import gi

gi.require_version("Gsf", "1")

from gi.repository import Gsf


def clone(iput, oput):
    size = iput.size
    if size > 0:
        lr = iput.remaining()
        while lr > 0:
            data = iput.read(lr)
            ld = len(data)
            lr = lr - ld
            oput.write(data)
    else:
        clone_dir(iput, oput)
    oput.close()


def clone_dir(iput, oput):
    nc = iput.num_children()
    for i in range(nc):
        inew = iput.child_by_index(i)
        isdir = inew.num_children() > 0
        onew = oput.new_child(iput.name_by_index(i), isdir)
        clone(inew, onew)


def test(argv):
    print(f"test ‘{argv[1]}’ → ‘{argv[2]}’")

    input_file = Gsf.InputStdio.new(argv[1])
    if input_file == None:
        print("yuck1")
    infile = Gsf.InfileMSOle.new(input_file)
    if infile == None:
        print("yuck2")
    del input_file

    output = Gsf.OutputStdio.new(argv[2])
    if output == None:
        print("yuck3")
    outfile = Gsf.OutfileMSOle.new(output)
    if outfile == None:
        print("yuck4")
    del output

    clone(infile, outfile)


test(sys.argv)
