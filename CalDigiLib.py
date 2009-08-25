#$Header: /nfs/slac/g/glast/ground/cvs/GlastRelease-scons/CalDigi/CalDigiLib.py,v 1.2 2008/08/25 19:37:58 ecephas Exp $
def generate(env, **kw):
    if not kw.get('depsOnly', 0):
        env.Tool('addLibrary', library = ['CalDigi'])
    env.Tool('configDataLib')
    env.Tool('EventLib')
    env.Tool('CalUtilLib')
    env.Tool('xmlBaseLib')
    env.Tool('addLibrary', library = env['gaudiLibs'])
def exists(env):
    return 1;
