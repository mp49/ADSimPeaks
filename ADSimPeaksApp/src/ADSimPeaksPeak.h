/**
 * \brief Class which implements the probability distributions and other functions 
 *        that are used to produce the peaks used by the ADSimPeaks areaDetector driver.
 *
 * More detailed documentation can be found in the source file.
 *
 * \author Matt Pearson 
 * \date Aug 31st, 2022 
 *
 */

#ifndef ADSIMPEAKSPEAK_H
#define ADSIMPEAKSPEAK_H

#include <string>

#include <epicsTypes.h>
#include <ADSimPeaksData.h>

class ADSimPeaksPeak
{
 public:
  ADSimPeaksPeak(void);
  virtual ~ADSimPeaksPeak(void);

  enum class e_status {
    success,
    error
  };
  
  e_status compute1D(const ADSimPeaksData &data, epicsUInt32 type, epicsFloat64 &result);
  e_status compute2D(const ADSimPeaksData &data, epicsUInt32 type, epicsFloat64 &result);
  
  // 1D Profiles
  e_status computeGaussian(const ADSimPeaksData &data, epicsFloat64 &result); 
  e_status computeLorentz(const ADSimPeaksData &data, epicsFloat64 &result); 
  e_status computePseudoVoigt(const ADSimPeaksData &data, epicsFloat64 &result); 
  e_status computeLaplace(const ADSimPeaksData &data, epicsFloat64 &result); 
  e_status computeTriangle(const ADSimPeaksData &data, epicsFloat64 &result); 
  e_status computeSquare(const ADSimPeaksData &data, epicsFloat64 &result); 
  e_status computeMoffat(const ADSimPeaksData &data, epicsFloat64 &result); 

  enum class e_type_1d {
    none,
    square,
    triangle,
    gaussian,
    lorentz,
    pseudovoigt,
    laplace,
    moffat
  };
  
  // 2D Profiles
  e_status computeGaussian2D(const ADSimPeaksData &data, epicsFloat64 &result); 
  e_status computeLorentz2D(const ADSimPeaksData &data, epicsFloat64 &result); 
  e_status computePseudoVoigt2D(const ADSimPeaksData &data, epicsFloat64 &result); 
  e_status computePseudoVoigtEta(epicsFloat64 fwhm_g, epicsFloat64 fwhm_l, epicsFloat64 *eta);
  e_status computeLaplace2D(const ADSimPeaksData &data, epicsFloat64 &result); 
  e_status computePyramid2D(const ADSimPeaksData &data, epicsFloat64 &result); 
  e_status computeCone2D(const ADSimPeaksData &data, epicsFloat64 &result); 
  e_status computeSquare2D(const ADSimPeaksData &data, epicsFloat64 &result); 
  e_status computeMoffat2D(const ADSimPeaksData &data, epicsFloat64 &result); 

  enum class e_type_2d {
    none,
    square,
    pyramid,
    cone,
    gaussian,
    lorentz,
    pseudovoigt,
    laplace,
    moffat
  };
  
  // Read the string names of the supported peak types
  std::string getType1DName(e_type_1d type);
  std::string getType2DName(e_type_2d type);
  
 private:

  epicsFloat64 zeroCheck(epicsFloat64 value);
  
  // Static Data
  static const epicsFloat64 s_zeroCheck;
  static const epicsFloat64 s_2s2l2;
  static const epicsFloat64 s_s2pi;
  static const epicsFloat64 s_2l2;
  static const epicsFloat64 s_pv_p1;
  static const epicsFloat64 s_pv_p2;
  static const epicsFloat64 s_pv_p3;
  static const epicsFloat64 s_pv_p4;
  static const epicsFloat64 s_pv_e1;
  static const epicsFloat64 s_pv_e2;
  static const epicsFloat64 s_pv_e3;

};

#endif //ADSIMPEAKSPEAK_H

