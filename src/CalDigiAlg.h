#ifndef _GlastDigi_CalDigiAlg_H
#define _GlastDigi_CalDigiAlg_H 1


// Include files
#include "GaudiKernel/Algorithm.h"
#include "GlastSvc/GlastDetSvc/IGlastDetSvc.h"
#include <vector>

/*! \class CalDigiAlg
\brief Algorithm to convert from McIntegratingHit objects into 
CalDigi objects and store them in the TDS. Combines contributions from
Xtal segments and accounts for light taper along the length of the Xtals.
Energies are converted to adc values after pedestal subtraction, and the 
appropriate gain range is identified.

  Author:  A.Chekhtman
  
*/

class CalDigiAlg : public Algorithm {
    
public:
    
    //! Constructor of this form must be provided
    CalDigiAlg(const std::string& name, ISvcLocator* pSvcLocator); 
    
    //! mandatory
    StatusCode initialize();
    //! mandatory
    StatusCode execute();
    //! mandatory
    StatusCode finalize();
    
    //! pair of signals per Xtal. For SignalMap.
    class XtalSignal {
    public:
        XtalSignal();
        XtalSignal(double s1, double s2);
        ~XtalSignal() {};
        //!  return signal from selected diode
        double getSignal(int face) {return m_signal[face];};
        //!  add to existing diode signals
        void addSignal(double s1, double s2);
		double getDiodeEnergy(int diode) const { return m_Diodes_Energy[diode];}
		void addDiodeEnergy(double ene, int diode) { m_Diodes_Energy[diode]+=ene;}
		void setDiodeEnergy(double ene, int diode) { m_Diodes_Energy[diode]=ene;}
    
	private:
        double m_signal[2];  // signal for both xtal faces (POS, NEG)
		std::vector<double> m_Diodes_Energy; // direct energy depositions in 4 diodes of one xtal
    };
    
    
    private:
        
        enum {fLATObjects, fTowerY, fTowerX, fTowerObjects, fLayer,
            fMeasure, fCALXtal,fCellCmp, fSegment};
        
        
        /// constants defined in xml files
        int m_xNum;  // x tower number
        int m_yNum;  // y tower number
        int m_nTowers;  // total number of towers
        int m_eTowerCAL;  
        int m_eLATTowers;
        int m_CALnLayer;  // number of CAL layers
        int m_nCsIPerLayer;  // number of Xtals per layer
        int m_eXtal;
        int m_nCsISeg;  // number of geometric segments per Xtal
        int m_eDiodeMSmall;   
        int m_eDiodePSmall;
        int m_eDiodeMLarge;
        int m_eDiodePLarge;
        int m_eMeasureX;
        int m_eMeasureY;
        int m_ePerMeV[2];  // gain - electrons/MeV 1=Small, 0=Large
        int m_noise[2];  // noise for diodes 1=Small, 0=Large units=electrons
        int m_pedestal;  // single pedestal
        int m_maxAdc;  // max value for ADC
        double m_thresh;  // zero suppression threshold
        double m_maxEnergy[4];  // highest energy for each energy range
        double m_lightAtt;  // light attenuation factor
        double m_CsILength;  // Xtal length
        
};


#endif // _GlastDigi_CalDigiAlg_H
