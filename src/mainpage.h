// Mainpage for doxygen

/** @mainpage package CalDigi
 *
 * @authors A.Chekhtman and R.Dubois
 *
 * @section description Description
 *
 * This package performs the digitization of the calorimeter (CAL)
 * simulated readout, as laid out in these <a href="http://www-glast.slac.stanford.edu/software/DataStructuresTF/20011220/CalorimeterDigiRequirements.htm"> requirements</a>.
 * 
 * CalDigiAlg takes Hits from McIntegratingHit and performs the following steps:
 * <b>Thresholds and Energy Deposit</b>
 *
 * This plot shows the effect of the threshold cut (applied here at 2 MeV,
 * translating to 100 ADC counts). Signals from each end are entered into 
 * the plot from vertical muons at x=y=40 mm, so one sees two peaks
 * (at around 300 and 400 ADC counts) representing the light taper to the two ends.
 *
 * @image html muonEnergy.jpg
 * 
 * @section Tests Tests
 * A test program, under src/test, exercises everything. It is set up as a Gaudi algorithm
 * and runs CalDigiAlg with an MC root file as input for on-axis muons. The algorithm checks
 * that there are output digis in the TDS. The input file is 2 GeV vertical muons 
 * at x=y=40 mm, z=800mm
 *
 * @section jobOptions jobOptions
 *
 * @param CalDigiAlg.RangeType
 *  Select the readout range
 *  Available choices are "BEST" and "ALL"
 * @param CalDigiAlg.xtalADCToolName
 *  Select the tool to perform the conversion from energy to ADC units
 *  Available choice is "XtalADCTool"
 * @param CalDigiAlg.doFluctuations
 *  Flag to enable electron counting statistics fluctuations
 *
 *
 * <hr>
 * @section notes release.notes
 * release.notes
 * <hr>
 * @section requirements requirements
 * @verbinclude requirements
* <hr>
 * @todo add front-end non-linearity
 * @todo add failure modes
 * @todo add realistic light taper
 * @todo add calibration objects for light taper and front-end non-linearity
 *
 */
