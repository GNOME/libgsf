import argtypes

arg = argtypes.StringArg()

argtypes.matcher.register('char-const*', arg)
argtypes.matcher.register('gchar-const*', arg)
argtypes.matcher.register('guint8*', arg)
argtypes.matcher.register('const-guint8*', arg)
argtypes.matcher.register('guint8-const*', arg)
 
