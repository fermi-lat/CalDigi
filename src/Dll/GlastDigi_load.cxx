/** 
* @file GlastDigi_load.cpp
* @brief This is needed for forcing the linker to load all components
* of the library.
*
*  $Header: /nfs/slac/g/glast/ground/cvs/GlastDigi/src/Dll/GlastDigi_load.cxx,v 1.3 2002/04/23 23:28:27 richard Exp $
*/

#include "GaudiKernel/DeclareFactoryEntries.h"

DECLARE_FACTORY_ENTRIES(CalDigi) {
    DECLARE_ALGORITHM( CalDigiAlg );

} 

