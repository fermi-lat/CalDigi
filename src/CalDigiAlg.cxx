#include "CalDigiAlg.h"
#include "CalUtil/CalFailureModeSvc.h"
/// Gaudi specific include files
#include "GaudiKernel/MsgStream.h"
#include "GaudiKernel/AlgFactory.h"
#include "GaudiKernel/IDataProviderSvc.h"
#include "GaudiKernel/SmartDataPtr.h"
#include "GaudiKernel/IToolSvc.h"

/// Glast specific includes
#include "Event/TopLevel/EventModel.h"
#include "Event/TopLevel/DigiEvent.h"
#include "GaudiKernel/ObjectVector.h"

// Relational Table
#include "Event/RelTable/Relation.h"
#include "Event/RelTable/RelTable.h"
#include "Event/Digi/CalDigi.h"
#include "CLHEP/Random/RandGauss.h"
/// for min and floor functions
#include <algorithm>
#include <cmath>

// std stuff
#include <utility>
#include <string>

// Define the factory for this algorithm
static const AlgFactory<CalDigiAlg>  Factory;
const IAlgFactory& CalDigiAlgFactory = Factory;


// Algorithm parameters which can be set at run time must be declared.
// This should be done in the constructor.


CalDigiAlg::CalDigiAlg(const std::string& name, ISvcLocator* pSvcLocator) :
Algorithm(name, pSvcLocator) {


    // Declare the properties that may be set in the job options file
    declareProperty ("xmlFile", m_xmlFile="$(CALDIGIROOT)/xml/CalDigi.xml");
    declareProperty ("convertAdcToolName", m_convertAdcToolName="TestAdcTool");
    declareProperty ("doFluctuations", m_doFluctuations="yes");
    declareProperty ("RangeType", m_rangeType="BEST");
}

StatusCode CalDigiAlg::initialize() {
    // Purpose and Method: initialize the algorithm. Set up parameters from detModel
    // Inputs: detModel parameters

    MsgStream log(msgSvc(), name());
    log << MSG::INFO << "initialize" << endreq;

    double value;
    typedef std::map<int*,std::string> PARAMAP;

    PARAMAP param;
    param[&m_xNum]=        std::string("xNum");
    param[&m_yNum]=        std::string("yNum");
    param[&m_eTowerCal]=   std::string("eTowerCAL");
    param[&m_eLatTowers]=  std::string("eLATTowers");
    param[&m_CalNLayer]=   std::string("CALnLayer");
    param[&m_nCsIPerLayer]=std::string("nCsIPerLayer");
    param[&m_nCsISeg]= std::string("nCsISeg");


    // now try to find the GlastDevSvc service

    IGlastDetSvc* detSvc;
    StatusCode sc = service("GlastDetSvc", detSvc);
    if (sc.isFailure() ) {
        log << MSG::ERROR << "  Unable to get GlastDetSvc " << endreq;
        return sc;
    }

    for(PARAMAP::iterator it=param.begin(); it!=param.end();it++){
        if(!detSvc->getNumericConstByName((*it).second, &value)) {
            log << MSG::ERROR << " constant " <<(*it).second <<" not defined" << endreq;
            return StatusCode::FAILURE;
        } else *((*it).first)=(int)value;
    }



    sc = toolSvc()->retrieveTool(m_convertAdcToolName,m_convertAdc);
    if (sc.isFailure() ) {
        log << MSG::ERROR << "  Unable to create " << m_convertAdcToolName << endreq;
        return sc;
    }

    m_doFluctuationsBool = (m_doFluctuations == "yes") ? 1 : 0;

    sc = service("CalFailureModeSvc", m_FailSvc);
    if (sc.isFailure() ) {
        log << MSG::INFO << "  Did not find CalFailureMode service" << endreq;
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
    MsgStream   log( msgSvc(), name() );


    //Take care of insuring that data area has been created
    DataObject* pNode = 0;
    sc = eventSvc()->retrieveObject( EventModel::Digi::Event /*"/Event/Digi"*/, pNode);

    if (sc.isFailure()) {
        sc = eventSvc()->registerObject(EventModel::Digi::Event /*"/Event/Digi"*/,new Event::DigiEvent);
        if( sc.isFailure() ) {
            log << MSG::ERROR << "could not register " << EventModel::Digi::Event /*<< /Event/Digi "*/ << endreq;
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

    MsgStream log(msgSvc(), name());
    log << MSG::INFO << "finalize" << endreq;

    return StatusCode::SUCCESS;
}

StatusCode CalDigiAlg::createDigis() {
    // Purpose and Method: 
    // create digis on the TDS from the deposited energies

    StatusCode  sc = StatusCode::SUCCESS;
    MsgStream log(msgSvc(), name());

    Event::CalDigiCol* digiCol = new Event::CalDigiCol;

    Event::RelTable<Event::CalDigi, Event::McIntegratingHit> digiHit;
    digiHit.init();

    // Loop through all towers and crystals; collect up the McIntegratingHits by xtal Id and 
    // send them off to convertAdcTool to be digitized. Unhit crystals can have noise added,
    // so a null vector is sent in in that case.

    for (int tower = 0; tower < m_xNum*m_yNum; tower++){
        for (int layer = 0; layer < m_CalNLayer; layer++){
            for (int col = 0; col < m_nCsIPerLayer; col++){

                const idents::CalXtalId mapId(tower,layer,col);

                std::vector<const Event::McIntegratingHit*> nullList;
                std::vector<const Event::McIntegratingHit*>* hitList;

                // find this crystal in the existing list of McIntegratingHit vs Id map. If not found
                // use a null vector to see if a noise hit needs to be added.

                PreDigiMap::iterator hitListIt=m_idMcIntPreDigi.find(mapId);

                if (hitListIt == m_idMcIntPreDigi.end()){ 
                    hitList = &nullList;
                }
                else hitList = &(m_idMcIntPreDigi[mapId]);

                idents::CalXtalId::AdcRange rangeP; // output - best range
                idents::CalXtalId::AdcRange rangeN;  // output - best range
                std::vector<int> adcP(4,0);              // output - ADC's for all ranges 0-3
                std::vector<int> adcN(4,0);              // output - ADC's for all ranges 0-3

                bool lacP, lacN;

                sc = m_convertAdc->calculate(mapId,
                    *hitList, // list of all mc hits for this xtal & it's diodes.
                    lacP,     // plus end above threshold
                    lacN,     // neg end above threshold
                    rangeP, // output - best range
                    rangeN,  // output - best range
                    adcP,              // output - ADC's for all ranges 0-3
                    adcN              // output - ADC's for all ranges 0-3
                    );
                // set status to ok for POS and NEG if no other bits set.


                unsigned short status = 0;

                if (!lacP && !lacN) continue;  // nothing more to see here. Move along.

                log << MSG::DEBUG; 
                if (log.isActive()){ 
                    log.stream() <<" id=" << mapId 
                        << " rangeP=" << int(rangeP) << " adcP=" << adcP[rangeP]
                        << " rangeN=" << int(rangeN) << " adcN=" << adcN[rangeN];
                } 
                log << endreq;

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
                typedef std::multimap< idents::CalXtalId, Event::McIntegratingHit* >::const_iterator ItHit;
                std::pair<ItHit,ItHit> itpair = m_idMcInt.equal_range(mapId);

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
    MsgStream log(msgSvc(), name());
    SmartDataPtr<Event::McIntegratingHitVector> McCalHits(eventSvc(),EventModel::MC::McIntegratingHitCol ); //"/Event/MC/IntegratingHitsCol");

    if (McCalHits == 0) {
        log << MSG::DEBUG; if (log.isActive()){ log.stream() << "no calorimeter hits found" ;} log << endreq;
        return sc;
    }

    // loop over hits - pick out CAL hits

    for (Event::McIntegratingHitVector::const_iterator it = McCalHits->begin(); it != McCalHits->end(); it++) {


        //   extracting hit parameters - get energy and first moment

        idents::VolumeIdentifier volId = ((idents::VolumeIdentifier)(*it)->volumeID());


        //   extracting parameters from volume Id identifying as in CAL

        if ((int)volId[fLATObjects] == m_eLatTowers &&
            (int)volId[fTowerObjects] == m_eTowerCal){ 

                //	log << MSG::DEBUG <<  "McIntegratingHits info \n"  
                //  << " ID " << volId.name()
                //  <<  " energy " << ene
                //  <<  " moments " << mom1.x()
                //  << endreq;

                int col = volId[fCALXtal];
                int layer = volId[fLayer];
                int towy = volId[fTowerY];
                int towx = volId[fTowerX];
                int tower = m_xNum*towy+towx; 

                idents::CalXtalId mapId(tower,layer,col);

                log << MSG::DEBUG; if (log.isActive()){ log.stream() <<  "Identifier decomposition \n"  
                    << " col " << col
                    << " layer " << layer
                    << " towy " << towy
                    << " towx " << towx
                    ;} log << endreq;


                // Insertion of the id - McIntegratingHit pair

                m_idMcInt.insert(std::make_pair(mapId,*it));

                m_idMcIntPreDigi[mapId].push_back((const_cast<Event::McIntegratingHit*>(*it)));
            }

    }

    return sc;
}
