#ifndef CalDigiAlg_H
#define CalDigiAlg_H

// LOCAL INCLUDES

// GLAST INCLUDES
#include "CalUtil/CalDefs.h"
#include "CalXtalResponse/ICalSignalTool.h"
#include "Event/MonteCarlo/McIntegratingHit.h"
#include "Event/Digi/CalDigi.h"


// EXTLIB INCLUDES
#include "GaudiKernel/Algorithm.h"

class IGlastDetSvc;

// STD INCLUDES

/** @class CalDigiAlg
 * @brief Algorithm to convert McIntegratingHit objects into 
 * CalDigi objects and store them in the TDS.   
 * 
 * also stores McIntegratingHit  <-> CalDigi relational table in TDS
 *
 * jobOptions:
 * CalSignalTool (default="CalSignalTool") - used to convert McIntegratingHits to diode signals
 * XtalDigiTool (default="XtalDigiTool") - used to convert diode signals to CalDigi objects
 *
 * @author:  A. Chehtman 
 * @author:  Z. Fewtrell
 *
 */

class IXtalDigiTool;
class ITrgConfigSvc;

class CalDigiAlg : public Algorithm {

public:

  CalDigiAlg(const std::string& name, ISvcLocator* pSvcLocator); 

  StatusCode initialize();
  StatusCode execute();
  StatusCode finalize() {return StatusCode::SUCCESS;}
 
private:
  /// sum MC hit deposits into diode signals
  StatusCode fillSignalEnergies();

  /// gerenate output CalDigi objects
  StatusCode registerDigis();

  /// check TDS & create DigiEvent if needed
  StatusCode ensureDigiEventExists();

  /// get geometry constants from detector service
  StatusCode retrieveConstants();

  /// determine readout mode for this event based on trigger bits.
  StatusCode CalDigiAlg::getTrgConditions(idents::CalXtalId::CalTrigMode &calTrigMode,
                                          bool &zeroSupp);

  /// list of active tower bays, populated at run time
  std::vector<CalUtil::TwrNum> m_twrList;

  //-- CONSTANTS --//
  /// volume ID enumeration
  int m_eLATTowers;

  /// volume ID enumeration
  int m_eTowerCAL;  

  /// volume ID enum
  int m_eMeasureX;

  /// volume ID enum
  int m_eXtal;

  /// name of Tool for calculating single xtal digi response from diode signals
  StringProperty m_xtalDigiToolName;
  /// pointer to xtal digi tool
  IXtalDigiTool* m_xtalDigiTool;

  /// name of CalSignalTool tool, used for calculating diode signal levels from McIntegratingHits
  StringProperty m_calSignalToolName;

  /// ptr to CalSignalTool tool
  ICalSignalTool  *m_calSignalTool;

  /// used for constants & conversion routines.
  IGlastDetSvc* m_detSvc;

  /// name of optional TrgConfigSvc
  StringProperty m_trgConfigSvcName;
  /// used to get readout mode for current event based on trigger bits. (optional)
  ITrgConfigSvc *m_trgConfigSvc;

  /// fall back zero suppression mode if TrgConfigSvc not available (default=true)
  BooleanProperty m_defaultZeroSuppress;

  /// fall back 4range readout mode if TrgConfigSvc not available (default=false - single range readout)
  BooleanProperty m_defaultAllRange;
};

#endif

