#ifndef _GlastDigi_CalDigiAlg_H
#define _GlastDigi_CalDigiAlg_H 1

// Include files
#include "ITaper.h"
#include "CalUtil/ICalFailureModeSvc.h"
#include "CalUtil/IConvertAdc.h"
#include "GaudiKernel/Algorithm.h"
#include "GlastSvc/GlastDetSvc/IGlastDetSvc.h"
#include "GaudiKernel/IDataProviderSvc.h"
#include "GaudiKernel/Service.h"
#include "CalibData/CalibModel.h"
#include "CalibData/Cal/CalCalibPed.h"
#include "CalibData/Cal/CalCalibGain.h"
#include <vector>
#include <map>
// MC classes
#include "Event/MonteCarlo/McIntegratingHit.h"

/** @class CalDigiAlg
 * @brief Algorithm to convert from McIntegratingHit objects into 
 * CalDigi objects and store them in the TDS. Combines contributions from
 * Xtal segments and accounts for light taper along the length of the Xtals.
 * Energies are converted to adc values after pedestal subtraction, and the 
 * appropriate gain range is identified.
 *
 * Author:  A.Chekhtman
 *
*/

// Forward declarations
class IDetDataSvc;

class CalDigiAlg : public Algorithm {
    
public:
    
    CalDigiAlg(const std::string& name, ISvcLocator* pSvcLocator); 
    
    StatusCode initialize();
    StatusCode execute();
    StatusCode finalize();
    
    //! pair of signals per Xtal. For SignalMap.
/** @class XtalSignal
 * @brief nested class of CalDigiAlg to separately hold the energy deposits in the crystal
 * and the diodes.Vector of diodes holds all 4 per crystal.
 *
 * Author:  A.Chekhtman
 *
*/
    class XtalSignal {
    public:
        XtalSignal();
        /// constructor given signal from both ends of xtal
        XtalSignal(double s1, double s2);
        ~XtalSignal() {};
        ///  return signal from selected diode by specifying the face
        double getSignal(int face) {return m_signal[face];};
        ///  add to existing diode signals
        void addSignal(double s1, double s2);
        /// fetch diode energy, given the diode number
        double getDiodeEnergy(int diode) const { return m_Diodes_Energy[diode];}
        /// add energy to the selected (already existing) diode
        void addDiodeEnergy(double ene, int diode) { m_Diodes_Energy[diode]+=ene;}
        /// set the (initial) energy for a diode
        void setDiodeEnergy(double ene, int diode) { m_Diodes_Energy[diode]=ene;}
            
    private:
        /// signal for both xtal faces (POS, NEG)
        double m_signal[2];  
        /// direct energy depositions in 4 diodes of one xtal; vector contains all 4 diodes
        std::vector<double> m_Diodes_Energy; 
    };
    
    protected:
        StatusCode fillSignalEnergies();
        StatusCode addNoiseToSignals();
        //StatusCode addNewNoiseHits();
        StatusCode createDigis();
    
    private:
        
        /// names for identifier fields
        enum {fLATObjects, fTowerY, fTowerX, fTowerObjects, fLayer,
            fMeasure, fCALXtal,fCellCmp, fSegment};
        
        
        /// local cache for constants defined in xml files
        /// x tower number
        int m_xNum;  
        /// y tower number
        int m_yNum;  
        /// total number of towers
        int m_nTowers;  
        /// detModel identifier for CAL
        int m_eTowerCal;  
        /// detModel identifier for LAT Towers
        int m_eLatTowers;
        /// number of layers (ie in z)
        int m_CalNLayer;  
        // number of Xtals per layer
        int m_nCsIPerLayer;  
        int m_eXtal;
        /// number of geometric segments per Xtal
        int m_nCsISeg;  
        /// detModel identifier for small minus-side diode
        int m_eDiodeMSmall;   
        /// detModel identifier for small plus-side diode
        int m_eDiodePSmall;
        /// detModel identifier for large minus-side diode
        int m_eDiodeMLarge;
        /// detModel identifier for large plus-side diode
        int m_eDiodePLarge;
        /// detModel identifier for xtal measuring 'x'
        int m_eMeasureX;
        /// detModel identifier for xtal measuring 'y'
        int m_eMeasureY;
        /// gain - electrons/MeV 1=Small, 0=Large
        int m_ePerMeV[2];  
        /// noise for diodes 1=Small, 0=Large units=electrons
        int m_noise[2];  
        /// single pedestal
        int m_pedestal;  
        /// max value for ADC
        int m_maxAdc;  
        /// zero suppression threshold
        double m_thresh;  
        /// highest valid energy for each energy range
        double m_maxEnergy[4];  
        /// light attenuation factor
        double m_lightAtt;  
        /// Xtal length
        double m_CsILength;  
        /// input XML file containing parameters for Digitization
        std::string  m_xmlFile;
        /// electrons per MeV for diodes
        double  m_ePerMeVinDiode;
        /// name of Tool for calculating light taper
        std::string m_taperToolName;
        /// pointer to actual tool for calculating light taper
        ITaper* m_taper;
        /// string flag for applying electron statistics fluctuations per channel
        std::string m_doFluctuations;
        /// string flag for applying electron statistics fluctuations per channel
        bool m_doFluctuationsBool;
        /// map to contain true energy deposits by XtaIdID
            
        typedef std::map<idents::CalXtalId,XtalSignal> SignalMap;
        SignalMap m_signalMap;
       /// map to contain the McIntegratingHit vs XtaliD relational table
        std::multimap< idents::CalXtalId, Event::McIntegratingHit* > m_idMcInt;   

       /// pointer to failure mode service
        ICalFailureModeSvc* m_FailSvc;

        /// name of Tool for calculating light taper
        std::string m_convertAdcToolName;
        /// pointer to actual tool for converting energy to ADC
        IConvertAdc* m_convertAdc;
        
        // Calibration Services
         IDataProviderSvc* m_pCalibDataSvc;
  
        /// Handle to the IDetDataSvc interface of the CalibDataSvc
        IDetDataSvc* m_detDataSvc;

        /// Absolute time of first event (yyyy-mm-dd_hh:mm, trailing fields
        /// optional)
        std::string m_startTimeAsc;

        /// Absolute time of first event (seconds)
        long m_startTime;
    
        /// "flavor" of calibration files
        std::string m_calibFlavor;

        CalibData::CalCalibPed* pPeds;
        CalibData::CalCalibGain* pGains;
};



#endif // _GlastDigi_CalDigiAlg_H

