
#include <ADSimPeaksData.h>


ADSimPeaksData::ADSimPeaksData(void) {
  clear();
}

ADSimPeaksData::~ADSimPeaksData(void) {
}

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

epicsFloat64 ADSimPeaksData::getPositionX(void) const {
  return m_position_x;
}

epicsFloat64 ADSimPeaksData::getPositionY(void) const {
  return m_position_y;
}

epicsFloat64 ADSimPeaksData::getFWHMX(void) const {
  return m_fwhm_x;
}

epicsFloat64 ADSimPeaksData::getFWHMY(void) const {
  return m_fwhm_y;
}

epicsFloat64 ADSimPeaksData::getAmplitude(void) const {
  return m_amplitude;
}

epicsFloat64 ADSimPeaksData::getCorrelation(void) const {
  return m_correlation;
}

epicsFloat64 ADSimPeaksData::getParam1(void) const {
  return m_param1;
}

epicsFloat64 ADSimPeaksData::getParam2(void) const {
  return m_param2;
}

epicsInt32 ADSimPeaksData::getBinX(void) const {
  return m_bin_x;
}

epicsInt32 ADSimPeaksData::getBinY(void) const {
  return m_bin_y;
}

void ADSimPeaksData::setPositionX(epicsFloat64 value) {
  m_position_x = value;
}

void ADSimPeaksData::setPositionY(epicsFloat64 value) {
  m_position_y = value;
}

void ADSimPeaksData::setFWHMX(epicsFloat64 value) {
  m_fwhm_x = value;
}

void ADSimPeaksData::setFWHMY(epicsFloat64 value) {
  m_fwhm_y = value;
}

void ADSimPeaksData::setAmplitude(epicsFloat64 value) {
  m_amplitude = value;
}

void ADSimPeaksData::setCorrelation(epicsFloat64 value) {
  m_correlation = value;
}

void ADSimPeaksData::setParam1(epicsFloat64 value) {
  m_param1 = value;
}

void ADSimPeaksData::setParam2(epicsFloat64 value) {
  m_param2 = value;
}

void ADSimPeaksData::setBinX(epicsInt32 value) {
  m_bin_x = value;
}

void ADSimPeaksData::setBinY(epicsInt32 value) {
  m_bin_y = value;
}



