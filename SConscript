# -*- python -*-
# $Header: /nfs/slac/g/glast/ground/cvs/CalDigi/SConscript,v 1.3 2008/11/12 20:31:21 glastrm Exp $
# Authors: Richard Dubois <richard@slac.stanford.edu>, Alexander Chekhtman <chehtman@ssd5.nrl.navy.mil>,Zachary Fewtrell <zachary.fewtrell@nrl.navy.mil>
# Version: CalDigi-03-07-00
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
