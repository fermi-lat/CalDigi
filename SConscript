# -*- python -*-
# $Header: /nfs/slac/g/glast/ground/cvs/CalDigi/SConscript,v 1.12 2011/12/12 20:37:34 heather Exp $
# Authors: Richard Dubois <richard@slac.stanford.edu>, Alexander Chekhtman <chehtman@ssd5.nrl.navy.mil>,Zachary Fewtrell <zachary.fewtrell@nrl.navy.mil>
# Version: CalDigi-03-09-00

Import('baseEnv')
Import('listFiles')
Import('packages')
progEnv = baseEnv.Clone()
libEnv = baseEnv.Clone()

libEnv.Tool('addLinkDeps', package='CalDigi', toBuild='component')

CalDigi = libEnv.ComponentLibrary('CalDigi', listFiles(['src/*.cxx']))


progEnv.Tool('CalDigiLib')

test_CalDigi = progEnv.GaudiProgram('test_CalDigi',
                                    listFiles(['src/test/*.cxx']),
                                    test = 1, package='CalDigi')

progEnv.Tool('registerTargets', package = 'CalDigi',
             libraryCxts = [[CalDigi, libEnv]],
             testAppCxts = [[test_CalDigi, progEnv]],
             jo = ['src/test/jobOptions.txt'])

