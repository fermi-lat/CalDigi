#include "OnePlusExpTaper.h"
#include "GaudiKernel/ToolFactory.h"
#include "GlastSvc/GlastDetSvc/IGlastDetSvc.h"
#include "GaudiKernel/MsgStream.h"

// to access an XML containing Digi parameters file
#include "xml/IFile.h"

#include "math.h"

static ToolFactory<OnePlusExpTaper> s_factory;
const IToolFactory& OnePlusExpTaperFactory = s_factory;

OnePlusExpTaper::OnePlusExpTaper( const std::string& type, 
                          const std::string& name, 
                          const IInterface* parent)
  : AlgTool(type,name,parent)
{
  // declare base interface for all consecutive concrete classes
  declareInterface<ITaper>(this);
    // Declare the properties that may be set in the job options file

  declareProperty ("xmlFile", m_xmlFile="$(CALDIGIROOT)/xml/CalDigi.xml");

}


StatusCode OnePlusExpTaper::initialize() {

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
    if(!detSvc->getNumericConstByName("CsILength",&m_CsILength)) {
            log << MSG::ERROR << " constant " << " CsILength not defined" << endreq;
            return StatusCode::FAILURE;
        } 

   // Read in the parameters from the XML file
    xml::IFile m_ifile(m_xmlFile.c_str());
    m_scaleExponential = m_ifile.getDouble("onePlusExpDigi", "scaleExponential");
    m_scaleExponent = m_ifile.getDouble("onePlusExpDigi", "scaleExponent");
 

     return StatusCode::SUCCESS;

}

std::pair<double, double> OnePlusExpTaper::calculateSignals(idents::CalXtalId id,
                                                            double relativePosition, 
                                                        double depositedEnergy) {

    // Purpose and Method: calculate light taper for the two crystal ends .
    // Inputs: crystal id, deposited energy and relative position in the crystal
    // Outputs: energy seen at the two crystal ends


    MsgStream log(msgSvc(), name());
    log << MSG::INFO << "execute" << endreq;

    
    double norm = 0.5+0.5*m_lightAtt; // light tapering in the center of crystal (relpos=0.5)

    double s1 = depositedEnergy*(1-relativePosition*(1-m_lightAtt) -
                 m_scaleExponential*exp(-m_scaleExponent*m_CsILength*relativePosition) )/norm;
    
    double s2 = depositedEnergy*(1-(1-relativePosition)*(1-m_lightAtt) -
                 m_scaleExponential*exp(-m_scaleExponent*m_CsILength*(1.-relativePosition)) )/norm;

    return std::pair<double,double>(s1,s2);
    
        
};
