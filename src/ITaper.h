#ifndef _GlastDigi_ITaper_H
#define _GlastDigi_ITaper_H 1


// Include files
#include "GaudiKernel/IAlgTool.h"
#include "idents/CalXtalId.h"
#include <string>
#include <vector>

/** @class ITaper
 * @brief Base class for CAL Digi functions to calculate light taper
 *
 * Author:  R.Dubois
 *
*/

static const InterfaceID IID_ITaper("ITaper", 1 , 0);


class ITaper : virtual public IAlgTool {
    
public:
    
    ITaper() {}; 
    ~ITaper() {};

    static const InterfaceID& interfaceID() { return IID_ITaper; }

    virtual std::pair<double, double> calculateSignals(idents::CalXtalId id,
        double relativePosition, 
        double depositedEnergy) = 0;
    
        
};


#endif // _GlastDigi_ITaper_H
