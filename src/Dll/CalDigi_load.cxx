/** 
 * @file GlastDigi_load.cpp
 * @brief This is needed for forcing the linker to load all components
 * of the library.
 *
 *  $Header: /nfs/slac/g/glast/ground/cvs/CalDigi/src/Dll/CalDigi_load.cxx,v 1.6 2008/10/15 15:43:19 usher Exp $
 */

#include "GaudiKernel/DeclareFactoryEntries.h"
#include "GaudiKernel/IToolFactory.h"

#define DLL_DECL_TOOL(x)       extern const IToolFactory& x##Factory; x##Factory.addRef();


DECLARE_FACTORY_ENTRIES(CalDigi) {
  DECLARE_ALGORITHM( CalDigiAlg );
} 

