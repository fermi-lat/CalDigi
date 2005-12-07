/**
 * @file CalDigiAlg.cxx
 * @brief implementation  of the algorithm CalDigiAlg.
 *
 *  $Header: /nfs/slac/g/glast/ground/cvs/CalDigi/src/CalDigiAlg.cxx,v 1.46 2005/10/05 19:33:46 fewtrell Exp $
 */
// LOCAL include files
#include "CalDigiAlg.h"

// Gaudi specific include files
#include "GaudiKernel/MsgStream.h"
#include "GaudiKernel/AlgFactory.h"
#include "GaudiKernel/IDataProviderSvc.h"
#include "GaudiKernel/SmartDataPtr.h"
#include "GaudiKernel/IToolSvc.h"

// Glast specific includes
#include "Event/TopLevel/Event.h"
#include "Event/TopLevel/EventModel.h"
#include "Event/TopLevel/DigiEvent.h"
#include "Event/Digi/CalDigi.h"
#include "Event/Digi/GltDigi.h"
#include "GaudiKernel/ObjectVector.h"
#include "CalUtil/CalDefs.h"

// Relational Table
#include "Event/RelTable/Relation.h"
#include "Event/RelTable/RelTable.h"

// std stuff
#include <utility>
#include <string>
#include <iomanip>
#include <algorithm>
#include <cmath>
#include <typeinfo>

// Define the factory for this algorithm
static const AlgFactory<CalDigiAlg>  Factory;
const IAlgFactory& CalDigiAlgFactory = Factory;

using namespace std;
using idents::CalXtalId;
using namespace CalUtil;

/// construct object & declare jobOptions
CalDigiAlg::CalDigiAlg(const string& name, ISvcLocator* pSvcLocator) :
  Algorithm(name, pSvcLocator),
  m_rangeMode(CalXtalId::BESTRANGE)
{

  // Declare the properties that may be set in the job options file
  declareProperty("XtalDigiToolName", m_xtalDigiToolName = "XtalDigiTool");
  declareProperty("CalTrigToolName",  m_calTrigToolName  = "CalTrigTool");
  declareProperty("RangeType",        m_rangeTypeStr     = "BEST");
  declareProperty("GetEvtHdr",        m_getEvtHdr        = false);
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
  if (sc.isFailure()) {
    msglog << MSG::ERROR << "Could not set jobOptions properties" << endreq;
    return sc;
  }
    
  if (m_rangeTypeStr.value() == "BEST") m_rangeMode = CalXtalId::BESTRANGE;
  else m_rangeMode = CalXtalId::ALLRANGE;

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
  sc = toolSvc()->retrieveTool(m_xtalDigiToolName, m_xtalDigiTool, this);
  if (sc.isFailure() ) {
    msglog << MSG::ERROR << "  Unable to create " << m_xtalDigiToolName << endreq;
    return sc;
  }

  // this tool needs to be shared by CalDigiAlg, XtalDigiTool & TriggerAlg, so I am
  // giving it global ownership
  sc = toolSvc()->retrieveTool(m_calTrigToolName, m_calTrigTool);
  if (sc.isFailure() ) {
    msglog << MSG::ERROR << "  Unable to create " << m_calTrigToolName << endreq;
    return sc;
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
    volId.append(0); // layer
    volId.append(m_eMeasureX);
    volId.append(0); // column
    volId.append(m_eXtal);
    volId.append(0); // segment Id

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
  if (sc.isFailure()) return sc;

  sc = createDigis();
  if (sc.isFailure()) return sc;

  return sc;
}

/** \brief Loop through each existing xtal & generate digis.

per xtal digi simulation is done w/ CalXtalResponse::XtalDigiTool();
also populate mcHit->Digi relational table.
also populate (&generate if needed) GltDigi TDS class w/ CALLO & CALHI tirgge
also register TDS digi data.
*/
StatusCode CalDigiAlg::createDigis() { 
  StatusCode  sc = StatusCode::SUCCESS;

  // collection of xtal digis for entire event.
  Event::CalDigiCol* digiCol = new Event::CalDigiCol;

  Event::RelTable<Event::CalDigi, Event::McIntegratingHit> digiHit;
  digiHit.init();
  
  // used for xtals w/ no hits.
  vector<const Event::McIntegratingHit*> nullList;

  // get pointer to EventHeader (has runId, evtId, etc...)
  // EventHeader                                                                                                                                                                                     
  
  Event::EventHeader *evtHdr = 0;
  if (m_getEvtHdr) {
    evtHdr = SmartDataPtr<Event::EventHeader>(eventSvc(),EventModel::EventHeader) ;
    if (!evtHdr) {
      MsgStream msglog( msgSvc(), name() );
      msglog<<MSG::ERROR<<"Event header not found !"<<endreq ;
    }
  }

  // search for GltDigi in TDS, create if needed
  Event::GltDigi* glt = m_calTrigTool->setupGltDigi(eventSvc());
  if (!glt) return StatusCode::FAILURE;

  CalArray<FaceNum, bool> lacBits;
  CalArray<XtalDiode, bool> trigBits;


  /* Loop through (installed) towers and crystals; collect up the McIntegratingHits by 
     xtal Id and send them off to xtalDigiTool to be digitized. Unhit crystals 
     can have noise added, so a null vector is sent in in that case.
  */
  for (unsigned twrSeq = 0; twrSeq < m_twrList.size(); twrSeq++) {
    // get bay id of nth live tower
    TwrNum twr(m_twrList[twrSeq]);
    for (LyrNum lyr; lyr.isValid(); lyr++) {
      for (ColNum col; col.isValid(); col++) {

        // assemble current calXtalId
        const CalXtalId mapId(twr,lyr,col);

        // list of mc hits for this xtal.
        vector<const Event::McIntegratingHit*>* hitList;

        // find this crystal in the existing list of McIntegratingHit vs Id map. 
        // If not found use null vector to see if a noise hit needs to be added.
        PreDigiMap::iterator hitListIt = m_idMcIntPreDigi.find(mapId);

        // we still process empty xtals (noise simulation may cause hits)
        if (hitListIt == m_idMcIntPreDigi.end())
          hitList = &nullList;
        else hitList = &(m_idMcIntPreDigi[mapId]);

        // note - important to reinitialize to 0 for each iteration
        lacBits.fill(false);
        trigBits.fill(false);
        
        // new digi for this xtal
        // auto_ptr will automatically delete it if ownership of object
        // is not passed on to TDS data
        auto_ptr<Event::CalDigi> curDigi(new Event::CalDigi(m_rangeMode, mapId));     
        
        sc = m_xtalDigiTool->calculate(*hitList,
                                       (m_getEvtHdr) ? evtHdr : NULL,
                                       *curDigi,
                                       lacBits,
                                       trigBits,
                                       glt
                                       );
        if (sc.isFailure()) continue;   // bad hit

        // move on to next xtal if there is no log-accept.
        if (lacBits.find(true) == lacBits.end()) continue;
        
        // set up the relational table between McIntegratingHit and digis
        typedef multimap< CalXtalId, Event::McIntegratingHit* >::const_iterator ItHit;
        pair<ItHit,ItHit> itpair = m_idMcInt.equal_range(mapId);

        for (ItHit mcit = itpair.first; mcit!=itpair.second; mcit++)
          digiHit.addRelation(new Event::Relation<Event::CalDigi,Event::McIntegratingHit>(curDigi.get(),mcit->second));

        // add the digi to the digi collection
        digiCol->push_back(curDigi.release());
      } // col loop
    } // lyr loop
  } // twr loop

  sc = eventSvc()->registerObject(EventModel::Digi::CalDigiCol, digiCol);
  if (sc.isFailure()) {
    delete digiCol;
    return sc;
  }

  sc = eventSvc()->registerObject(EventModel::Digi::CalDigiHitTab,digiHit.getAllRelations());
  if (sc.isFailure()) return sc;

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

      CalXtalId mapId(volId);

      // Insertion of the id - McIntegratingHit pair
      m_idMcInt.insert(make_pair(mapId,*it));
      m_idMcIntPreDigi[mapId].push_back((const_cast<Event::McIntegratingHit*> 
                                         (*it)));
    }
  }

  return sc;
}

StatusCode CalDigiAlg::finalize() {
  MsgStream msglog(msgSvc(), name());
  msglog << MSG::INFO << "finalize" << endreq;
  
  if (m_xtalDigiTool)
    m_xtalDigiTool->finalize();

  return StatusCode::SUCCESS;
}
