// Mainpage for doxygen

/** @mainpage package CalDigi
 *
 * @authors A.Chekhtman and R.Dubois
 *
 * @section description Description
 *
 * This package performs the digitization of the calorimeter (CAL)
 * simulated readout, as laid out in these 
 * <a href="http://www-glast.slac.stanford.edu/software/DataStructuresTF/20011220/CalorimeterDigiRequirements.htm"> requirements</a>.
 * 
 * CalDigiAlg takes perrforms the following steps:
 * - invoke CalSignalTool to sum all McIntegratingHits into Cal crystal diodes, either by CsI scintillation or direct diode deposit.
 * - call TrgConfigSvc to determine readout mode (allrange/bestrange , zeroSupression) for current event digis.
 * - call CalXtalResponse/IXtalDigiTool to generate CalDigis for individual crystals
 * - ignore crystals under LAC threshold if zeroSuppression is requested by trigger configuration.
 * - store McIntegratingHit <> CalDigi relations to file
 * - save CalDigiinformation to TDS
 *
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
 * @param CalDigiAlg.xtalDigiToolName
 *  Select the tool to perform the conversion from energy to ADC units for individual xtals
 *  Available choice is "XtalDigiTool"
 *
 *
 * <hr>
 * @section notes release.notes
 * release.notes
 * <hr>
 * @section requirements requirements
 * @verbinclude requirements
 * <hr>
 * @todo add failure modes
 *
 */
