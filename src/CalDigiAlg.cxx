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
#include "GaudiKernel/ObjectVector.h"


// Relational Table
#include "Event/RelTable/Relation.h"
#include "Event/RelTable/RelTable.h"

#include "Event/Digi/CalDigi.h"
#include "CLHEP/Random/RandGauss.h"

/// for min and floor functions
#include <algorithm>
#include <math.h>

// to access an XML containing Digi parameters file
#include "xml/IFile.h"

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
    declareProperty ("taperToolName", m_taperToolName="LinearTaper");
    declareProperty ("doFluctuations", m_doFluctuations="yes");
}


StatusCode CalDigiAlg::initialize() {
    // Purpose and Method: initialize the algorithm. Set up parameters from detModel
    // Inputs: detModel parameters
    
    MsgStream log(msgSvc(), name());
    log << MSG::INFO << "initialize" << endreq;
    
    // extracting int constants from detModel. Store into local cache - member variables.
    
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
    param[&m_eXtal]=       std::string("eXtal");
    param[&m_eDiodeMSmall]=std::string("eDiodeMSmall");
    param[&m_eDiodePSmall]=std::string("eDiodePSmall");
    param[&m_eDiodeMLarge]=std::string("eDiodeMLarge");
    param[&m_eDiodePLarge]=std::string("eDiodePLarge");
    param[&m_eMeasureX]=std::string("eMeasureX");
    param[&m_eMeasureY]=std::string("eMeasureY");
    param[m_noise]=std::string("cal.noiseLarge");
    param[m_noise+1]=std::string("cal.noiseSmall");
    param[m_ePerMeV+1]=std::string("cal.ePerMeVSmall");
    param[m_ePerMeV]=std::string("cal.ePerMevLarge");
    param[&m_pedestal]=std::string("cal.pedestal");
    param[&m_maxAdc]=std::string("cal.maxAdcValue");
    
    // now try to find the GlastDevSvc service
    
    IGlastDetSvc* detSvc;
    StatusCode sc = service("GlastDetSvc", detSvc);
    
    for(PARAMAP::iterator it=param.begin(); it!=param.end();it++){
        if(!detSvc->getNumericConstByName((*it).second, &value)) {
            log << MSG::ERROR << " constant " <<(*it).second <<" not defined" << endreq;
            return StatusCode::FAILURE;
        } else *((*it).first)=(int)value;
    }
    
    
    // extracting double constants from detModel. Store into local cache - member variables.
    
    typedef std::map<double*,std::string> DPARAMAP;
    DPARAMAP dparam;
    dparam[m_maxEnergy]=std::string("cal.maxResponse0");
    dparam[m_maxEnergy+1]=std::string("cal.maxResponse1");
    dparam[m_maxEnergy+2]=std::string("cal.maxResponse2");
    dparam[m_maxEnergy+3]=std::string("cal.maxResponse3");
    dparam[&m_lightAtt]=std::string("cal.lightAtt");
    dparam[&m_CsILength]=std::string("CsILength");
    dparam[&m_thresh]=std::string("cal.zeroSuppressEnergy");
    
    for(DPARAMAP::iterator dit=dparam.begin(); dit!=dparam.end();dit++){
        if(!detSvc->getNumericConstByName((*dit).second,(*dit).first)) {
            log << MSG::ERROR << " constant " <<(*dit).second << " not defined" << endreq;
            return StatusCode::FAILURE;
        } 
    }
    
    
    // scale max energies and thresholds from GeV to MeV
    //for (int r=0; r<4;r++) m_maxEnergy[r] *= 1000.; 
    //m_thresh *= 1000.;
    
    // Read in the parameters from the XML file
    xml::IFile m_ifile(m_xmlFile.c_str());
    if (m_ifile.contains("cal","ePerMeVinDiode")) 
        m_ePerMeVinDiode = m_ifile.getDouble("cal", "ePerMeVinDiode");
    else return StatusCode::FAILURE;
    
    
    sc = toolSvc()->retrieveTool(m_taperToolName,m_taper);
    if (sc.isFailure() ) {
        log << MSG::ERROR << "  Unable to create " << m_taperToolName << endreq;
        return sc;
    }
    
    m_doFluctuationsBool = (m_doFluctuations == "yes") ? 1 : 0;
    
    sc = service("CalFailureModeSvc", m_FailSvc);
    if (sc.isFailure() ) {
        log << MSG::INFO << "  Did not find CalFailureMode service" << endreq;
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
        sc = eventSvc()->registerObject(EventModel::Digi::Event /*"/Event/Digi"*/,new DataObject);
        if( sc.isFailure() ) {
            log << MSG::ERROR << "could not register " << EventModel::Digi::Event /*<< /Event/Digi "*/ << endreq;
            return sc;
        }
    }
    
    
    
    //  clear signal array: map relating xtal signal to id. Map holds diode and crystal responses
    //  separately during accumulation.
    
    m_signalMap.clear();
    m_idMcInt.clear();
    
    sc = fillSignalEnergies();
    if (sc != StatusCode::SUCCESS) return sc;
    
    sc = addNoiseToSignals();
    if (sc != StatusCode::SUCCESS) return sc;
    
    sc = addNewNoiseHits();
    if (sc != StatusCode::SUCCESS) return sc;
    
    
    
    log << MSG::DEBUG << m_signalMap.size() << "calorimeter hits in m_signalMap" << endreq;
    for( SignalMap::iterator jit=m_signalMap.begin(); jit!=m_signalMap.end();jit++){
        log << MSG::DEBUG << " id " << (*jit).first
            << " s0=" << (*jit).second.getSignal(0)
            << " s1=" << (*jit).second.getSignal(1)
            << " d0=" << (*jit).second.getDiodeEnergy(0)
            << " d1=" << (*jit).second.getDiodeEnergy(1)
            << " d2=" << (*jit).second.getDiodeEnergy(2)
            << " d3=" << (*jit).second.getDiodeEnergy(3)
            << endreq;
    }
    
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
    
    Event::CalDigiCol* digiCol = new Event::CalDigiCol;
    
    Event::RelTable<Event::CalDigi, Event::McIntegratingHit> digiHit;
    digiHit.init();
    
    // iterate through the SignalMap to look at only those Xtals that had energy deposited.
    // if either side is above threshold, then select the appropriate ADC range and
    // create a readout
    
    for(SignalMap::iterator nit=m_signalMap.begin(); nit!=m_signalMap.end();nit++){
        XtalSignal* signal = &(*nit).second;
        
        if(signal->getDiodeEnergy(0) > m_thresh 
            || signal->getDiodeEnergy(1) > m_thresh) {
            
            idents::CalXtalId xtalId = (*nit).first;
            
            // check for failure mode
            if (m_FailSvc != 0) {
                if (m_FailSvc->matchChannel(xtalId)) continue;
            }
            
            char rangeP,rangeM;
            unsigned short adcP,adcM;
            
            // loop over the plus and minus faces of the crystal 
            
            for (int face=0; face<2; face++){
                double resp;
                int br;
                
                // loop over the diodes and fetch the response
                for (int r = 0;r<4;r++){
                    int diode_type = r/2;
                    int diode = 2*diode_type+face;
                    resp = signal->getDiodeEnergy(diode);
                    
                    
                    br=r;
                    if( resp < m_maxEnergy[r]) break;														
                }
                
                // set readout to rail if saturated
                
                if(resp > m_maxEnergy[br])resp = m_maxEnergy[br];
                
                // convert energy to ADC units here
                
                unsigned short adc = (short unsigned int)((resp/m_maxEnergy[br])*(m_maxAdc-m_pedestal)+m_pedestal);
                
                // assign the plus/minus readouts
                
                if(face == idents::CalXtalId::POS){ adcP=adc; rangeP=br;}
                else { adcM=adc; rangeM=br;}
            }
            /*            
            log << MSG::DEBUG <<" id=" << xtalId 
            << " rangeP=" << int(rangeP) << " adcP=" << adcP
            << " rangeM=" << int(rangeM) << " adcM=" << adcM << endreq;
            */            
            Event::CalDigi::CalXtalReadout read = Event::CalDigi::CalXtalReadout(rangeP, adcP, rangeM, adcM);
            Event::CalDigi* curDigi = new Event::CalDigi(idents::CalXtalId::BESTRANGE, xtalId);
            curDigi->addReadout(read);
            
            typedef std::multimap< idents::CalXtalId, Event::McIntegratingHit* >::const_iterator ItHit;
            std::pair<ItHit,ItHit> itpair = m_idMcInt.equal_range(xtalId);
            
            for (ItHit mcit = itpair.first; mcit!=itpair.second; mcit++)
            {
                Event::Relation<Event::CalDigi,Event::McIntegratingHit> *rel =
                    new Event::Relation<Event::CalDigi,Event::McIntegratingHit>(curDigi,mcit->second);
                digiHit.addRelation(rel);
            }
            
            digiCol->push_back(curDigi);
        }
    }
    
    StatusCode sc = eventSvc()->registerObject(EventModel::Digi::CalDigiCol, digiCol);
    
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
        log << MSG::DEBUG << "no calorimeter hits found" << endreq;
        return sc;
    }
    
    // loop over hits - pick out CAL hits
    
    for (Event::McIntegratingHitVector::const_iterator it = McCalHits->begin(); it != McCalHits->end(); it++) {
        
        
        //   extracting hit parameters - get energy and first moment
        
        idents::VolumeIdentifier volId = ((idents::VolumeIdentifier)(*it)->volumeID());
        
        
        //   extracting parameters from volume Id identifying as in CAL
        
        if ((int)volId[fLATObjects] == m_eLatTowers &&
            (int)volId[fTowerObjects] == m_eTowerCal){ 
            
            double ene = (*it)->totalEnergy();
            HepPoint3D mom1 = (*it)->moment1();
            
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
            int segm = volId[fSegment];
            
            idents::CalXtalId mapId(tower,layer,col);
            
            log << MSG::DEBUG <<  "Identifier decomposition \n"  
                << " col " << col
                << " layer " << layer
                << " towy " << towy
                << " towx " << towx
                << " segm " << segm
                << endreq;
            
            XtalSignal& xtalSignalRef = m_signalMap[mapId];
            
            // Insertion of the id - McIntegratingHit pair
            
            m_idMcInt.insert(std::make_pair(mapId,*it));
            
            if((int)volId[fCellCmp] == m_eDiodeMLarge)
                xtalSignalRef.addDiodeEnergy(ene,0);
            
            else if((int)volId[fCellCmp] == m_eDiodePLarge)
                xtalSignalRef.addDiodeEnergy(ene,1);
            
            else if((int)volId[fCellCmp] == m_eDiodeMSmall )
                xtalSignalRef.addDiodeEnergy(ene,2);
            
            else if((int)volId[fCellCmp] == 	m_eDiodePSmall ) 
                xtalSignalRef.addDiodeEnergy(ene,3);
            
            else if((int)volId[fCellCmp] ==  m_eXtal ){
                
                
                // let's define the position of the segment along the crystal
                
                double relpos = (segm+0.5)/m_nCsISeg;
                
                
                // in local reference system x is always oriented along the crystal
                double dpos =  mom1.x(); 
                
                relpos += dpos/m_CsILength;
                
                
                
                // take into account light tapering
                
                std::pair<double,double> signals;
                signals = m_taper->calculateSignals(mapId,relpos,ene);
                
                // set up a XtalMap to act as key for the map - if there is no entry, add
                // add one, otherwise add signal to existing map element.
                
                
                xtalSignalRef.addSignal(signals.first,signals.second);
                
                
                
            }
        }
    }   
    return sc;
}

StatusCode CalDigiAlg::addNoiseToSignals() {
    // Purpose and Method: 
    // add electronic noise to the diode response and add to the crystal 
    // response. The diodeEnergy becomes the readout source.
    
    for(SignalMap::iterator mit=m_signalMap.begin(); mit!=m_signalMap.end();mit++){
        XtalSignal* xtal_signal = &(*mit).second;
        
        for ( int idiode =0; idiode < 4; idiode++){
            int diode_type = idiode/2;
            int face  = idiode%2;
            double signal = xtal_signal->getSignal(face);
            
            double diode_ene = xtal_signal->getDiodeEnergy(idiode);
            
            
            // convert energy deposition in a diode to
            // the equivalent energy in a crystal 
            diode_ene *= m_ePerMeVinDiode/m_ePerMeV[diode_type];
            
            // add crystal signal - now diode energy contains
            // the signal at given diode in energy units
            // (equivalent energy deposition at the crystal center)
            diode_ene += signal;
            
            // add poissonic fluctuations in the number of electrons in a diode
            if (m_doFluctuationsBool) {
                float numberElectrons = diode_ene * m_ePerMeV[diode_type];
                
                // approximate Poisson distribution by gaussian for numberElectrons >> 1
                float electronFluctuation = sqrt(numberElectrons) * RandGauss::shoot();
                
                diode_ene += electronFluctuation /m_ePerMeV[diode_type];
            }
            
            // add electronic noise
            diode_ene += RandGauss::shoot()*m_noise[diode_type]/m_ePerMeV[diode_type];
            
            //store modified diode signal in the signal map
            xtal_signal->setDiodeEnergy(diode_ene,idiode);
            
        }
    }
    return StatusCode::SUCCESS;
}


StatusCode CalDigiAlg::addNewNoiseHits() {
    // Purpose and Method: 
    //  adding electronic noise to the channels without signal,
    // 	storing it in the signal map if  one of crystal faces
    //	has the noise above the threshold 	
    
    double noise_MeV = double(m_noise[0])/double(m_ePerMeV[0]); // noise in MeV for the large diode
    for (int tower = 0; tower < m_xNum*m_yNum; tower++){
        for (int layer = 0; layer < m_CalNLayer; layer++){
            for (int col = 0; col < m_nCsIPerLayer; col++){
                double eneM = noise_MeV*RandGauss::shoot();
                double eneP = noise_MeV*RandGauss::shoot();
                idents::CalXtalId mapId(tower,layer,col);
                if((eneM > m_thresh || eneP > m_thresh) &&
                    m_signalMap.find(mapId) == m_signalMap.end()){
                    XtalSignal& xtalSignalRef = m_signalMap[mapId];
                    xtalSignalRef.addDiodeEnergy(eneM,0);
                    xtalSignalRef.addDiodeEnergy(eneP,1);
                }
            }
        }
    }
    return StatusCode::SUCCESS;;
}

CalDigiAlg::XtalSignal::XtalSignal() {
    // Purpose and Method: default constructor setting signals to zero
    m_signal[0] = 0;
    m_signal[1] = 0;
    
    for(int i=0; i<4; i++)m_Diodes_Energy.push_back(0.);
    
}

CalDigiAlg::XtalSignal::XtalSignal(double s1, double s2) {
    // Purpose and Method: constructor setting signals to those input
    m_signal[0] = s1;
    m_signal[1] = s2;
    
    for(int i=0; i<4; i++)m_Diodes_Energy.push_back(0.);
    
}
void CalDigiAlg::XtalSignal::addSignal(double s1, double s2) {
    // Purpose and Method: add signals s1, s2 to already existing signals.
    m_signal[0] += s1;
    m_signal[1] += s2;
    return;
} 
