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
 * <li>for deposit in a crystal segment, take into account light propagation to 
 * the two ends and apply light taper based on position along the length.</li>
 * <li>keep track of direct deposit in the diode.</li>
 * <li> add poissonic fluctuations to the diode</li>
 * <li> combine (with appropriate scale factor) with crystal deposits </li>
 * <li> add noise, convert to ADC units and pick the appropriate readout range 
 * for hits above threshold. This is all done in CalUtil</li>
 *
 * Note that the geometry specification allows the artificial segmenting of
 * the crystals along their length.
 * 
 * <b>Light Taper</b>
 *
 * The plot below shows the principal of the light taper correction. Signal
 * propagates to either end of the crystal with the size approximately
 * linearly split. The current algorithm applies an exactly linear correction.
 * The signal is scaled such that each end makes a measurement of the full
 * particle energy.
 *
 * @image html taper.jpg
 *
 * The crystals are segmented in the geometry (default is 12 segments). The
 * energy contributions from each segment are subjected to the taper correction
 * and then added to form the total signal. For a linear model of attenuation:
 *
 *  Normalization N = 0.5+0.5*lightAtt
 *
 *  (for lightAtt=0, the signal is split evenly in two)
 *
 *  s1 = energy*(1-relPos*(1-lightAtt))/N, where
 *  relPos is the relative position calculated from the first moment (dPos)
 * available from McIntegratingHit.
 *
 *  relPos = (nSegment+0.5)/nCsISeg + dPos/CsILength
 *
 *  s2 = energy*(1-(1-relPos)*(1-lightAtt))/N
 *
 * Note that s1 + s2 = 2*energy and
 *
 * s1 - s2 = energy/N * (1-2*relPos)*(1-lightAtt) so that
 *
 *  (s1-s2)/(s1+s2) = (1-2*relPos)*(1-lightAtt)/2
 *
 *  which is a measure of the relative position.
 *
 *  this light taper is modeled via user selectable functions. These are implemented as
 *  Gaudi tools. See below for the options.
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
 * <b>Noise Effects</b>
 *
 * Two noise contributions are added:
 * 
 * <li> Poisson fluctuation of observed electrons at the diode</li>
 * <li> Gaussian electronic noise. In practice, the threshold is at
 *  about 5 sigma of noise, so new hits attributed to noise are
 * negligible. This last noise is simulated in CalUtil</li>
 *
 * <b> Digitization</b>
 *
 * The true energy deposited in the simulation (then corrected and fluctuated)
 * must be quantized into ADC units. Recall that there are two diodes per
 * crystal end, with two gain ranges each. So, there are 4 possible ranges
 * to choose from per end.
 * 
 * The conversion from deposited energy to ADC is handled by a converter Gaudi Tool
 * in CalUtil. The Tool is selectable from the jobOptions file.
 *
 * See the CAL PDR report for a fuller <a href="https://www-doc.slac.stanford.edu/CyberDOCS/quickstart.asp?show=COPY:505:&user=glastguest:guest&library=SLAC&noframes">description</a> of the PIN diode readout, especially Section 10.3.
 *
 * There is overlap in the energy ranges, and a saturation value per range. 
 * The best range is selected by the largest readout value that is below
 * the saturation value of its range.
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
 * @param CalDigiAlg.convertAdcToolName
 *  Select the tool to perform the conversion from energy to ADC units
 *  Available choice is "LinearConvertAdc
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
