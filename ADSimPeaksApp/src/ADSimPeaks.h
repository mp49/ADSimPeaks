/**
 * \brief areaDetector driver to simulate 1D or 2D peaks with background 
 *        profiles and noise. 
 *
 * More detailed documentation can be found in the source file.
 *
 * \author Matt Pearson 
 * \date Aug 31st, 2022 
 *
 */

#ifndef ADSIMPEAKS_H
#define ADSIMPEAKS_H

#include <string>
#include <random>

#include <epicsEvent.h>
#include "ADDriver.h"
#include "ADSimPeaksData.h"
#include "ADSimPeaksPeak.h"

/* These are the drvInfo strings that are used to identify the parameters.
 * They are used by asyn clients, including standard asyn device support */

#define ADSPIntegrateParamString   "ADSP_INTEGRATE"
#define ADSPNoiseTypeParamString   "ADSP_NOISE_TYPE"
#define ADSPNoiseLevelParamString  "ADSP_NOISE_LEVEL"
#define ADSPNoiseClampParamString  "ADSP_NOISE_CLAMP"
#define ADSPNoiseLowerParamString  "ADSP_NOISE_LOWER"
#define ADSPNoiseUpperParamString  "ADSP_NOISE_UPPER"
#define ADSPElapsedTimeParamString "ADSP_ELAPSEDTIME"
// Peak Information Params
#define ADSPPeakType1DParamString  "ADSP_PEAK_TYPE1D"
#define ADSPPeakType2DParamString  "ADSP_PEAK_TYPE2D"
#define ADSPPeakPosXParamString    "ADSP_PEAK_POSX"
#define ADSPPeakPosYParamString    "ADSP_PEAK_POSY"
#define ADSPPeakFWHMXParamString   "ADSP_PEAK_FWHMX"
#define ADSPPeakFWHMYParamString   "ADSP_PEAK_FWHMY"
#define ADSPPeakAmpParamString     "ADSP_PEAK_AMP"
#define ADSPPeakCorParamString     "ADSP_PEAK_COR"
#define ADSPPeakP1ParamString      "ADSP_PEAK_P1"
#define ADSPPeakP2ParamString      "ADSP_PEAK_P2"
#define ADSPPeakMinXParamString    "ADSP_PEAK_MINX"
#define ADSPPeakMinYParamString    "ADSP_PEAK_MINY"
#define ADSPPeakMaxXParamString    "ADSP_PEAK_MAXX"
#define ADSPPeakMaxYParamString    "ADSP_PEAK_MAXY"

// Background Coefficients
// X
#define ADSPBGTypeXParamString "ADSP_BG_TYPEX"
#define ADSPBGC0XParamString  "ADSP_BG_C0X"
#define ADSPBGC1XParamString  "ADSP_BG_C1X"
#define ADSPBGC2XParamString  "ADSP_BG_C2X"
#define ADSPBGC3XParamString  "ADSP_BG_C3X"
#define ADSPBGSHXParamString  "ADSP_BG_SHX"
// Y
#define ADSPBGTypeYParamString "ADSP_BG_TYPEY"
#define ADSPBGC0YParamString  "ADSP_BG_C0Y"
#define ADSPBGC1YParamString  "ADSP_BG_C1Y"
#define ADSPBGC2YParamString  "ADSP_BG_C2Y"
#define ADSPBGC3YParamString  "ADSP_BG_C3Y"
#define ADSPBGSHYParamString  "ADSP_BG_SHY"

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
  int ADSPNoiseTypeParam;
  int ADSPNoiseLevelParam;
  int ADSPNoiseClampParam;
  int ADSPNoiseLowerParam;
  int ADSPNoiseUpperParam;
  int ADSPElapsedTimeParam;
  int ADSPPeakType1DParam;
  int ADSPPeakType2DParam;
  int ADSPPeakPosXParam;
  int ADSPPeakPosYParam;
  int ADSPPeakFWHMXParam;
  int ADSPPeakFWHMYParam;
  int ADSPPeakAmpParam;
  int ADSPPeakCorParam;
  int ADSPPeakP1Param;
  int ADSPPeakP2Param;
  int ADSPPeakMinXParam;
  int ADSPPeakMinYParam;
  int ADSPPeakMaxXParam;
  int ADSPPeakMaxYParam;
  int ADSPBGTypeXParam;
  int ADSPBGTypeYParam;
  int ADSPBGC0XParam;
  int ADSPBGC1XParam;
  int ADSPBGC2XParam;
  int ADSPBGC3XParam;
  int ADSPBGSHXParam;
  int ADSPBGC0YParam;
  int ADSPBGC1YParam;
  int ADSPBGC2YParam;
  int ADSPBGC3YParam;
  int ADSPBGSHYParam;
  
  //Internal data
  epicsUInt32 m_maxSizeX;
  epicsUInt32 m_maxSizeY;
  epicsUInt32 m_maxPeaks;
  bool m_2d;
  bool m_acquiring;
  epicsUInt32 m_uniqueId;
  
  epicsEventId m_startEvent;
  epicsEventId m_stopEvent;

  bool m_initialized;

  NDArray *p_NDArray;
  bool m_needNewArray;
  bool m_needReset;

  std::default_random_engine m_rand_gen;

  // Create object used to access the various probability
  // distributions and other types of peaks.
  ADSimPeaksPeak m_peaks;
  
  /**
   * The enum for the type of noise. This needs to match
   * the list order presented to the user in the database.
   */
  enum class e_noise_type {
    none = 0,
    uniform,
    gaussian  
  };

  /**
   * The enum for the type of backgroun. This needs to match
   * the list order presented to the user in the database.
   */
  enum class e_bg_type {
    none = 0,
    polynomial,
    exponential  
  };
  
  // Static Data
  static const std::string s_className;
  static const epicsFloat64 s_zeroCheck;

  asynStatus computeData(NDDataType_t dataType);
  template <typename T> asynStatus computeDataT();
  
  // Utilty Functions
  epicsFloat64 zeroCheck(epicsFloat64 value);
  
};


#endif /* ADSIMPEAKS_H */

