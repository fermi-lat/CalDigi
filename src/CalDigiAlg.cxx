/**
 * @file CalDigiAlg.cxx
 * @brief implementation  of the algorithm CalDigiAlg.
 *
 * @author Zach Fewtrell zachary.fewtrell@nrl.navy.mil
 * 
 *  $Header: /nfs/slac/g/glast/ground/cvs/CalDigi/src/CalDigiAlg.cxx,v 1.54 2007/10/16 18:37:51 fewtrell Exp $
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
#include "CalUtil/CalDefs.h"
#include "GlastSvc/GlastDetSvc/IGlastDetSvc.h"
#include "CalXtalResponse/IXtalDigiTool.h"
#include "CalUtil/CalGeom.h"                     // findActiveTowers()
#include "Trigger/ITrgConfigSvc.h"
#include "configData/gem/TrgConfig.h" 

// Relational Table
#include "Event/RelTable/Relation.h"
#include "Event/RelTable/RelTable.h"

// std stuff


// Define the factory for this algorithm
static const AlgFactory<CalDigiAlg>  Factory;
const IAlgFactory& CalDigiAlgFactory = Factory;

using namespace std;
using idents::CalXtalId;
using namespace CalUtil;

/// construct object & declare jobOptions
CalDigiAlg::CalDigiAlg(const string& name, ISvcLocator* pSvcLocator) :
  Algorithm(name, pSvcLocator),
  m_eLATTowers(0),
  m_eTowerCAL(0), 
  m_eMeasureX(0),
  m_eXtal(0),
  m_xtalDigiTool(0),
  m_calSignalTool(0),
  m_detSvc(0),
  m_trgConfigSvc(0)
{

  // Declare the properties that may be set in the job options file
  declareProperty("CalSignalToolName",   m_calSignalToolName = "CalSignalTool");
  declareProperty("XtalDigiToolName",    m_xtalDigiToolName = "XtalDigiTool");
  declareProperty("TrgConfigSvcName",    m_trgConfigSvcName = "TrgConfigSvc");
  declareProperty("DefaultZeroSuppress", m_defaultZeroSuppress = true);
  declareProperty("DefaultAllRange",     m_defaultAllRange = false);
}

/// initialize the algorithm. retrieve helper tools & services
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
    
  ///////////////////////////////////////
  //-- RETRIEVE HELPER TOOLS & SVCS  --//
  ///////////////////////////////////////
  // try to find the GlastDetSvc service
  sc = service("GlastDetSvc", m_detSvc);
  if (sc.isFailure() ) {
    msglog << MSG::ERROR << "  can't get GlastDetSvc " << endreq;
    return sc;
  }

  sc = toolSvc()->retrieveTool(m_xtalDigiToolName, 
                               m_xtalDigiTool, 
                               this); // not shared
  if (sc.isFailure() ) {
    msglog << MSG::ERROR << "  Unable to create " << m_xtalDigiToolName << endreq;
    return sc;
  }
  
  sc = toolSvc()->retrieveTool(m_calSignalToolName,
                               m_calSignalTool,
                               0); // intended to be shared
  if (sc.isFailure() ) {
    msglog << MSG::ERROR << "  can't create " << m_calSignalToolName << endreq;
    return sc;
  }

  sc = retrieveConstants();
  if (sc.isFailure())
    return sc;

  //-- find out which tems are installed.
  m_twrList = CalUtil::findActiveTowers(*m_detSvc);

  /// locate optional TrgConfigSvc
  if (m_trgConfigSvcName.length() != 0) {
    sc = service("TrgConfigSvc", m_trgConfigSvc, true); 
    if (sc.isFailure())
      m_trgConfigSvc = 0; //
  }

  return StatusCode::SUCCESS;
}

/// \brief take Hits from McIntegratingHits, create & register CalDigis
StatusCode CalDigiAlg::execute() {
  StatusCode  sc;

  sc = ensureDigiEventExists();
  if (sc.isFailure())
    return sc;

  sc = registerDigis();
  if (sc.isFailure()) return sc;

  return StatusCode::SUCCESS;
}

StatusCode CalDigiAlg::ensureDigiEventExists() {
  //Take care of insuring that data area has been created
  DataObject* pNode = 0;
  StatusCode sc = eventSvc()->retrieveObject( EventModel::Digi::Event /*"/Event/Digi"*/, 
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

  return StatusCode::SUCCESS;
}


/** \brief Loop through each existing xtal & generate digis.

also populate mcHit->Digi relational table.
also register TDS digi data.

mc -> diode signal is done with CalSignalTool
diode signal -> digi generation is done w/ XtalDigiTool
*/
StatusCode CalDigiAlg::registerDigis() {
  StatusCode  sc;

  // collection of xtal digis for entire event.
  auto_ptr<Event::CalDigiCol> digiCol(new Event::CalDigiCol);

  // store log-accept results
  CalUtil::CalArray<CalUtil::FaceNum, bool> lacBits;

  // input list of xtalIdx <> mchit relations from CalSignalTool
  const ICalSignalTool::CalRelationMap *calRelMap = m_calSignalTool->getCalRelationMap();
  if (calRelMap == 0) {
    MsgStream msglog(msgSvc(), name());
    msglog << MSG::ERROR << "Can't retrieve CalRelationMap" << endreq;

    return StatusCode::FAILURE;
  }
  
  /// get trigger conditions
  idents::CalXtalId::CalTrigMode calTrigMode;
  bool zeroSupp;
  sc = getTrgConditions(calTrigMode, zeroSupp);
  if (sc.isFailure())
    return sc;

  
  // TDS list of digi <> mchit relations
  Event::RelTable<Event::CalDigi, Event::McIntegratingHit> finalRelMap;
  finalRelMap.init();

  /* Loop through (installed) towers and crystals; retrieve signal for each
  */
  for (unsigned twrSeq = 0; twrSeq < m_twrList.size(); twrSeq++) {
    // get bay id of nth live tower
    const TwrNum twr(m_twrList[twrSeq]);
    for (LyrNum lyr; lyr.isValid(); lyr++)
      for (ColNum col; col.isValid(); col++) {

        // assemble current calXtalId
        const CalXtalId mapId(twr.val(),
                              lyr.val(),
                              col.val());

        // note - important to reinitialize to 0 for each iteration
        fill(lacBits.begin(), lacBits.end(), false);
        
        // new digi for this xtal
        // auto_ptr will automatically delete it if ownership of object
        // is not passed on to TDS data
        auto_ptr<Event::CalDigi> curDigi(new Event::CalDigi(calTrigMode, mapId));     

        //-- get signal map--//
        // look up xtal in hit map
        const CalUtil::XtalIdx xtalIdx(mapId);
        ICalSignalTool::XtalSignalMap xtalSignalMap;
        sc = m_calSignalTool->getXtalSignalMap(xtalIdx, xtalSignalMap);
        if (sc.isFailure())
          return sc;
        
        
        sc = m_xtalDigiTool->calculate(xtalSignalMap,
                                       *curDigi,
                                       lacBits,
                                       zeroSupp);
        if (sc.isFailure()) 
          return sc;

        // move on to next xtal if there is no log-accept.
        if (zeroSupp &&
            find(lacBits.begin(), lacBits.end(), true) == lacBits.end()) continue;

        // register this crystal <> MC relationship
        // find all hits for this xtal
        typedef ICalSignalTool::CalRelationMap::const_iterator CalRelationIt;
        pair<CalRelationIt, CalRelationIt> xtalHitMatches(calRelMap->equal_range(xtalIdx));
        for (CalRelationIt it(xtalHitMatches.first);
             it != xtalHitMatches.second;
             it++)
          finalRelMap.addRelation(new Event::Relation<Event::CalDigi, 
                                  Event::McIntegratingHit>(curDigi.get(),
                                                           it->second));

        // add the digi to the digi collection
        digiCol->push_back(curDigi.release());

      } // col loop
  } // twr loop


  /// register CalDigi in TDS
  sc = eventSvc()->registerObject(EventModel::Digi::CalDigiCol, digiCol.release());
  if (sc.isFailure())
    return sc;


  // register mc <> digi relations relations
  sc = eventSvc()->registerObject(EventModel::Digi::CalDigiHitTab,finalRelMap.getAllRelations());
  if (sc.isFailure()) return sc;

  return StatusCode::SUCCESS;
}

StatusCode CalDigiAlg::retrieveConstants() {
  double value;
  typedef map<int*,string> PARAMAP;


  PARAMAP param;
  param[&m_eTowerCAL]  =    string("eTowerCAL");
  param[&m_eLATTowers] =    string("eLATTowers");
  param[&m_eXtal]      =    string("eXtal");
  param[&m_eMeasureX] =    string("eMeasureX");


  for(PARAMAP::iterator it=param.begin(); it!=param.end();it++){
    if(!m_detSvc->getNumericConstByName((*it).second, &value)) {
      MsgStream msglog(msgSvc(), name());
      msglog << MSG::ERROR << " constant " <<(*it).second 
             <<" not defined" << endreq;
      return StatusCode::FAILURE;
    } else *((*it).first)=(int)value;
  }

  return StatusCode::SUCCESS;
}

StatusCode CalDigiAlg::getTrgConditions(idents::CalXtalId::CalTrigMode &rangeType,
                                        bool &zeroSupp) {
  /// if trgConfigSvc is not available, return default answer
  if (m_trgConfigSvc == 0) {
    rangeType = (m_defaultAllRange) ? idents::CalXtalId::ALLRANGE : idents::CalXtalId::BESTRANGE;
    zeroSupp = m_defaultZeroSuppress;
    return StatusCode::SUCCESS;
  }
  
  // get trigger word
  SmartDataPtr<Event::EventHeader> evtHdr(eventSvc(), EventModel::EventHeader);
  if (!evtHdr) {
    MsgStream msglog(msgSvc(), name());
    msglog << MSG::ERROR << "Failed to retrieve Event" << endreq;
    return StatusCode::FAILURE;
  }

  const unsigned gltWord = evtHdr->trigger();
  
  // get readout mode from trigger context
  TrgConfig const*const tcf  = m_trgConfigSvc->getTrgConfig();
  const unsigned gltengine   = tcf->lut()->engineNumber(gltWord&31); 
  zeroSupp= tcf->trgEngine()->zeroSuppression(gltengine);
  rangeType = (tcf->trgEngine()->fourRangeReadout(gltengine)) ?
    idents::CalXtalId::ALLRANGE : idents::CalXtalId::BESTRANGE;

  return StatusCode::SUCCESS;
}
