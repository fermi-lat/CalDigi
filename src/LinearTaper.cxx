#include "LinearTaper.h"
#include "GaudiKernel/ToolFactory.h"
#include "GlastSvc/GlastDetSvc/IGlastDetSvc.h"
#include "GaudiKernel/MsgStream.h"


static ToolFactory<LinearTaper> s_factory;
const IToolFactory& LinearTaperFactory = s_factory;

LinearTaper::LinearTaper( const std::string& type, 
                          const std::string& name, 
                          const IInterface* parent)
  : AlgTool(type,name,parent)
{
  // declare base interface for all consecutive concrete classes
  declareInterface<ITaper>(this);
    // Declare the properties that may be set in the job options file

}


StatusCode LinearTaper::initialize() {
    // Purpose and Method: initialize linear taper model function .
    // Inputs: light attenuation parameter from detModel service.

    MsgStream log(msgSvc(), name());
    log << MSG::INFO << "initialize" << endreq;
    StatusCode sc;
    
    IGlastDetSvc* detSvc;

    if( serviceLocator() ) {
      StatusCode sc = serviceLocator()->service( "GlastDetSvc", detSvc, true );
    }
    if(sc.isFailure())
    {
      log << MSG::ERROR << "Could not find eventSvc" << endreq;
      return sc;
    }

    if(!detSvc->getNumericConstByName("cal.lightAtt",&m_lightAtt)) {
            log << MSG::ERROR << " constant " << " cal.lightAtt not defined" << endreq;
            return StatusCode::FAILURE;
        } 

     return StatusCode::SUCCESS;

}

std::pair<double, double> LinearTaper::calculateSignals(idents::CalXtalId id,
                                                        double relativePosition, 
                                                        double depositedEnergy) {

    // Purpose and Method: calculate light taper for the two crystal ends .
    // Inputs: crystal id, deposited energy and relative position in the crystal
    // Outputs: energy seen at the two crystal ends


    double norm = 0.5+0.5*m_lightAtt; // light tapering in the center of crystal (relpos=0.5)

    double s1 = depositedEnergy*(1-relativePosition*(1-m_lightAtt))/norm;
    double s2 = depositedEnergy*(1-(1-relativePosition)*(1-m_lightAtt))/norm;

    return std::pair<double,double>(s1,s2);
    
        
};
