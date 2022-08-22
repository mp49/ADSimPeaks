/**
 * \brief areaDetector driver to simulate 1D or 2D peaks with background 
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
#define ADSPNoiseLevelParamString  "ADSP_NOISE_LEVEL"
#define ADSPNoiseClampParamString   "ADSP_NOISE_CLAMP"
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
#define ADSPPeakCorrParamString    "ADSP_PEAK_CORR"
#define ADSPPeakAmpParamString     "ADSP_PEAK_AMP"
// Background Polynomial Coefficients
// X
#define ADSPBGC0XParamString  "ADSP_BG_C0X"
#define ADSPBGC1XParamString  "ADSP_BG_C1X"
#define ADSPBGC2XParamString  "ADSP_BG_C2X"
#define ADSPBGC3XParamString  "ADSP_BG_C3X"
#define ADSPBGSHXParamString  "ADSP_BG_SHX"
// Y
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
  int ADSPPeakCorrParam;
  int ADSPPeakAmpParam;
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

  /**
   * The enum for the type of 1D peak function. This needs to match
   * the list order presented to the user. 
   */
  enum class e_peak_type_1d {
    none,
    gaussian,
    lorentz,
    pseudovoigt  
  };

  /**
   * The enum for the type of 2D peak function. This needs to match
   * the list order presented to the user. 
   */
  enum class e_peak_type_2d {
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
  
  // Static Data
  static const std::string s_className;
  static const epicsFloat64 s_zeroCheck;

  asynStatus computeData(NDDataType_t dataType);
  template <typename T> asynStatus computeDataT();

  // 1D Profiles
  asynStatus computeGaussian(epicsFloat64 pos, epicsFloat64 fwhm, epicsInt32 bin, epicsFloat64 *result);
  asynStatus computeLorentz(epicsFloat64 pos, epicsFloat64 fwhm, epicsInt32 bin, epicsFloat64 *result);
  asynStatus computePseudoVoigt(epicsFloat64 pos, epicsFloat64 fwhm, epicsInt32 bin, epicsFloat64 *result);

  // 2D Profiles
  asynStatus computeGaussian2D(epicsFloat64 x_pos, epicsFloat64 y_pos, epicsFloat64 x_fwhm, epicsFloat64 y_fwhm,
			       epicsInt32 x_bin, epicsInt32 y_bin, epicsFloat64 rho, epicsFloat64 *result);
  asynStatus computeLorentz2D(epicsFloat64 x_pos, epicsFloat64 y_pos,
			      epicsFloat64 fwhm, epicsInt32 x_bin,
			      epicsInt32 y_bin, epicsFloat64 *result);
  asynStatus computePseudoVoigt2D(epicsFloat64 x_pos, epicsFloat64 y_pos,
				  epicsFloat64 x_fwhm, epicsFloat64 y_fwhm,
				  epicsInt32 x_bin, epicsInt32 y_bin,
				  epicsFloat64 *result);
  asynStatus computePseudoVoigtEta(epicsFloat64 fwhm_g, epicsFloat64 fwhm_l, epicsFloat64 *eta);

  //Utilty Functions
  epicsFloat64 zeroCheck(epicsFloat64 value);
  
};


#endif /* ADSIMPEAKS_H */
