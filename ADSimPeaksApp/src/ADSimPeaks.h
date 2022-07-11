/*
 * areaDetector driver to simulate 1D and 2D peaks with 
 * background profiles and noise. 
 *
 * Matt Pearson 
 * July 11th, 2022 
 *
 */

#ifndef ADSIMPEAKS_H
#define ADSIMPEAKS_H

#include <string>

#include <epicsEvent.h>
#include "ADDriver.h"

/* These are the drvInfo strings that are used to identify the parameters.
 * They are used by asyn clients, including standard asyn device support */

#define ADSPIntegrateParamString "ADSP_INTEGRATE"


class ADSimPeaks : public ADDriver {

 public:
  ADSimPeaks(const char *portName, int maxSizeX, int maxSizeY, int maxPeaks, NDDataType_t dataType,
	     int maxBuffers, size_t maxMemory, int priority, int stackSize);

  virtual ~ADSimPeaks();

  virtual asynStatus writeInt32(asynUser *pasynUser, epicsInt32 value);
  virtual asynStatus writeFloat64(asynUser *pasynUser, epicsFloat64 value);
  virtual void report(FILE *fp, int details);

  void ADSimPeaksTask(void);


  bool getInitialized(void);

 private:

  //Values used for pasynUser->reason, and indexes into the parameter library.
  int ADSPIntegrateParam;

  //Internal data
  epicsUInt32 m_maxSizeX;
  epicsUInt32 m_maxSizeY;
  epicsUInt32 m_maxPeaks;

  bool m_acquiring;
  
  epicsEventId m_startEvent;
  epicsEventId m_stopEvent;

  bool m_initialized;

  //Static Data
  static const std::string s_className;

};


#endif /* ADSIMPEAKS_H */
