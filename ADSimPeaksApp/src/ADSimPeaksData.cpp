/**
 * \brief Container class for data associated with the peaks produced
 *        by the ADSimPeaks areaDetector driver.
 *
 * This class is simply a container class for the peak profile 
 * information which include, for both X and Y:
 * 
 *   position
 *   full width half max
 *   correlation 
 *   extra parameters needed for some functions
 *   bin to use for the probability calculation
 *
 * This class can be used for both 1D and 2D applications. In the 
 * case of 1D then only the 'X' functions are needed. The main
 * use of an instance of this class is to simplfy the calling
 * argument list for the probability distribution functions
 * used by ADSimPeaks.
 * 
 * \author Matt Pearson 
 * \date August 31st, 2022
 *
 */

#include <ADSimPeaksData.h>

/**
 * Constructor. This initializes all the 
 * data members to zero. 
 */ 
ADSimPeaksData::ADSimPeaksData(void) {
  clear();
}

/**
 * Destructor
 */
ADSimPeaksData::~ADSimPeaksData(void) {
}

/*******************************************************/
/* Get Functions */

/**
 * Get the peak X position 
 */
epicsFloat64 ADSimPeaksData::getPositionX(void) const {
  return m_position_x;
}

/**
 * Get the peak Y position 
 */
epicsFloat64 ADSimPeaksData::getPositionY(void) const {
  return m_position_y;
}

/**
 * Get the peak X full width half max 
 */
epicsFloat64 ADSimPeaksData::getFWHMX(void) const {
  return m_fwhm_x;
}

/**
 * Get the peak Y full width half max 
 */
epicsFloat64 ADSimPeaksData::getFWHMY(void) const {
  return m_fwhm_y;
}

/**
 * Get the peak amplitude 
 */
epicsFloat64 ADSimPeaksData::getAmplitude(void) const {
  return m_amplitude;
}

/**
 * Get the peak X/Y correlation  
 */
epicsFloat64 ADSimPeaksData::getCorrelation(void) const {
  return m_correlation;
}

/**
 * Get the param1 value  
 */
epicsFloat64 ADSimPeaksData::getParam1(void) const {
  return m_param1;
}

/**
 * Get the param2 value  
 */
epicsFloat64 ADSimPeaksData::getParam2(void) const {
  return m_param2;
}

/**
 * Get the array index in the X direction  
 */
epicsInt32 ADSimPeaksData::getBinX(void) const {
  return m_bin_x;
}

/**
 * Get the array index in the Y direction  
 */
epicsInt32 ADSimPeaksData::getBinY(void) const {
  return m_bin_y;
}

/*******************************************************/
/* Set Functions */

/**
 * Clear all the parameters (set them to zero)
 */
void ADSimPeaksData::clear(void) {
  m_position_x = 0.0;
  m_position_y = 0.0;
  m_fwhm_x = 0.0;
  m_fwhm_y = 0.0;
  m_amplitude = 0.0;
  m_correlation = 0.0;
  m_param1 = 0.0;
  m_param2 = 0.0;
  m_bin_x = 0;
  m_bin_y = 0;
}

/**
 * Set the peak X position 
 */
void ADSimPeaksData::setPositionX(epicsFloat64 value) {
  m_position_x = value;
}

/**
 * Set the peak Y position 
 */
void ADSimPeaksData::setPositionY(epicsFloat64 value) {
  m_position_y = value;
}

/**
 * Set the peak X full width half max 
 */
void ADSimPeaksData::setFWHMX(epicsFloat64 value) {
  m_fwhm_x = value;
}

/**
 * Set the peak Y full width half max 
 */
void ADSimPeaksData::setFWHMY(epicsFloat64 value) {
  m_fwhm_y = value;
}

/**
 * Set the peak amplitude 
 */
void ADSimPeaksData::setAmplitude(epicsFloat64 value) {
  m_amplitude = value;
}

/**
 * Set the peak X/Y correlation  
 */
void ADSimPeaksData::setCorrelation(epicsFloat64 value) {
  m_correlation = value;
}

/**
 * Set the param1 value  
 */
void ADSimPeaksData::setParam1(epicsFloat64 value) {
  m_param1 = value;
}

/**
 * Set the param2 value  
 */
void ADSimPeaksData::setParam2(epicsFloat64 value) {
  m_param2 = value;
}

/**
 * Set the array index in the X direction  
 */
void ADSimPeaksData::setBinX(epicsInt32 value) {
  m_bin_x = value;
}

/**
 * Set the array index in the X direction  
 */
void ADSimPeaksData::setBinY(epicsInt32 value) {
  m_bin_y = value;
}



