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

// Algorithm parameters which can be set at run time must be declared.
// This should be done in the constructor.

CalDigiAlg::CalDigiAlg(const string& name, ISvcLocator* pSvcLocator) :
  Algorithm(name, pSvcLocator) {


  // Declare the properties that may be set in the job options file
  declareProperty ("xtalADCToolName", m_xtalADCToolName="XtalADCTool");
  declareProperty ("doFluctuations", m_doFluctuations="yes");
  declareProperty ("RangeType", m_rangeType="BEST");
}

StatusCode CalDigiAlg::initialize() {
  // Purpose and Method: initialize the algorithm. Set up parameters from detModel
  // Inputs: detModel parameters

  MsgStream msglog(msgSvc(), name());
  StatusCode sc;
  
  msglog << MSG::INFO << "initialize" << endreq;

  // Get properties from the JobOptionsSvc
  sc = setProperties();
  if ( !sc.isSuccess() ) {
    msglog << MSG::ERROR << "Could not set jobOptions properties" << endreq;
    return sc;
  }

  double value;
  typedef map<int*,string> PARAMAP;

  PARAMAP param;
  param[&m_xNum]=        string("xNum");
  param[&m_yNum]=        string("yNum");
  param[&m_eTowerCal]=   string("eTowerCAL");
  param[&m_eLatTowers]=  string("eLATTowers");
  param[&m_CalNLayer]=   string("CALnLayer");
  param[&m_nCsIPerLayer]=string("nCsIPerLayer");

  // now try to find the GlastDevSvc service

  IGlastDetSvc* detSvc;
  sc = service("GlastDetSvc", detSvc);
  if (sc.isFailure() ) {
    msglog << MSG::ERROR << "  Unable to get GlastDetSvc " << endreq;
    return sc;
  }

  for(PARAMAP::iterator it=param.begin(); it!=param.end();it++){
    if(!detSvc->getNumericConstByName((*it).second, &value)) {
      msglog << MSG::ERROR << " constant " <<(*it).second <<" not defined" << endreq;
      return StatusCode::FAILURE;
    } else *((*it).first)=(int)value;
  }

  sc = toolSvc()->retrieveTool(m_xtalADCToolName,m_xtalADCTool);
  if (sc.isFailure() ) {
    msglog << MSG::ERROR << "  Unable to create " << m_xtalADCToolName << endreq;
    return sc;
  }

  m_doFluctuationsBool = (m_doFluctuations == "yes") ? 1 : 0;

  sc = service("CalFailureModeSvc", m_FailSvc);
  if (sc.isFailure() ) {
    msglog << MSG::INFO << "  Did not find CalFailureMode service" << endreq;
    m_FailSvc = 0;
  }


  return StatusCode::SUCCESS;
}


StatusCode CalDigiAlg::execute() {

  // Purpose and Method: take Hits from McIntegratingHit and perform the following steps:
  //  for deposit in a crystal segment, take into account light propagation to the two ends and 
  // apply light taper based on position along the length.
  //  keep track of direct deposit in the diode.
  // Then combine diode (with appropriate scale factor) and crystal deposits and add noise.
  // Then, add noise to 'unhit' crystals.
  //  Finally, convert to ADC units and pick the range for hits above threshold.
  // Inputs: McIntegratingHit

  StatusCode  sc = StatusCode::SUCCESS;
  
  //Take care of insuring that data area has been created
  DataObject* pNode = 0;
  sc = eventSvc()->retrieveObject( EventModel::Digi::Event /*"/Event/Digi"*/, pNode);

  if (sc.isFailure()) {
    sc = eventSvc()->registerObject(EventModel::Digi::Event /*"/Event/Digi"*/,new Event::DigiEvent);
    if( sc.isFailure() ) {
      MsgStream   msglog( msgSvc(), name() );
      msglog << MSG::ERROR << "could not register " << EventModel::Digi::Event /*<< /Event/Digi "*/ << endreq;
      return sc;
    }
  }

  //  clear signal array: map relating xtal signal to id. Map holds diode and crystal responses
  //  separately during accumulation.

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

StatusCode CalDigiAlg::createDigis() {
  // Purpose and Method: 
  // create digis on the TDS from the deposited energies

  StatusCode  sc = StatusCode::SUCCESS;

  Event::CalDigiCol* digiCol = new Event::CalDigiCol;

  Event::RelTable<Event::CalDigi, Event::McIntegratingHit> digiHit;
  digiHit.init();

  // Loop through all towers and crystals; collect up the McIntegratingHits by xtal Id and 
  // send them off to xtalADCTool to be digitized. Unhit crystals can have noise added,
  // so a null vector is sent in in that case.

  for (int tower = 0; tower < m_xNum*m_yNum; tower++){
    for (int layer = 0; layer < m_CalNLayer; layer++){
      for (int col = 0; col < m_nCsIPerLayer; col++){

        const idents::CalXtalId mapId(tower,layer,col);

        vector<const Event::McIntegratingHit*> nullList;
        vector<const Event::McIntegratingHit*>* hitList;

        // find this crystal in the existing list of McIntegratingHit vs Id map. If not found
        // use a null vector to see if a noise hit needs to be added.

        PreDigiMap::iterator hitListIt=m_idMcIntPreDigi.find(mapId);

        if (hitListIt == m_idMcIntPreDigi.end()){ 
          hitList = &nullList;
        }
        else hitList = &(m_idMcIntPreDigi[mapId]);

        idents::CalXtalId::AdcRange rangeP; // output - best range
        idents::CalXtalId::AdcRange rangeN;  // output - best range
        vector<int> adcP(4,0);              // output - ADC's for all ranges 0-3
        vector<int> adcN(4,0);              // output - ADC's for all ranges 0-3

        bool lacP, lacN;
        bool peggedP, peggedN;

        sc = m_xtalADCTool->calculate(mapId,
                                      *hitList, // list of all mc hits for this xtal & it's diodes.
                                      lacP,     // plus end above threshold
                                      lacN,     // neg end above threshold
                                      rangeP, // output - best range
                                      rangeN,  // output - best range
                                      adcP,              // output - ADC's for all ranges 0-3
                                      adcN,              // output - ADC's for all ranges 0-3
                                      peggedP,
                                      peggedN
                                      );
        // set status to ok for POS and NEG if no other bits set.
        
        if (!((CalDefs::RngNum)rangeP).isValid() || !((CalDefs::RngNum)rangeN).isValid() ) {
          MsgStream msglog(msgSvc(), name());
          msglog << MSG::ERROR;
          if (msglog.isActive()){
            msglog.stream() <<"Range exceeded!!! id=" << mapId
                            << " rangeP=" << int(rangeP) << " adcP=" << setw(4) << adcP[rangeP] << " lacP=" << lacP
                            << " rangeN=" << int(rangeN) << " adcN=" << setw(4) << adcN[rangeN] << " lacN=" << lacN;
          }
          msglog << endreq;
          return StatusCode::FAILURE;
        }

        unsigned short status = 0;
        if (!lacP && !lacN) continue;  // nothing more to see here. Move along.

        // only create MsgStream() class if we have to since it's a costly operation to
        // do repeatedly
        if (msgSvc()->outputLevel(name()) <= MSG::DEBUG) {
          MsgStream msglog(msgSvc(), name());
          msglog.stream() << " id=" << mapId
                          << " rangeP=" << int(rangeP) << " adcP=" << setw(4) << adcP[rangeP] << " lacP=" << lacP
                          << " rangeN=" << int(rangeN) << " adcN=" << setw(4) << adcN[rangeN] << " lacN=" << lacN;
          msglog << endreq;
        } 
        // check for failure mode. If killed, set to zero and set DEAD bit

        if (m_FailSvc != 0) {   
          if (m_FailSvc->matchChannel(mapId,
                                      (idents::CalXtalId::POS))) {

            if (lacP) (status = status | Event::CalDigi::CalXtalReadout::DEAD_P);
          }
          if (m_FailSvc->matchChannel(mapId,
                                      (idents::CalXtalId::NEG))) {
            if (lacN) (status = status | Event::CalDigi::CalXtalReadout::DEAD_N);

          }
        }

        if ((status & 0x00FF) == 0) status = 
                                      (status | Event::CalDigi::CalXtalReadout::OK_P);
        if ((status & 0xFF00) == 0) status = 
                                      (status | Event::CalDigi::CalXtalReadout::OK_N);

        idents::CalXtalId::CalTrigMode rangeMode;
        int readoutLimit = 1;
        if (m_rangeType == "BEST") rangeMode = idents::CalXtalId::BESTRANGE;
        else  {
          rangeMode = idents::CalXtalId::ALLRANGE;
          readoutLimit = 4;
        }

        Event::CalDigi* curDigi = new Event::CalDigi(rangeMode, mapId);

        // set up the digi

        for (int iLim=0; iLim<readoutLimit; iLim++) {
          int inputRangeP = rangeP;
          int inputRangeN = rangeN;

          if(m_rangeType == "ALL") {
            inputRangeP = iLim;
            inputRangeN = iLim;
          }

          Event::CalDigi::CalXtalReadout read = Event::CalDigi::CalXtalReadout(inputRangeP, 
                                                                               adcP[inputRangeP], inputRangeN, adcN[inputRangeN], status);

          curDigi->addReadout(read);
        }

        // set up the relational table between McIntegratingHit and digis
        typedef multimap< idents::CalXtalId, Event::McIntegratingHit* >::const_iterator ItHit;
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

StatusCode CalDigiAlg::fillSignalEnergies() {
  // Purpose and Method: collect deposited energies from McIntegratingHits and store
  // in map sorted by XtalID. 
  // multimap used to associate mcIntegratingHit to id. There can be multiple
  // hits for the same id.  

  // get McIntegratingHit collection. Abort if empty.

  StatusCode  sc = StatusCode::SUCCESS;
  SmartDataPtr<Event::McIntegratingHitVector> McCalHits(eventSvc(),EventModel::MC::McIntegratingHitCol ); //"/Event/MC/IntegratingHitsCol");

  if (McCalHits == 0) {
    MsgStream msglog(msgSvc(), name());
    msglog << MSG::DEBUG; if (msglog.isActive()){ msglog.stream() << "no calorimeter hits found" ;} msglog << endreq;
    return sc;
  }

  // loop over hits - pick out CAL hits

  MsgStream msglog(msgSvc(), name());
  for (Event::McIntegratingHitVector::const_iterator it = McCalHits->begin(); it != McCalHits->end(); it++) {


    //   extracting hit parameters - get energy and first moment

    idents::VolumeIdentifier volId = ((idents::VolumeIdentifier)(*it)->volumeID());


    //   extracting parameters from volume Id identifying as in CAL

    if ((int)volId[fLATObjects] == m_eLatTowers &&
        (int)volId[fTowerObjects] == m_eTowerCal){ 

      msglog << MSG::DEBUG <<  "McIntegratingHits info \n"  
             << " ID " << volId.name()
             << endreq;

      int col = volId[fCALXtal];
      int layer = volId[fLayer];
      int towy = volId[fTowerY];
      int towx = volId[fTowerX];
      int tower = m_xNum*towy+towx; 

      idents::CalXtalId mapId(tower,layer,col);

      msglog << MSG::DEBUG; if (msglog.isActive()){ msglog.stream() <<  "Identifier decomposition \n"  
             << " col " << col
             << " layer " << layer
             << " towy " << towy
             << " towx " << towx
                                                      ;} msglog << endreq;


      // Insertion of the id - McIntegratingHit pair

      m_idMcInt.insert(make_pair(mapId,*it));

      m_idMcIntPreDigi[mapId].push_back((const_cast<Event::McIntegratingHit*>(*it)));
    }
  }

  return sc;
}
