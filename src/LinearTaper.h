#ifndef _GlastDigi_LinearTaper_H
#define _GlastDigi_LinearTaper_H 1


// Include files

#include "ITaper.h"
#include "GaudiKernel/AlgTool.h"

/** @class LinearTaper
 * @brief Linear taper model implementation for crystal digitization
 *
 * Author:  R.Dubois
 *
*/

class LinearTaper : public AlgTool, virtual public ITaper {
    
public:
    
    LinearTaper( const std::string& type, const std::string& name, const IInterface* parent);
    virtual ~LinearTaper() {};
    virtual std::pair<double, double> calculateSignals(idents::CalXtalId id, 
        double relativePosition, 
        double depositedEnergy);

    ///Implementation of the method provided by the base class AlgTool.
    virtual StatusCode initialize();
    
private:

    /// light attenuation parameter for decay of light along length of crystal
    double m_lightAtt;
};


#endif // _GlastDigi_LinearTaper_H
