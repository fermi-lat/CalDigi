#ifndef CalDigiAlg_H
#define CalDigiAlg_H

// LOCAL INCLUDES

// GLAST INCLUDES
#include "CalUtil/CalDefs.h"
#include "CalUtil/CalConfig.h"
#include "CalXtalResponse/ICalSignalTool.h"
#include "Event/MonteCarlo/McIntegratingHit.h"
#include "Event/Digi/CalDigi.h"
#include "Event/RelTable/Relation.h"

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
 * jobOptions (see mainpage.h)
 *
 *
 * @author:  A. Chehtman 
 * @author:  Z. Fewtrell
 *
 */

class IXtalDigiTool;
class IConfigSvc;
class ICalDiagnosticTool;

class CalDigiAlg : public Algorithm {

public:

  CalDigiAlg(const std::string& name, ISvcLocator* pSvcLocator); 

  StatusCode initialize();
  StatusCode execute();
  StatusCode finalize() {return StatusCode::SUCCESS;}
 
private:
  /// sum MC hit deposits into diode signals
  StatusCode fillSignalEnergies();

  /// generate & register output CalDigi objects
  StatusCode registerDigis();

  /// store relationship between cal digis & mc integrating hits
  typedef Event::RelTable<Event::CalDigi, Event::McIntegratingHit> CalDigiMcRelMap;

  /// generate output CalDigi objects, caldigi<>mc rel table
  /// @param xtalMcRelMap input xtalId <> McIntegrating hit map
  /// @param digiCol output CalDigi collection 
  /// @param digiMcRelMap output CalDigi <> McIntegrating hit map 
  StatusCode genDigis(const idents::CalXtalId::CalTrigMode calTrigMode,
                      const bool zeroSupp,
                      const ICalSignalTool::CalRelationMap &xtalMcRelMap,
                      Event::CalDigiCol &digiCol,
                      CalDigiMcRelMap &digiMcRelMap
                      );

  /// generate & register Cal diagnostic contribution
  StatusCode registerDiagnosticData();

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

  /// name of optional ConfigSvc
  StringProperty m_configSvcName;
  /// used to get readout mode for current event based on trigger bits. (optional)
  IConfigSvc *m_configSvc;

  /// fall back zero suppression mode if ConfigSvc not available (default=true)
  BooleanProperty m_defaultZeroSuppress;

  /// fall back 4range readout mode if ConfigSvc not available (default=false - single range readout)
  BooleanProperty m_defaultAllRange;

  /// store first range option ("autoRng" ---> best range first, "lex8", "lex1", "hex8", "hex1" ---> lex8-hex1 first)
  StringProperty m_firstRng;

  /// enable generation of Cal Diagnostic data in digi
  BooleanProperty m_createDiagnosticData;

  /// name of optional calDiagnosticTool
  StringProperty m_calDiagnosticToolName;

  /// ptr to optional calDiagnosticTool
  ICalDiagnosticTool *m_calDiagnosticTool;
};

#endif
