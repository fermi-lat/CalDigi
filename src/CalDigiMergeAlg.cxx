/**
 * @file CalDigiMergeAlg.cxx
 * @brief implementation  of the algorithm CalDigiMergeAlg.
 *
 * @author Zach Fewtrell zachary.fewtrell@nrl.navy.mil
 * 
 *  $Header: /nfs/slac/g/glast/ground/cvs/CalDigi/src/CalDigiMergeAlg.cxx,v 0.00 2008/06/17 18:49:24 echarles Exp $
 */

// Gaudi specific include files
#include "GaudiKernel/Algorithm.h"
#include "GaudiKernel/AlgFactory.h"
#include "GaudiKernel/MsgStream.h"
#include "GaudiKernel/IDataProviderSvc.h"
#include "GaudiKernel/SmartDataPtr.h"

// Glast specific includes
#include "Event/TopLevel/EventModel.h"
#include "Event/TopLevel/Event.h"
#include "Event/Digi/CalDigi.h"
#include "CalUtil/CalDefs.h"
#include "GlastSvc/GlastDetSvc/IGlastDetSvc.h"
#include "CalUtil/CalGeom.h"                     

#include "CalXtalResponse/ICalCalibSvc.h"

#include <map>

// Class definition
class CalDigiMergeAlg : public Algorithm {

public:

    CalDigiMergeAlg(const std::string& name, ISvcLocator* pSvcLocator); 

    StatusCode initialize();
    StatusCode execute();
    StatusCode finalize() {return StatusCode::SUCCESS;}
 
private:

    /// used for constants & conversion routines.
    IGlastDetSvc*  m_detSvc;
  
    /// pointer to CalCalibSvc objects.
    ICalCalibSvc*  m_overCalibSvc;  
    ICalCalibSvc*  m_digiCalibSvc;  

    /// store first range option ("autoRng" ---> best range first, "lex8", "lex1", "hex8", "hex1" ---> lex8-hex1 first)
    StringProperty m_firstRng;
};


// Define the factory for this algorithm
static const AlgFactory<CalDigiMergeAlg>  Factory;
const IAlgFactory& CalDigiMergeAlgFactory = Factory;

/// construct object & declare jobOptions
CalDigiMergeAlg::CalDigiMergeAlg(const string& name, ISvcLocator* pSvcLocator) :
  Algorithm(name, pSvcLocator)
{

    // Declare the properties that may be set in the job options file
    declareProperty("FirstRangeReadout",   m_firstRng= "autoRng"); 
}

/// initialize the algorithm. retrieve helper tools & services
StatusCode CalDigiMergeAlg::initialize() 
{
    StatusCode sc = StatusCode::SUCCESS;
  
    MsgStream msglog(msgSvc(), name());
    msglog << MSG::INFO << "initialize" << endreq;

    sc = setProperties();
    if (sc.isFailure()) {
        msglog << MSG::ERROR << "Could not set jobOptions properties" << endreq;
        return sc;
    }
    
    // try to find the GlastDetSvc service
    sc = service("GlastDetSvc", m_detSvc);
    if (sc.isFailure() ) {
        msglog << MSG::ERROR << "  can't get GlastDetSvc " << endreq;
        return sc;
    }
  
    // obtain the CalCalibSvc for the input overlay digis (data!)
    sc = service("CalOverlayCalibSvc", m_overCalibSvc);
    if (sc.isFailure()) 
    {
        msglog << MSG::ERROR << "can't get CalOverlayCalibSvc "  << endreq;
        return sc;
    }
  
    // now obtain the CalCalibSvc for the simulation digis
    sc = service("CalCalibSvc", m_digiCalibSvc);
    if (sc.isFailure()) 
    {
        msglog << MSG::ERROR << "can't get CalCalibSvc "  << endreq;
        return sc;
    }

    return sc;
}

/// \brief take Hits from McIntegratingHits, create & register CalDigis
StatusCode CalDigiMergeAlg::execute() 
{
    // Get a message object for output
    MsgStream msglog(msgSvc(), name());

    // First, recover any overlay digis, to see if we have anything to do
    SmartDataPtr<Event::CalDigiCol> overlayDigiCol(eventSvc(), EventModel::Overlay::CalDigiCol);
    if(!overlayDigiCol) return StatusCode::SUCCESS;

    // Now recover the digis for this event
    SmartDataPtr<Event::CalDigiCol> calDigiCol(eventSvc(), EventModel::Digi::CalDigiCol);

    // Create a map of the simulation digis, indexing by identifier
    std::map<idents::CalXtalId, Event::CalDigi*> idToDigiMap;

    for(Event::CalDigiCol::iterator calIter = calDigiCol->begin(); calIter != calDigiCol->end(); calIter++)
    {
        Event::CalDigi*         calDigi = *calIter;
        const idents::CalXtalId digiId  = calDigi->getPackedId();

        // Add this digi to our map
        idToDigiMap[digiId] = calDigi;
    }

    // The calibration will need the Event Header information for both the sim and the overlay
    SmartDataPtr<Event::EventHeader> digiHeader(eventSvc(), "/Event");
  
    if (!digiHeader) {
      msglog << MSG::ERROR << "Unable to retrieve event timestamp for digis" << endreq;
      return StatusCode::FAILURE;
    }

    // The calibration will need the Event Header information for both the sim and the overlay
    SmartDataPtr<Event::EventHeader> overHeader(eventSvc(), EventModel::Overlay::EventHeader);
  
    if (!overHeader) {
      msglog << MSG::ERROR << "Unable to retrieve event timestamp for overlay" << endreq;
      return StatusCode::FAILURE;
    }

    // Loop through the input digis and using the above map merge with existing MC digis
    for(Event::CalDigiCol::iterator overIter  = overlayDigiCol->begin(); overIter != overlayDigiCol->end(); overIter++)
    {
        Event::CalDigi*         overDigi = *overIter;
        const idents::CalXtalId overId   = overDigi->getPackedId();

        // Reference to our overlay digi CalXtalReadout vector
        const Event::CalDigi::CalXtalReadoutCol& overXtalCol = overDigi->getReadoutCol();
        const Event::CalDigi::CalXtalReadout&    overXtal    = overXtalCol.front();

        unsigned short overStatus = overXtal.getStatus();

        // Set up positive and negative "FaceNum"'s for pedestal extraction
        CalUtil::FaceNum posFaceNum(idents::CalXtalId::POS);
        CalUtil::FaceNum negFaceNum(idents::CalXtalId::NEG);

        // We will need to correct the pedestals for the overlay digi, to match the simulation
        // The first step requires overwriting the time stamp information in the EventHeader, 
        // start by storing the current time locally
        TimeStamp currentTime(digiHeader->time());

        // Now overwrite with the time stamp from the overlay file
        digiHeader->setTime(overHeader->time());

        // Set up to extract the overlay pedestals
        CalUtil::XtalIdx overXtalIdx(overDigi->getPackedId());
        CalUtil::RngIdx  overPosRngIdx(overXtalIdx, posFaceNum, overXtal.getRange(idents::CalXtalId::POS));
        CalUtil::RngIdx  overNegRngIdx(overXtalIdx, negFaceNum, overXtal.getRange(idents::CalXtalId::NEG));

        // Pedestals are extracted as floats, for reasons I'm not sure I understand
        float overPosPed = 0.;
        float overNegPed = 0.;

        // Go get them! 
        m_overCalibSvc->getPed(overPosRngIdx, overPosPed);
        m_overCalibSvc->getPed(overNegRngIdx, overNegPed);

        // Return the time stamp information to normal
        digiHeader->setTime(currentTime);

        // Get the pedestal subtracted ADC values for the overlay digi
        unsigned short overAdcP = overXtal.getAdc(idents::CalXtalId::POS);
        unsigned short overAdcN = overXtal.getAdc(idents::CalXtalId::NEG);

        float tempAdcP = overAdcP - overPosPed > 0. ? overAdcP - overPosPed : 0.;
        float tempAdcN = overAdcN - overNegPed > 0. ? overAdcN - overNegPed : 0.;

        overAdcP = tempAdcP;
        overAdcN = tempAdcN;

        // Does this overlay digi match one in the sim map?
        std::map<idents::CalXtalId,Event::CalDigi*>::iterator calIter = idToDigiMap.find(overId);

        // If this digi is not already in the CalDigiCol then no merging needed, just add it
        if (calIter == idToDigiMap.end())
        {
            Event::CalDigi* digi = new Event::CalDigi();
            *digi = *overDigi;
            digi->setParent(0);
            digi->addToStatus(Event::CalDigi::DIGI_OVERLAY);

            const Event::CalDigi::CalXtalReadout& digiXtal = digi->getReadoutCol().front();

            // Here we need to get the sim version of the pedestals for this overlay digi
            CalUtil::RngIdx  overPosRngIdx(overXtalIdx, posFaceNum, digiXtal.getRange(idents::CalXtalId::POS));
            CalUtil::RngIdx  overNegRngIdx(overXtalIdx, negFaceNum, digiXtal.getRange(idents::CalXtalId::NEG));

            float simPosPed = 0.;
            float simNegPed = 0.;
        
            m_digiCalibSvc->getPed(overPosRngIdx, simPosPed);
            m_digiCalibSvc->getPed(overNegRngIdx, simNegPed);

            // Add these to the overlay digi ADC's
            overAdcP += simPosPed;
            overAdcN += simNegPed;

            if (overAdcP > 4095) overAdcP = 4095;
            if (overAdcN > 4095) overAdcN = 4095;

            // Reset the now ped subtracted ADC counts
            const_cast<Event::CalDigi::CalXtalReadout&>(digiXtal).initialize(digiXtal.getRange(idents::CalXtalId::POS), 
                                                                             overAdcP, 
                                                                             digiXtal.getRange(idents::CalXtalId::NEG),
                                                                             overAdcN, 
                                                                             overStatus);

            calDigiCol->push_back(digi);
        }
        // Otherwise, we need to merge the digi
        else
        {
            // Recover pointer to the CalDigi from the simulation
            Event::CalDigi* simDigi = calIter->second;

            // Reference to its CalXtalReadout vector
            const Event::CalDigi::CalXtalReadoutCol& digiXtalCol = simDigi->getReadoutCol();
            const Event::CalDigi::CalXtalReadout&    digiXtal    = digiXtalCol.front();

            unsigned short digiStatus = digiXtal.getStatus();

            // Here we need to get the sim version of the pedestals for this sim digi
            CalUtil::XtalIdx simXtalIdx(simDigi->getPackedId());
            CalUtil::RngIdx  simPosRngIdx(simXtalIdx, posFaceNum, digiXtal.getRange(idents::CalXtalId::POS));
            CalUtil::RngIdx  simNegRngIdx(simXtalIdx, negFaceNum, digiXtal.getRange(idents::CalXtalId::NEG));

            float simPosPed = 0.;
            float simNegPed = 0.;
        
            m_digiCalibSvc->getPed(simPosRngIdx, simPosPed);
            m_digiCalibSvc->getPed(simNegRngIdx, simNegPed);

            // Recover the ADC/Range info
            unsigned short adcP       = digiXtal.getAdc(idents::CalXtalId::POS);
            char           rangeP     = digiXtal.getRange(idents::CalXtalId::POS);
            unsigned short adcM       = digiXtal.getAdc(idents::CalXtalId::NEG);
            char           rangeM     = digiXtal.getRange(idents::CalXtalId::NEG);

            // Check for dead channel consistency (can this happen?)
            if ((digiStatus & Event::CalDigi::CalXtalReadout::DEAD_P) != (overStatus & Event::CalDigi::CalXtalReadout::DEAD_P))
            {
                int j = 0;
            }

            // Channel is not dead
            if (!(digiStatus & Event::CalDigi::CalXtalReadout::DEAD_P))
            {
                // Check range
                if (rangeP == overXtal.getRange(idents::CalXtalId::POS))
                       adcP += overXtal.getAdc(idents::CalXtalId::POS);
                else if (rangeP < overXtal.getRange(idents::CalXtalId::POS))
                {
                    adcP   = overXtal.getAdc(idents::CalXtalId::POS);
                    rangeP = overXtal.getRange(idents::CalXtalId::POS);
                }

                if (adcP > 4095) adcP = 4095;
            }

            // Check for dead channel consistency (can this happen?)
            if ((digiStatus & Event::CalDigi::CalXtalReadout::DEAD_N) != (overStatus & Event::CalDigi::CalXtalReadout::DEAD_N))
            {
                int j = 0;
            }

            // Channel is not dead
            if (!(digiStatus & Event::CalDigi::CalXtalReadout::DEAD_N))
            {
                // Check range
                if (rangeM == overXtal.getRange(idents::CalXtalId::NEG))
                       adcM += overXtal.getAdc(idents::CalXtalId::NEG);
                else if (rangeM < overXtal.getRange(idents::CalXtalId::NEG))
                {
                    adcM   = overXtal.getAdc(idents::CalXtalId::NEG);
                    rangeM = overXtal.getRange(idents::CalXtalId::NEG);
                }

                if (adcM > 4095) adcM = 4095;
            }

            // Re-initialize the info
            const_cast<Event::CalDigi::CalXtalReadout&>(digiXtal).initialize(rangeP, adcP, rangeM, adcM, digiStatus);
            simDigi->addToStatus(Event::CalDigi::DIGI_OVERLAY);
        }
    }

    return StatusCode::SUCCESS;
}
