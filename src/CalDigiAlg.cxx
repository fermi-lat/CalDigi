// LOCAL include files
#include "CalDigiAlg.h"

// Gaudi specific include files
#include "GaudiKernel/MsgStream.h"
#include "GaudiKernel/AlgFactory.h"
#include "GaudiKernel/IDataProviderSvc.h"
#include "GaudiKernel/SmartDataPtr.h"
#include "GaudiKernel/IToolSvc.h"

// Glast specific includes
#include "Event/TopLevel/EventModel.h"
#include "Event/TopLevel/DigiEvent.h"
#include "GaudiKernel/ObjectVector.h"
#include "CalUtil/ICalFailureModeSvc.h"
#include "CalUtil/CalDefs.h"

// Relational Table
#include "Event/RelTable/Relation.h"
#include "Event/RelTable/RelTable.h"
#include "Event/Digi/CalDigi.h"

// std stuff
#include <utility>
#include <string>
#include <iomanip>
#include <algorithm>
#include <cmath>

// Define the factory for this algorithm
static const AlgFactory<CalDigiAlg>  Factory;
const IAlgFactory& CalDigiAlgFactory = Factory;

using namespace std;
using idents::CalXtalId;
using namespace CalDefs;

/// construct object & delcare jobOptions
CalDigiAlg::CalDigiAlg(const string& name, ISvcLocator* pSvcLocator) :
  Algorithm(name, pSvcLocator) {

  // Declare the properties that may be set in the job options file
  declareProperty ("xtalDigiToolName", m_xtalDigiToolName = "XtalDigiTool");
  declareProperty ("RangeType",       m_rangeType       = "BEST");
}

/// initialize the algorithm. Set up parameters from detModel
StatusCode CalDigiAlg::initialize() {
  StatusCode sc;
  
  MsgStream msglog(msgSvc(), name());
  msglog << MSG::INFO << "initialize" << endreq;

  /////////////////////
  //-- JOB OPTIONS --//
  /////////////////////

  sc = setProperties();
  if ( !sc.isSuccess() ) {
    msglog << MSG::ERROR << "Could not set jobOptions properties" << endreq;
    return sc;
  }

  /////////////////////////////
  //-- RETRIEVE CONSTANTS  --//
  /////////////////////////////
  
  double value;
  typedef map<int*,string> PARAMAP;

  PARAMAP param;
  param[&m_xNum]=        string("xNum");
  param[&m_yNum]=        string("yNum");
  
  param[&m_eTowerCAL]=     string("eTowerCAL");
  param[&m_eLATTowers]=    string("eLATTowers");
  param[&m_eMeasureX]=     string("eMeasureX");
  param[&m_eXtal]=         string("eXtal");
  
  param[&m_CalNLayer]=     string("CALnLayer");
  param[&m_nCsIPerLayer]=  string("nCsIPerLayer");

  // now try to find the GlastDevSvc service
  IGlastDetSvc* detSvc;
  sc = service("GlastDetSvc", detSvc);
  if (sc.isFailure() ) {
    msglog << MSG::ERROR << "  Unable to get GlastDetSvc " << endreq;
    return sc;
  }

  for(PARAMAP::iterator it=param.begin(); it!=param.end();it++){
    if(!detSvc->getNumericConstByName((*it).second, &value)) {
      msglog << MSG::ERROR << " constant " <<(*it).second 
             <<" not defined" << endreq;
      return StatusCode::FAILURE;
    } else *((*it).first)=(int)value;
  }

  ///////////////////////////////////////
  //-- RETRIEVE HELPER TOOLS & SVCS  --//
  ///////////////////////////////////////

  sc = toolSvc()->retrieveTool(m_xtalDigiToolName,m_xtalDigiTool);
  if (sc.isFailure() ) {
    msglog << MSG::ERROR << "  Unable to create " << m_xtalDigiToolName << endreq;
    return sc;
  }
  
  sc = service("CalFailureModeSvc", m_FailSvc);
  if (sc.isFailure() ) {
    msglog << MSG::INFO << "  Did not find CalFailureMode service" << endreq;
    m_FailSvc = 0;
  }

  ////////////////////////////
  //-- FIND ACTIVE TOWERS --//
  ////////////////////////////

  // clear old list just to be safe
  m_twrList.clear();

  for (TwrNum testTwr; testTwr.isValid(); testTwr++) {
    // create geometry ID for 1st xtal in each tower
    idents::VolumeIdentifier volId;

    // volId info snagged from 
    // http://www.slac.stanford.edu/exp/glast/ground/software/geometry/docs/identifiers/geoId-RitzId.shtml
    volId.append(m_eLATTowers);
    volId.append(testTwr.getRow());
    volId.append(testTwr.getCol());
    volId.append(m_eTowerCAL);
    volId.append(0);
    volId.append(m_eMeasureX);
    volId.append(0);
    volId.append(m_eXtal);
    volId.append(0);

    // test to see if the volume ID is valid.
    string tmpStr; vector<double> tmpVec;
    sc = detSvc->getShapeByID(volId, &tmpStr, &tmpVec);
	if (!sc.isFailure()) {
		msglog << MSG::VERBOSE << "Cal unit detected twr_bay=" << testTwr << endreq;
		m_twrList.push_back(testTwr);
	}
  }

  return StatusCode::SUCCESS;
}


/// \brief take Hits from McIntegratingHits, create & register CalDigis
StatusCode CalDigiAlg::execute() {
  StatusCode  sc = StatusCode::SUCCESS;
  
  //Take care of insuring that data area has been created
  DataObject* pNode = 0;
  sc = eventSvc()->retrieveObject( EventModel::Digi::Event /*"/Event/Digi"*/, 
                                   pNode);

  if (sc.isFailure()) {
    sc = eventSvc()->registerObject(EventModel::Digi::Event /*"/Event/Digi"*/,
                                    new Event::DigiEvent);
    if( sc.isFailure() ) {
      // create msglog only when needed for performance
      MsgStream msglog( msgSvc(), name() );
      msglog << MSG::ERROR << "could not register " << 
        EventModel::Digi::Event /*<< /Event/Digi "*/ << endreq;
      return sc;
    }
  }

  // clear signal array: map relating xtal signal to id. 
  // Map holds diode and crystal responses
  // separately during accumulation.

  m_idMcInt.clear();
  m_idMcIntPreDigi.clear();

  sc = fillSignalEnergies();
  if (sc != StatusCode::SUCCESS) return sc;

  sc = createDigis();
  if (sc != StatusCode::SUCCESS) return sc;    

  return sc;
}

StatusCode CalDigiAlg::finalize() {
  MsgStream msglog(msgSvc(), name());
  msglog << MSG::INFO << "finalize" << endreq;

  return StatusCode::SUCCESS;
}

/// Template function fills any STL type container with zero values
template <class T> static void fill_zero(T &container) {
   fill(container.begin(), container.end(), 0);
}

/// Loop through each existing xtal & generate digis.
StatusCode CalDigiAlg::createDigis() {
  // Purpose and Method: 
  // create digis on the TDS from the deposited energies

  StatusCode  sc = StatusCode::SUCCESS;

  Event::CalDigiCol* digiCol = new Event::CalDigiCol;

  Event::RelTable<Event::CalDigi, Event::McIntegratingHit> digiHit;
  digiHit.init();

  /* Loop through all towers and crystals; collect up the McIntegratingHits by 
     xtal Id and send them off to xtalDigiTool to be digitized. Unhit crystals 
     can have noise added, so a null vector is sent in in that case.
  */
  
  vector<const Event::McIntegratingHit*> nullList;      
  // variables used in loop
  vector<int> adcP(RngNum::N_VALS);         
  vector<int> adcN(RngNum::N_VALS);         
  
  // loop through existing towers.
  for (unsigned twrSeq = 0; twrSeq < m_twrList.size(); twrSeq++) {
    // get bay id of nth live tower
    TwrNum twr = m_twrList[twrSeq];
    for (LyrNum lyr; lyr.isValid(); lyr++) {
      for (ColNum col; col.isValid(); col++) {

        // assemble current calXtalId
		const CalXtalId mapId(twr,lyr,col);

        vector<const Event::McIntegratingHit*>* hitList;

        // find this crystal in the existing list of McIntegratingHit vs Id map. 
        // If not found use null vector to see if a noise hit needs to be added.
        PreDigiMap::iterator hitListIt=m_idMcIntPreDigi.find(mapId);

        // we still process empty xtals (noise simulation may cause hits)
		if (hitListIt == m_idMcIntPreDigi.end())
          hitList = &nullList;

        else hitList = &(m_idMcIntPreDigi[mapId]);

        // note - important to reinitialize to 0 for each iteration
        CalXtalId::AdcRange rangeP = (CalXtalId::AdcRange)0;
        CalXtalId::AdcRange rangeN = (CalXtalId::AdcRange)0;
        bool lacP = false;
		bool lacN = false;
		fill_zero(adcP);
		fill_zero(adcN);
       
        sc = m_xtalDigiTool->calculate(mapId,
                                       *hitList,//in- all mchits on xtal & diodes
                                       lacP,    //out - pos face above thold
                                       lacN,    //out - neg face above thold
                                       rangeP,  //out - best range
                                       rangeN,  //out - best range
                                       adcP,    //out - ADC's for all ranges 0-3
                                       adcN     //out - ADC's for all ranges 0-3
                                       );
        if (sc.isFailure()) continue; // bad event
        

        // set status to ok for POS and NEG if no other bits set.
        
        if (!((CalDefs::RngNum)rangeP).isValid() || 
            !((CalDefs::RngNum)rangeN).isValid() ) {
          // create msglog only when needed for performance
          MsgStream msglog(msgSvc(), name());
          msglog << MSG::ERROR;
          if (msglog.isActive()){
            // use stream() support setw() manipulator
            msglog.stream() <<"Range exceeded!!! id=" << mapId
                            << " rangeP=" << int(rangeP) 
                            << " adcP=" << setw(4) << adcP[rangeP] 
                            << " lacP=" << lacP
                            << " rangeN=" << int(rangeN) 
                            << " adcN=" << setw(4) << adcN[rangeN] 
                            << " lacN=" << lacN;
          }
          msglog << endreq;
          return StatusCode::FAILURE;
        }

        unsigned short status = 0;
        if (!lacP && !lacN) continue;  // nothing more to see here. Move along.

        if (msgSvc()->outputLevel(name()) <= MSG::DEBUG) {
          // create msglog only when needed for speed
          MsgStream msglog(msgSvc(), name());
          msglog << MSG::DEBUG;
          // use stream() support setw() manipulator
          msglog.stream() << "id=" << mapId 
                          << "\trangeP=" << int(rangeP) 
                          << " adcP=" << setw(4) << adcP[rangeP] 
                          << " lacP=" << lacP
                          << "\trangeN=" << int(rangeN) 
                          << " adcN=" << setw(4) 
                          << adcN[rangeN] << " lacN=" << lacN;
          msglog << endreq;
        } 
        // check for failure mode. If killed, set to zero and set DEAD bit

        if (m_FailSvc != 0) {   
          if (m_FailSvc->matchChannel(mapId,
                                      (CalXtalId::POS))) {

            if (lacP) (status = status | Event::CalDigi::CalXtalReadout::DEAD_P);
          }
          if (m_FailSvc->matchChannel(mapId,
                                      (CalXtalId::NEG))) {
            if (lacN) (status = status | Event::CalDigi::CalXtalReadout::DEAD_N);

          }
        }

        if ((status & 0x00FF) == 0) status = 
                                      (status | Event::CalDigi::CalXtalReadout::OK_P);
        if ((status & 0xFF00) == 0) status = 
                                      (status | Event::CalDigi::CalXtalReadout::OK_N);

        CalXtalId::CalTrigMode rangeMode;
        int roLimit = 1;
        if (m_rangeType == "BEST") rangeMode = CalXtalId::BESTRANGE;
        else  {
          rangeMode = CalXtalId::ALLRANGE;
          roLimit = 4;
        }

        Event::CalDigi* curDigi = new Event::CalDigi(rangeMode, mapId);

        // set up the digi
        for (int nRo=0; nRo < roLimit; nRo++) {
          // represents ranges used for current readout in loop
          short roRangeP = (rangeP + nRo)%CalDefs::RngNum::N_VALS; 
          short roRangeN = (rangeN + nRo)%CalDefs::RngNum::N_VALS; 
          
          Event::CalDigi::CalXtalReadout ro = Event::CalDigi::CalXtalReadout(roRangeP, 
                                                                             adcP[roRangeP], 
                                                                             roRangeN, 
                                                                             adcN[roRangeN], 
                                                                             status);
          curDigi->addReadout(ro);
        }

        // set up the relational table between McIntegratingHit and digis
        typedef multimap< CalXtalId, Event::McIntegratingHit* >::const_iterator ItHit;
        pair<ItHit,ItHit> itpair = m_idMcInt.equal_range(mapId);

        for (ItHit mcit = itpair.first; mcit!=itpair.second; mcit++)
          {
            Event::Relation<Event::CalDigi,Event::McIntegratingHit> *rel =
              new Event::Relation<Event::CalDigi,Event::McIntegratingHit>(curDigi,mcit->second);
            digiHit.addRelation(rel);
          }

        // add the digi to the digi collection
        digiCol->push_back(curDigi);

      }
    }
  }


  sc = eventSvc()->registerObject(EventModel::Digi::CalDigiCol, digiCol);

  if (!(sc == StatusCode::FAILURE))
    sc = eventSvc()->registerObject(EventModel::Digi::CalDigiHitTab,digiHit.getAllRelations());
  return sc;
}

/** \brief collect deposited energies from McIntegratingHits and store in map sorted by XtalID. 

multimap used to associate mcIntegratingHit to id. There can be multiple
hits for the same id.  
*/
StatusCode CalDigiAlg::fillSignalEnergies() {
  StatusCode  sc = StatusCode::SUCCESS;

  // get McIntegratingHit collection. Abort if empty.
  SmartDataPtr<Event::McIntegratingHitVector> 
    McCalHits(eventSvc(), EventModel::MC::McIntegratingHitCol );

  if (McCalHits == 0) {
    // create msglog only when needed for speed.
    MsgStream msglog(msgSvc(), name());
    msglog << MSG::DEBUG; 
    if (msglog.isActive()){ 
      msglog.stream() << "no cal hits found" ;} 
    msglog << endreq;
    return sc;
  }

  // loop over hits - pick out CAL hits
  for (Event::McIntegratingHitVector::const_iterator it = McCalHits->begin(); it != McCalHits->end(); it++) {

    //   extracting hit parameters - get energy and first moment
    idents::VolumeIdentifier volId = 
      ((idents::VolumeIdentifier)(*it)->volumeID());

    //   extracting parameters from volume Id identifying as in CAL
    if ((int)volId[fLATObjects]   == m_eLATTowers &&
        (int)volId[fTowerObjects] == m_eTowerCAL){ 

      int col   = volId[fCALXtal];
      int lyr   = volId[fLayer];
      int towy  = volId[fTowerY];
      int towx  = volId[fTowerX];
      int twr   = m_xNum*towy+towx; 

      if (msgSvc()->outputLevel(name()) <= MSG::VERBOSE) {
        MsgStream msglog(msgSvc(), name());
        msglog << MSG::VERBOSE << "McIntegratingHits info \n"  
               << " ID " << volId.name() << endreq;
        msglog << MSG::VERBOSE <<  "MC Hit "  
               << " col "   << col
               << " layer " << lyr
               << " towy "  << towy
               << " towx "  << towx
               << endreq;
      }

      CalXtalId mapId(twr,lyr,col);

      // Insertion of the id - McIntegratingHit pair
      m_idMcInt.insert(make_pair(mapId,*it));
      m_idMcIntPreDigi[mapId].push_back((const_cast<Event::McIntegratingHit*> 
                                         (*it)));
    }
  }

  return sc;
}
