
#include "GlastSvc/GlastRandomSvc/IRandomAccess.h"
#include "GlastSvc/GlastRandomSvc/RandomAccess.h"
#include "GaudiKernel/ToolFactory.h"

/** @class CalDigiRandom
 *
* @brief Gaudi Tool to flag that package uses CLHEP random numbers
* 
* This tool inherits from the RandomAccess tool in GlastSvc. It's
* purpose is to flag that shared libraries built by this package
* uses a private copy of the CLHEP random number generator. The
* GlastRandomSvc service looks for instances of tools that inherit 
* from RandomAccess on startup. If it finds an instance it sets the
* the CLHEP random engine type and gets the address of the CLHEP 
* random engine for that shared library. GlastRandomSvc uses the 
* address to set the random seed for that engine (currently on a per 
* event basis)    
*     
*
* @authors Toby Burnett, Karl Young
*
* $Header
*/

class CalDigiRandom : public RandomAccess {
public:
  CalDigiRandom( const std::string& type, const std::string& name, const
		  IInterface* parent):RandomAccess(type,name,parent) {}
};

// Static factory for instantiation of algtool objects
static ToolFactory<CalDigiRandom> s_factory;
const IToolFactory& CalDigiRandomFactory = s_factory;
