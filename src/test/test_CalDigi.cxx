// $Header: /nfs/slac/g/glast/ground/cvs/CalDigi/src/test/test_CalDigi.cxx,v 1.2 2002/08/18 17:01:50 richard Exp $

// Include files
// Gaudi system includes
#include "GaudiKernel/MsgStream.h"
#include "GaudiKernel/AlgFactory.h"
#include "GaudiKernel/IDataProviderSvc.h"
#include "GaudiKernel/SmartDataPtr.h"
#include "GaudiKernel/Algorithm.h"

// TDS class declarations: input data, and McParticle tree

#include "Event/TopLevel/EventModel.h"

#include "Event/Digi/CalDigi.h"

// Define the class here instead of in a header file: not needed anywhere but here!
//------------------------------------------------------------------------------
/** 
A simple algorithm.

  
*/
class test_CalDigi : public Algorithm {
public:
    test_CalDigi(const std::string& name, ISvcLocator* pSvcLocator);
    StatusCode initialize();
    StatusCode execute();
    StatusCode finalize();
    
private: 
    //! number of times called
    int m_count; 
    //! the GlastDetSvc used for access to detector info
};
//------------------------------------------------------------------------

// necessary to define a Factory for this algorithm
// expect that the xxx_load.cxx file contains a call     
//     DLL_DECL_ALGORITHM( test_CalDigi );

static const AlgFactory<test_CalDigi>  Factory;
const IAlgFactory& test_CalDigiFactory = Factory;

//------------------------------------------------------------------------
//! ctor
test_CalDigi::test_CalDigi(const std::string& name, ISvcLocator* pSvcLocator)
:Algorithm(name, pSvcLocator)
,m_count(0)
{
}

//------------------------------------------------------------------------
//! set parameters and attach to various perhaps useful services.
StatusCode test_CalDigi::initialize(){
    StatusCode  sc = StatusCode::SUCCESS;
    MsgStream log(msgSvc(), name());
    log << MSG::INFO << "initialize" << endreq;
    
    return sc;
}

//------------------------------------------------------------------------
//! process an event
StatusCode test_CalDigi::execute()
{
    StatusCode  sc = StatusCode::SUCCESS;
    MsgStream   log( msgSvc(), name() );
    log << MSG::INFO << "executing " << ++m_count << " time" << endreq;


 // First, the collection of CalDigis is retrieved from the TDS
  SmartDataPtr<Event::CalDigiCol> digiCol(eventSvc(),EventModel::Digi::CalDigiCol );

  if ((digiCol == 0) || (digiCol->size()) == 0) {
    log << MSG::FATAL << "no/incorrect number calorimeter digis found" << endreq;
    sc = StatusCode::FAILURE;
    return sc;
  }


    return sc;
}

//------------------------------------------------------------------------
//! clean up, summarize
StatusCode test_CalDigi::finalize(){
    StatusCode  sc = StatusCode::SUCCESS;
    MsgStream log(msgSvc(), name());
    log << MSG::INFO << "finalize after " << m_count << " calls." << endreq;
    
    return sc;
}



