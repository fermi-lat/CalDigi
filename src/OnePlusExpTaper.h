#ifndef _GlastDigi_OnePlusExpTaper_H
#define _GlastDigi_OnePlusExpTaper_H 1


// Include files

#include "ITaper.h"
#include "GaudiKernel/AlgTool.h"

/** @class OnePlusExpTaper
 * @brief onePlusExp taper model implementation for crystal digitization
 *
 * Author:  R.Dubois
 *
*/

class OnePlusExpTaper : public AlgTool, virtual public ITaper {
    
public:
    
    OnePlusExpTaper( const std::string& type, const std::string& name, const IInterface* parent);
    virtual ~OnePlusExpTaper() {};
    virtual std::pair<double, double> calculateSignals(idents::CalXtalId id, 
        double relativePosition, double depositedEnergy);

    ///Implementation of the method provided by the base class AlgTool.
    virtual StatusCode initialize();
    
private:

    /// light attenuation parameter - linear term
    double m_lightAtt;
    /// full length of crystal
    double m_CsILength;
    /// light attenuation parameter - scale factor on the exponential
    double m_scaleExponential;
    /// light attenuation parameter - scale factor on the exponent - effective length scale
    double m_scaleExponent;
    /// input XML file containing parameters for Digitization
    std::string	m_xmlFile;
};


#endif // _GlastDigi_OnePlusExpTaper_H
