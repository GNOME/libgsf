#!/usr/bin/python2.2

import sys, gsf
try:
    i=gsf.InputStdio(sys.argv[1])
except:
    print "Unable to open " + sys.argv[1]
    sys.exit(-1)

try:
    z=gsf.InputGZip(i)
except:
    print "Not a gzipped file"
    sys.exit(-1)

t=gsf.InputTextline(z)
while 1:
    s = t.utf8_gets()
    if not s:
        break
    print s
