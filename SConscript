# -*- python -*-
# $Header: /nfs/slac/g/glast/ground/cvs/CalDigi/SConscript,v 1.2 2008/08/25 19:34:01 ecephas Exp $
# Authors: Richard Dubois <richard@slac.stanford.edu>, Alexander Chekhtman <chehtman@ssd5.nrl.navy.mil>,Zachary Fewtrell <zachary.fewtrell@nrl.navy.mil>
# Version: CalDigi-03-06-00
Import('baseEnv')
Import('listFiles')
Import('packages')
progEnv = baseEnv.Clone()
libEnv = baseEnv.Clone()

libEnv.Tool('CalDigiLib', depsOnly = 1)
CalDigi = libEnv.SharedLibrary('CalDigi', listFiles(['src/*.cxx','src/Dll/*.cxx']))

progEnv.Tool('CalDigiLib')
test_CalDigi = progEnv.GaudiProgram('test_CalDigi', listFiles(['src/test/*.cxx']), test = 1)

progEnv.Tool('registerObjects', package = 'CalDigi', libraries = [CalDigi], testApps = [test_CalDigi])
