// $Header$

// Author:
//   Marco Frailis

/// Glast specific includes
#include "Event/TopLevel/EventModel.h"
#include "GaudiKernel/ObjectList.h"

// MC and Digi classes
#include "Event/Digi/CalDigi.h"
#include "Event/MonteCarlo/McIntegratingHit.h"
#include "Event/MonteCarlo/McParticle.h"

// Relational Table
#include "Event/RelTable/Relation.h"
#include "Event/RelTable/RelTable.h"


// Gaudi system includes
#include "GaudiKernel/IDataProviderSvc.h"
#include "GaudiKernel/SmartDataPtr.h"
#include "GaudiKernel/MsgStream.h"
#include "GaudiKernel/AlgFactory.h"
#include "GaudiKernel/Algorithm.h"
#include <string>
#include <vector>
#include <map>


class DigiHitsTestAlg : public Algorithm {
    
public:

  DigiHitsTestAlg(const std::string& name, ISvcLocator* pSvcLocator); 
    
  StatusCode initialize();
  StatusCode execute();
  StatusCode finalize();

};


static const AlgFactory<DigiHitsTestAlg>  Factory;
const IAlgFactory& DigiHitsTestAlgFactory = Factory;


//------------------------------------------------------------------------------
//
DigiHitsTestAlg::DigiHitsTestAlg(const std::string& name, ISvcLocator* pSvcLocator) :
  Algorithm(name, pSvcLocator){
    
}


//------------------------------------------------------------------------------
/*! */
StatusCode DigiHitsTestAlg::initialize() {
    
    
  StatusCode sc = StatusCode::SUCCESS;
    
  return sc;
}


//------------------------------------------------------------------------------
StatusCode DigiHitsTestAlg::execute() {
    
  StatusCode  sc = StatusCode::SUCCESS;
  MsgStream   log( msgSvc(), name() );    


  // First, the collection of CalDigis is retrieved from the TDS
  SmartDataPtr<Event::CalDigiCol> digiCol(eventSvc(),EventModel::Digi::CalDigiCol );

  if (digiCol == 0) {
    log << MSG::DEBUG << "no calorimeter hits found" << endreq;
    sc = StatusCode::FAILURE;
    return sc;
  }
    
  
  // The purpose of the following typedefs  is to write shorter lines of code
  typedef Event::Relation< Event::CalDigi,Event::McIntegratingHit > DigiHitsRel;
  typedef ObjectList<DigiHitsRel > DigiHitsCol;

  // Then an ObjectList of Relations between CalDigis and McHintegratingHits is 
  // retrieved from the TDS
  SmartDataPtr<DigiHitsCol> digiHitsCol(eventSvc(), EventModel::Digi::CalDigiHitTab );

  if (digiHitsCol == 0) {
    log << MSG::DEBUG << "no CalDigi-Hits Relations found" << endreq;
    sc = StatusCode::FAILURE;
    return sc;
  }
 
  // How many Relations do we have?            
  log << MSG::INFO << "Number of Relations = "
      << digiHitsCol->size() << endreq;

  // A Relational Table is initialized with the collection of Relations retrieved 
  // from the TDS
  Event::RelTable<Event::CalDigi,Event::McIntegratingHit > digiHitsTab(digiHitsCol);

  // Now, we want to compute the total energy per primary particle deposited into the
  // calorimeter

  // First, a map associating to each particle id its total energy is defined
  std::map<Event::McParticle::StdHepId, double> particlesTotalEnergy;

  typedef std::vector< std::pair<Event::McParticle::StdHepId, double> > EnergyDepositMapId; 

  // All iterators needed in the following loops
  
  // The iterator for the collection of digis
  Event::CalDigiCol::const_iterator itDigis;

  // The iterator for the vector of Relations returned by each query
  vector<DigiHitsRel*>::const_iterator itRels;

  // The iterator for the particle-energy map returned by each McIntegratingHit
  EnergyDepositMapId::const_iterator itEn;

  
  // The meaning of these three nested loops is:
  // for each digi, retrieve all hits related to it, and for each McIntegratingHit
  // retrieve the energy map and add all energy values to the map relating
  // each primary particle to its total energy

  for (itDigis = digiCol->begin(); itDigis != digiCol->end(); itDigis++) {
    vector<DigiHitsRel*> relations = digiHitsTab.getRelByFirst(*itDigis);
    for (itRels = relations.begin(); itRels != relations.end(); itRels++) {
      EnergyDepositMapId& energyItemId = (*itRels)->getSecond()->itemizedEnergyId();
      for (itEn = energyItemId.begin(); itEn != energyItemId.end(); itEn++) {
        particlesTotalEnergy[(*itEn).first] += (*itEn).second;
      }
    }
  }       


  return sc;
}


//------------------------------------------------------------------------------
StatusCode DigiHitsTestAlg::finalize() {
    
  return StatusCode::SUCCESS;
}







