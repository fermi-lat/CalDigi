#include "OnePlusExpTaper.h"
#include "GaudiKernel/ToolFactory.h"
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
    

   // Read in the parameters from the XML file
    xml::IFile m_ifile(m_xmlFile.c_str());
    if (m_ifile.contains("onePlusExpDigi", "offset")) 
            m_offset = m_ifile.getDouble("onePlusExpDigi", "offset");
    else return StatusCode::FAILURE;

     if (m_ifile.contains("onePlusExpDigi", "scaleExponential")) 
        m_scaleExponential = m_ifile.getDouble("onePlusExpDigi", "scaleExponential");
     else return StatusCode::FAILURE;

     if (m_ifile.contains("onePlusExpDigi", "scaleExponent")) 
        m_scaleExponent = m_ifile.getDouble("onePlusExpDigi", "scaleExponent");
      else return StatusCode::FAILURE;

     if (m_ifile.contains("onePlusExpDigi",
        "scaleTurnoverExponential")) 
        m_scaleTurnoverExponential = m_ifile.getDouble("onePlusExpDigi",
        "scaleTurnoverExponential");
     else return StatusCode::FAILURE;
 
     if (m_ifile.contains("onePlusExpDigi", 
        "scaleTurnoverExponent")) 
        m_scaleTurnoverExponent = m_ifile.getDouble("onePlusExpDigi",
        "scaleTurnoverExponent");
     else return StatusCode::FAILURE;
 

     return StatusCode::SUCCESS;

}

std::pair<double, double> OnePlusExpTaper::calculateSignals(idents::CalXtalId id,
                                                            double relativePosition, 
                                                        double depositedEnergy) {

    // Purpose and Method: calculate light taper for the two crystal ends .
    // Inputs: crystal id, deposited energy and relative position in the crystal
    // Outputs: energy seen at the two crystal ends

    

    double s1 = depositedEnergy*(m_offset+m_scaleExponential*exp(-relativePosition*m_scaleExponent) -
                 m_scaleTurnoverExponential*exp(m_scaleTurnoverExponent*relativePosition));
    
    double s2 = depositedEnergy*(m_offset+m_scaleExponential*exp(-(1-relativePosition)*m_scaleExponent) -
                 m_scaleTurnoverExponential*exp(m_scaleTurnoverExponent*(1-relativePosition)));

    return std::pair<double,double>(s1,s2);
    
        
};
