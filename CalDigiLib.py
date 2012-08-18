#$Header: /nfs/slac/g/glast/ground/cvs/GlastRelease-scons/CalDigi/CalDigiLib.py,v 1.3 2009/08/25 23:20:16 jrb Exp $
def generate(env, **kw):
    if not kw.get('depsOnly', 0):
        env.Tool('addLibrary', library = ['CalDigi'])
    env.Tool('configDataLib')
    env.Tool('EventLib')
    env.Tool('CalUtilLib')
    env.Tool('xmlBaseLib')
    env.Tool('addLibrary', library = env['gaudiLibs'])
    if env['PLATFORM']=='win32' and env.get('CONTAINERNAME','')=='GlastRelease':
        env.Tool('findPkgPath', package = 'CalXtalResponse') 
        env.Tool('findPkgPath', package = 'LdfEvent') 
        env.Tool('findPkgPath', package = 'Event') 
        env.Tool('findPkgPath', package = 'enums') 
	env.Tool('findPkgPath', package = 'ConfigSvc')
def exists(env):
    return 1;
