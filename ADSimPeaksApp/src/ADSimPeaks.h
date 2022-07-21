/**
 * \brief areaDetector driver to simulate 1D peaks with background 
 *        profiles and noise. 
 *
 * More detailed documentation can be found in the source file.
 *
 * \author Matt Pearson 
 * \date July 11th, 2022 
 *
 */

#ifndef ADSIMPEAKS_H
#define ADSIMPEAKS_H

#include <string>
#include <random>

#include <epicsEvent.h>
#include "ADDriver.h"

/* These are the drvInfo strings that are used to identify the parameters.
 * They are used by asyn clients, including standard asyn device support */

#define ADSPIntegrateParamString   "ADSP_INTEGRATE"
#define ADSPNoiseTypeParamString   "ADSP_NOISE_TYPE"
#define ADSPNoiseLevelParamString   "ADSP_NOISE_LEVEL"
#define ADSPElapsedTimeParamString "ADSP_ELAPSEDTIME"
// Peak Information Params
#define ADSPPeakTypeParamString  "ADSP_PEAK_TYPE"
#define ADSPPeakPosParamString   "ADSP_PEAK_POS"
#define ADSPPeakFWHMParamString  "ADSP_PEAK_FWHM"
#define ADSPPeakMaxParamString "ADSP_PEAK_MAX"
// Background Polynomial Coefficients
#define ADSPBGC0ParamString  "ADSP_BG_C0"
#define ADSPBGC1ParamString  "ADSP_BG_C1"
#define ADSPBGC2ParamString  "ADSP_BG_C2"
#define ADSPBGC3ParamString  "ADSP_BG_C3"
#define ADSPBGSHParamString  "ADSP_BG_SH"

class ADSimPeaks : public ADDriver {

 public:
  ADSimPeaks(const char *portName, int maxSize, int maxPeaks, NDDataType_t dataType,
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
  int ADSPNoiseTypeParam;
  int ADSPNoiseLevelParam;
  int ADSPElapsedTimeParam;
  int ADSPPeakTypeParam;
  int ADSPPeakPosParam;
  int ADSPPeakFWHMParam;
  int ADSPPeakMaxParam;
  int ADSPBGC0Param;
  int ADSPBGC1Param;
  int ADSPBGC2Param;
  int ADSPBGC3Param;
  int ADSPBGSHParam;
  
  //Internal data
  epicsUInt32 m_maxSize;
  epicsUInt32 m_maxPeaks;

  bool m_acquiring;
  epicsUInt32 m_uniqueId;
  
  epicsEventId m_startEvent;
  epicsEventId m_stopEvent;

  bool m_initialized;

  NDArray *p_NDArray;
  bool m_needNewArray;
  bool m_needReset;

  std::default_random_engine m_rand_gen;

  /**
   * The enum for the type of peak function. This needs to match
   * the list order presented to the user. 
   */
  enum class e_peak_type {
    none,
    gaussian,
    lorentz,
    pseudovoigt  
  };

  /**
   * The enum for the type of noise. This needs to match
   * the list order presented to the user. 
   */
  enum class e_noise_type {
    none,
    uniform,
    gaussian  
  };
  
  //Static Data
  static const std::string s_className;
  static const epicsFloat64 s_zeroCheck;

  asynStatus computeData(NDDataType_t dataType);
  template <typename T> asynStatus computeDataT();

  asynStatus computeGaussian(epicsFloat64 pos, epicsFloat64 fwhm, epicsInt32 bin, epicsFloat64 *result);
  asynStatus computeLorentz(epicsFloat64 pos, epicsFloat64 fwhm, epicsInt32 bin, epicsFloat64 *result);
  asynStatus computePseudoVoigt(epicsFloat64 pos, epicsFloat64 fwhm, epicsInt32 bin, epicsFloat64 *result);
  epicsFloat64 zeroCheck(epicsFloat64 value);
  
};


#endif /* ADSIMPEAKS_H */
