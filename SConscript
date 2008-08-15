# -*- python -*-
# $Header$
# Authors: Richard Dubois <richard@slac.stanford.edu>, Alexander Chekhtman <chehtman@ssd5.nrl.navy.mil>,Zachary Fewtrell <zachary.fewtrell@nrl.navy.mil>
# Version: CalDigi-03-05-01
Import('baseEnv')
Import('listFiles')
Import('packages')
progEnv = baseEnv.Clone()
libEnv = baseEnv.Clone()

libEnv.Tool('CalDigiLib', depsOnly = 1)
CalDigi = libEnv.SharedLibrary('CalDigi', listFiles(['src/*.cxx']) + listFiles(['src/Dll/*.cxx']))

progEnv.Tool('CalDigiLib')
test_CalDigi = progEnv.GaudiProgram('test_CalDigi', listFiles(['src/test/*.cxx']), test = 1)

progEnv.Tool('registerObjects', package = 'CalDigi', libraries = [CalDigi], testApps = [test_CalDigi])
