
#ifndef ADSIMPEAKSDATA_H
#define ADSIMPEAKSDATA_H

#include <epicsTypes.h>

class ADSimPeaksData
{

 public:
  ADSimPeaksData(void);
  virtual ~ADSimPeaksData(void);

  epicsFloat64 getPositionX(void) const;
  epicsFloat64 getPositionY(void) const;
  epicsFloat64 getFWHMX(void) const;
  epicsFloat64 getFWHMY(void) const;
  epicsFloat64 getAmplitude(void) const;
  epicsFloat64 getCorrelation(void) const;
  epicsFloat64 getParam1(void) const;
  epicsFloat64 getParam2(void) const;
  epicsInt32 getBinX(void) const;
  epicsInt32 getBinY(void) const;

  void clear(void);
  
  void setPositionX(epicsFloat64 value);
  void setPositionY(epicsFloat64 value);
  void setFWHMX(epicsFloat64 value);
  void setFWHMY(epicsFloat64 value);
  void setAmplitude(epicsFloat64 value);
  void setCorrelation(epicsFloat64 value);
  void setParam1(epicsFloat64 value);
  void setParam2(epicsFloat64 value);
  void setBinX(epicsInt32 value);
  void setBinY(epicsInt32 value);

 private:
  epicsFloat64 m_position_x;
  epicsFloat64 m_position_y;
  epicsFloat64 m_fwhm_x;
  epicsFloat64 m_fwhm_y;
  epicsFloat64 m_amplitude;
  epicsFloat64 m_correlation;
  epicsFloat64 m_param1;
  epicsFloat64 m_param2;
  epicsFloat64 m_bin_x;
  epicsFloat64 m_bin_y;

};

#endif //ADSIMPEAKSDATA_H
