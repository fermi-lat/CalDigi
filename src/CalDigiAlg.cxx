/**
 * @file CalDigiAlg.cxx
 * @brief implementation  of the algorithm CalDigiAlg.
 *
 * @author Zach Fewtrell zachary.fewtrell@nrl.navy.mil
 * 
 *  $Header: /nfs/slac/g/glast/ground/cvs/CalDigi/src/CalDigiAlg.cxx,v 1.63 2008/08/07 20:19:09 fewtrell Exp $
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
#include "CalXtalResponse/ICalDiagnosticTool.h"
#include "CalUtil/CalGeom.h"                     // findActiveTowers()
#include "ConfigSvc/IConfigSvc.h"
#include "configData/gem/TrgConfig.h" 

// Relational Table
#include "Event/RelTable/RelTable.h"

// std stuff


// Define the factory for this algorithm
//static const AlgFactory<CalDigiAlg>  Factory;
//const IAlgFactory& CalDigiAlgFactory = Factory;
DECLARE_ALGORITHM_FACTORY(CalDigiAlg);

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
  m_configSvc(0),
  m_firstRng("autoRng"),
  m_calDiagnosticTool(0)
{

  // Declare the properties that may be set in the job options file
  declareProperty("CalSignalToolName",   m_calSignalToolName = "CalSignalTool");
  declareProperty("XtalDigiToolName",    m_xtalDigiToolName = "XtalDigiTool");
  declareProperty("ConfigSvcName",    m_configSvcName = "ConfigSvc");
  declareProperty("DefaultZeroSuppress", m_defaultZeroSuppress = true);
  declareProperty("DefaultAllRange",     m_defaultAllRange = false);
  declareProperty("FirstRangeReadout",   m_firstRng= "autoRng"); 
  declareProperty("CreateDiagnosticData", m_createDiagnosticData="false");
  declareProperty("CalDiagnosticToolName", m_calDiagnosticToolName="CalDiagnosticTool");
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

  sc = toolSvc()->retrieveTool("XtalDigiTool",
                               m_xtalDigiToolName, 
                               m_xtalDigiTool, 
                               this); // not shared
  if (sc.isFailure() ) {
    msglog << MSG::ERROR << "  Unable to create " << m_xtalDigiToolName << endreq;
    return sc;
  }
  
  sc = toolSvc()->retrieveTool("CalSignalTool",
                               m_calSignalToolName,
                               m_calSignalTool,
                               0); // intended to be shared
  if (sc.isFailure() ) {
    msglog << MSG::ERROR << "  can't create " << m_calSignalToolName << endreq;
    return sc;
  }

  sc = toolSvc()->retrieveTool("CalDiagnosticTool",
                               m_calDiagnosticToolName,
                               m_calDiagnosticTool,
                               0); // intended to be shared
  if (sc.isFailure() ) {
    msglog << MSG::ERROR << "  can't create " << m_calDiagnosticToolName << endreq;
    return sc;
  }



  sc = retrieveConstants();
  if (sc.isFailure())
    return sc;

  //-- find out which tems are installed.
  m_twrList = CalUtil::findActiveTowers(*m_detSvc);

  /// locate optional ConfigSvc
  if (m_configSvcName.value().length() != 0) {
    sc = service("ConfigSvc", m_configSvc, true); 
    if (sc.isFailure())
      m_configSvc = 0; //
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

    * CalDigiAlg takes perrforms the following steps:
    * - invoke CalSignalTool to sum all McIntegratingHits into Cal crystal diodes, either by CsI scintillation or direct diode deposit.
    * - call ConfigSvc to determine readout mode (allrange/bestrange , zeroSupression) for current event digis.
    * - call CalXtalResponse/IXtalDigiTool to generate CalDigis for individual crystals
    * - ignore crystals under LAC threshold if zeroSuppression is requested by trigger configuration.
    * - store McIntegratingHit <> CalDigi relations to file
    * - optionall generate Cal Diagnostic data words with ICalDiagnosticTool
    * - save CalDigi info TDS

    */
StatusCode CalDigiAlg::registerDigis() {
  StatusCode  sc;

  // check for McIntegratingHit collection. Abort if empty.
  SmartDataPtr<Event::McIntegratingHitVector> 
    McCalHits(eventSvc(), EventModel::MC::McIntegratingHitCol );
  if (McCalHits == 0)
    return StatusCode::SUCCESS;

  // input list of xtalIdx <> mchit relations from CalSignalTool
  const ICalSignalTool::CalRelationMap *xtalMcRelMap = m_calSignalTool->getCalRelationMap();
  if (xtalMcRelMap == 0) {
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
  
  /// TDS CalDigi collection
  auto_ptr<Event::CalDigiCol> digiCol(new Event::CalDigiCol());

  // TDS list of digi <> mchit relations
  CalDigiMcRelMap digiMcRelMap;
  digiMcRelMap.init();

  // collection of xtal digis for entire event.
  if (genDigis(calTrigMode, 
               zeroSupp,
               *xtalMcRelMap,
               *digiCol,
               digiMcRelMap).isFailure())
    return StatusCode::FAILURE;


  /// register CalDigi in TDS
  sc = eventSvc()->registerObject(EventModel::Digi::CalDigiCol, digiCol.release());
  if (sc.isFailure())
    return sc;


  // register mc <> digi relations relations
  sc = eventSvc()->registerObject(EventModel::Digi::CalDigiHitTab, 
                                  digiMcRelMap.getAllRelations());
  if (sc.isFailure()) return sc;

  if (m_createDiagnosticData)
    if (registerDiagnosticData().isFailure())
      return StatusCode::FAILURE;

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
  /// if ConfigSvc is not available, return default answer
  if (m_configSvc != 0) {
  
    // get trigger word
    SmartDataPtr<Event::EventHeader> evtHdr(eventSvc(), EventModel::EventHeader);
    // skip to default answer if no event header info in TDS
    if (evtHdr != 0) {
      const unsigned gltWord = evtHdr->trigger();

      // skip to default answer if trigger = -1 (means it has not been set yet)
      if (gltWord != 0xffffffff) {
        // get readout mode from trigger context
        TrgConfig const*const tcf  = m_configSvc->getTrgConfig();
        const unsigned gltengine   = tcf->lut()->engineNumber(gltWord&31); 

        // set readout mode bits
        zeroSupp= tcf->trgEngine()->zeroSuppression(gltengine);
        rangeType = (tcf->trgEngine()->fourRangeReadout(gltengine)) ?
          idents::CalXtalId::ALLRANGE : idents::CalXtalId::BESTRANGE;

        /// final, sucessful data path, all other lead to defaults.
        return StatusCode::SUCCESS;
      }
    }
  }
  
  // RETURN DEFAULTS if  no success
  rangeType = (m_defaultAllRange) ? idents::CalXtalId::ALLRANGE : idents::CalXtalId::BESTRANGE;
  zeroSupp = m_defaultZeroSuppress;
  return StatusCode::SUCCESS;
}


StatusCode CalDigiAlg::genDigis(const idents::CalXtalId::CalTrigMode calTrigMode,
                                const bool zeroSupp,
                                const ICalSignalTool::CalRelationMap &xtalMcRelMap,
                                Event::CalDigiCol &digiCol,
                                CalDigiMcRelMap &digiMcRelMap
                                ) {
  
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

        // store log-accept results
        CalUtil::CalVec<CalUtil::FaceNum, bool> lacBits;
        
        // new digi for this xtal
        // auto_ptr will automatically delete it if ownership of object
        // is not passed on to TDS data
        auto_ptr<Event::CalDigi> curDigi(new Event::CalDigi(calTrigMode, mapId));     

        //-- get signal map--//
        // look up xtal in hit map
                
        if(m_xtalDigiTool->calculate(*curDigi,
                                     lacBits,
                                     zeroSupp,
                                     m_firstRng).isFailure())
          return StatusCode::FAILURE;

        // move on to next xtal if there is no log-accept.
        if (zeroSupp &&
            find(lacBits.begin(), lacBits.end(), true) == lacBits.end()) continue;

        // register this crystal <> MC relationship
        // find all hits for this xtal
        typedef ICalSignalTool::CalRelationMap::const_iterator CalRelationIt;
        const CalUtil::XtalIdx xtalIdx(mapId);
        pair<CalRelationIt, CalRelationIt> xtalHitMatches(xtalMcRelMap.equal_range(xtalIdx));
        for (CalRelationIt it(xtalHitMatches.first);
             it != xtalHitMatches.second;
             it++)
          digiMcRelMap.addRelation(new Event::Relation<Event::CalDigi, 
                                   Event::McIntegratingHit>(curDigi.get(),
                                                            it->second));
        
        // add the digi to the digi collection
        digiCol.push_back(curDigi.release());

      } // col loop
  } // twr loop
                                                
  return StatusCode::SUCCESS;
}

StatusCode CalDigiAlg::registerDiagnosticData() {
  /// retrieve TDS Diagnostic Data
  SmartDataPtr<LdfEvent::DiagnosticData> diagTds(eventSvc(), "/Event/Diagnostic");

  // check to see if diagnostic data is present in Event.
  if (!diagTds) {
    // if not present, then create a new one
    LdfEvent::DiagnosticData *diagData = new LdfEvent::DiagnosticData();

    // register the new object.
    if (eventSvc()->registerObject("/Event/Diagnostic", diagData).isFailure()) {
      MsgStream msglog(msgSvc(), name());
      msglog << MSG::ERROR << "could not register " << "/Event/Diagnostic" << endreq;
      return StatusCode::FAILURE;
    }
    
    // reset SmartDataPtr
    diagTds = SmartDataPtr<LdfEvent::DiagnosticData>(eventSvc(), "/Event/Diagnostic");
  }


  /// loop through all installed towers
  for (vector<TwrNum>::const_iterator twrIt(m_twrList.begin());
       twrIt != m_twrList.end();
       twrIt++) {
    /// loop on each layer in tower
    for (LyrNum lyr; lyr.isValid(); lyr++) {
      auto_ptr<LdfEvent::CalDiagnosticData> calDiag(m_calDiagnosticTool->getDiagnosticData(*twrIt, lyr));
      if (calDiag.get() == 0)
        return StatusCode::FAILURE;

      /// diagTds will store a copy of my CalDiagnosticData class, i
      /// am free to delete this object after this call
      diagTds->addCalDiagnostic(*(calDiag.get()));
    }
  }
  
  return StatusCode::SUCCESS;
}

