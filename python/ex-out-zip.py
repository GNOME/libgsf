#!/usr/bin/python2.2

import gsf

zipname="/tmp/aa.zip"

o=gsf.OutputStdio(zipname)
z=gsf.OutfileZip(o)
d1=z.new_child("mydir", True)
c1=d1.new_child("hi", False)
c2=d1.new_child("there", False)
d2=d1.new_child("yourdir", True)
c3=d2.new_child("foo", False)
print "the parent of %s is %s" % (c1.name(), c1.container().name())
print "the parent of %s is %s" % (c2.name(), c2.container().name())
print "the parent of %s is %s" % (d1.name(), d1.container().name())
print "the parent of %s is %s" % (d2.name(), d2.container().name())
print "the parent of %s is %s" % (c3.name(), c3.container().name())
print "the parent of %s is %s" % (z, z.container())
s="Gnumeric rules "*20
c1.write(len(s),s)
c1.close()
s="The Barking Crab "*20
c2.write(len(s),s)
c2.close()
s="the quick brown fox"
c3.write(len(s),s)
c3.close()
d1.close()
d2.close()
z.close()
o.close()
# Local variables:
# py-python-command: "/usr/bin/python2.2"
# End:
