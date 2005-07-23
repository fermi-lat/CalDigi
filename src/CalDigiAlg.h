#ifndef _GlastDigi_CalDigiAlg_H
#define _GlastDigi_CalDigiAlg_H 1
// LOCAL INCLUDES

// GLAST INCLUDES
#include "CalUtil/CalDefs.h"
#include "CalXtalResponse/IXtalDigiTool.h"
#include "Event/MonteCarlo/McIntegratingHit.h"

// EXTLIB INCLUDES
#include "GaudiKernel/Algorithm.h"
#include "GlastSvc/GlastDetSvc/IGlastDetSvc.h"
#include "TTree.h"

// STD INCLUDES
#include <vector>
#include <map>

/** @class CalDigiAlg
 * @brief Algorithm to convert from McIntegratingHit objects into 
 * CalDigi objects and store them in the TDS. Groups hits by xtal & calls
 * CalXtalResponse/XtalDigiTool for each xtal.  Also calcuates CALLO & CALHI triggers
 * & writes them to GltDigi class in TDS.
 *
 * Author:  A.Chekhtman
 *
 */

using namespace std;
using namespace CalDefs;

class CalDigiAlg : public Algorithm {

 public:

  CalDigiAlg(const string& name, ISvcLocator* pSvcLocator); 

  StatusCode initialize();
  StatusCode execute();
  StatusCode finalize();
 
 protected:
  StatusCode fillSignalEnergies();
  StatusCode createDigis();

 private:

  /// list of active tower bays, populated at run time
  vector<TwrNum> m_twrList;

  //-- CONSTANTS --//
  /// x tower count
  int m_xNum;  
  /// y tower count
  int m_yNum;  
    
  /// volume ID enumeration
  int m_eTowerCAL;  
  /// volume ID enumeration
  int m_eLATTowers;
  /// volume ID enumeration
  int m_eMeasureX;
  /// volume ID enumeration
  int m_eXtal;
  
  /// number of layers (ie in z)
  int m_CalNLayer;  
  // number of Xtals per layer
  int m_nCsIPerLayer;  

  /// map to contain the McIntegratingHit vs XtaliD relational table
  typedef map< idents::CalXtalId,  vector< const Event::McIntegratingHit*> > PreDigiMap;

  /// map to contain the McIntegratingHit vs XtaliD relational table
  PreDigiMap m_idMcIntPreDigi;   

  /// map to contain the McIntegratingHit vs XtaliD relational table
  multimap< idents::CalXtalId, Event::McIntegratingHit* > m_idMcInt;   

  /// type of readout range: BEST or ALL
  StringProperty m_rangeTypeStr;
  CalXtalId::CalTrigMode m_rangeMode;

  /// name of Tool for calculating light taper
  StringProperty m_xtalDigiToolName;
  /// pointer to actual tool for converting energy to ADC
  IXtalDigiTool* m_xtalDigiTool;
};

#endif // _GlastDigi_CalDigiAlg_H

