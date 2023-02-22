/**
 * \brief Class which implements the probability distributions and 
 *        other functions that are used to produce the peaks used by 
 *        the ADSimPeaks areaDetector driver.
 *
 * This class is used by ADSimPeaks to calculate 1D or 2D profiles.
 * 
 * Supported 1D peak shapes are:
 * 1) Square
 * 2) Triangle
 * 3) Gaussian (normal)
 * 4) Lorentzian (also known as Cauchy)
 * 5) Voigt (implemented as a psudo-Voigt, which is an approximation)
 * 6) Laplace
 * 7) Moffat
 * 8) Smooth Step
 *
 * Supported 2D peak shapes are:
 * 1) Square
 * 2) Pyramid
 * 3) Eliptical Cone
 * 4) Gaussian (normal)
 * 5) Lorentzian (also known as Cauchy)
 * 6) Voigt (implemented as a psudo-Voigt, which is an approximation)
 * 7) Laplace
 * 8) Moffat
 * 9) Smooth Step
 *
 * \author Matt Pearson 
 * \date Aug 31st, 2022 
 *
 */

#include <cmath>

#include <ADSimPeaksPeak.h>

// Static Data (including some precalculated data for the function distributions)
// Constant used to test for 0.0
const epicsFloat64 ADSimPeaksPeak::s_zeroCheck = 1e-12;
// Constant 2.0*sqrt(2.0*log(2.0))
const epicsFloat64 ADSimPeaksPeak::s_2s2l2 = 2.3548200450309493;
// Constant sqrt(2.0*M_PI)
const epicsFloat64 ADSimPeaksPeak::s_s2pi = 2.5066282746310002;
// Constat 2.0*log(2.0)
const epicsFloat64 ADSimPeaksPeak::s_2l2 = 1.3862943611198906;
// Constant data for the psudoVoigt eta parameter
const epicsFloat64 ADSimPeaksPeak::s_pv_p1 = 2.69269;
const epicsFloat64 ADSimPeaksPeak::s_pv_p2 = 2.42843;
const epicsFloat64 ADSimPeaksPeak::s_pv_p3 = 4.47163;
const epicsFloat64 ADSimPeaksPeak::s_pv_p4 = 0.07842;
const epicsFloat64 ADSimPeaksPeak::s_pv_e1 = 1.36603;
const epicsFloat64 ADSimPeaksPeak::s_pv_e2 = 0.47719;
const epicsFloat64 ADSimPeaksPeak::s_pv_e3 = 0.11116;

/**
 * Constructor.  
 */ 
ADSimPeaksPeak::ADSimPeaksPeak(void) {
}

/**
 * Destructor
 */
ADSimPeaksPeak::~ADSimPeaksPeak(void) {
}

ADSimPeaksPeak::e_status ADSimPeaksPeak::compute1D(const ADSimPeaksData &data, e_type_1d type, epicsFloat64 &result)
{

  switch (type) {
  case e_type_1d::none:
    result = 0.0;
    return e_status::success;
    
  case e_type_1d::square:
    return computeSquare(data, result);
    
  case e_type_1d::triangle:
    return computeTriangle(data, result);
    
  case e_type_1d::gaussian:
    return computeGaussian(data, result);
    
  case e_type_1d::lorentz:
    return computeLorentz(data, result);
    
  case e_type_1d::pseudovoigt:
    return computePseudoVoigt(data, result);
    
  case e_type_1d::laplace:
    return computeLaplace(data, result);

  case e_type_1d::moffat:
    return computeMoffat(data, result);
    
  case e_type_1d::smoothstep:
    return computeSmoothStep(data, result);
  }
    
  return e_status::error;
}

std::string ADSimPeaksPeak::getType1DName(e_type_1d type)
{
  switch (type) {
  case e_type_1d::none:
    return "None";
    
  case e_type_1d::square:
    return "Square";

  case e_type_1d::triangle:
    return "Triangle";

  case e_type_1d::gaussian:
    return "Gaussian";

  case e_type_1d::lorentz:
    return "Lorentz";

  case e_type_1d::pseudovoigt:
    return "Pseudo-Voigt";

  case e_type_1d::laplace:
    return "Laplace";

  case e_type_1d::moffat:
    return "Moffat";

  case e_type_1d::smoothstep:
    return "SmoothStep";
  }

  return "None";   
}

ADSimPeaksPeak::e_status ADSimPeaksPeak::compute2D(const ADSimPeaksData &data, e_type_2d type, epicsFloat64 &result)
{
  switch (type) {
  case e_type_2d::none:
    result = 0.0;
    return e_status::success;

  case e_type_2d::square:
    return computeSquare2D(data, result);

  case e_type_2d::pyramid:
    return computePyramid2D(data, result);

  case e_type_2d::cone:
    return computeCone2D(data, result);

  case e_type_2d::gaussian:
    return computeGaussian2D(data, result);

  case e_type_2d::lorentz:
    return computeLorentz2D(data, result);

  case e_type_2d::pseudovoigt:
    return computePseudoVoigt2D(data, result);

  case e_type_2d::laplace:
    return computeLaplace2D(data, result);

  case e_type_2d::moffat:
    return computeMoffat2D(data, result);

  case e_type_2d::smoothstep:
    return computeSmoothStep2D(data, result);
  }
    
  return e_status::error;
}

std::string ADSimPeaksPeak::getType2DName(e_type_2d type)
{
  switch (type) {
  case e_type_2d::none:
    return "None";

  case e_type_2d::square:
    return "Square";

  case e_type_2d::pyramid:
    return "Pyramid";

  case e_type_2d::cone:
    return "Cone";

  case e_type_2d::gaussian:
    return "Gaussian";

  case e_type_2d::lorentz:
    return "Lorentz";

  case e_type_2d::pseudovoigt:
    return "Pseudo-Voigt";

  case e_type_2d::laplace:
    return "Laplace";

  case e_type_2d::moffat:
    return "Moffat";

  case e_type_2d::smoothstep:
    return "SmoothStep";
  }
  
  return "None";   
}


/*******************************************************************************************/
/* Implementations of the various probability distribution functions and other peak shapes */

/**
 * Implementation of a Gaussian function.
 * 
 * For more information on this see:
 * https://en.wikipedia.org/wiki/Normal_distribution
 * https://en.wikipedia.org/wiki/Gaussian_function
 *
 * /arg /c ADSimPeaksData object defining the peak position, shape and the array bin
 * /arg /c result This will be used to return the result of the calculation
 *
 * /return ADSimPeaksPeak::e_status
 */
ADSimPeaksPeak::e_status ADSimPeaksPeak::computeGaussian(const ADSimPeaksData& data, epicsFloat64 &result)
{
  epicsFloat64 pos = data.getPositionX();
  epicsFloat64 fwhm = data.getFWHMX();
  epicsInt32 bin = data.getBinX();  
  fwhm = std::max(1.0, fwhm);

  // This uses some class static constant data that has been pre-computed
  epicsFloat64 sigma = fwhm / s_2s2l2;
  
  result = (1.0 / (sigma*s_s2pi)) * exp(-(((bin-pos)*(bin-pos))) / (2.0*(sigma*sigma)));
  
  return e_status::success;
}

/**
 * Implementation of a Cauchy-Lorentz function.
 * 
 * For more information on this see:
 * https://en.wikipedia.org/wiki/Cauchy_distribution
 *
 * /arg /c ADSimPeaksData object defining the peak position, shape and the array bin
 * /arg /c result This will be used to return the result of the calculation
 *
 * /return ADSimPeaksPeak::e_status
 */
ADSimPeaksPeak::e_status ADSimPeaksPeak::computeLorentz(const ADSimPeaksData& data, epicsFloat64 &result)
{
  epicsFloat64 pos = data.getPositionX();
  epicsFloat64 fwhm = data.getFWHMX();
  epicsInt32 bin = data.getBinX();  
  fwhm = std::max(1.0, fwhm);
  
  epicsFloat64 gamma = fwhm / 2.0;
  result = (1 / (M_PI*gamma)) * ((gamma*gamma) / (((bin-pos)*(bin-pos)) + (gamma*gamma)));

  return e_status::success;
}

/**
 * Implementation of the approximation of the Voigt function (known as the Psudo-Voigt).
 * 
 * For more information on this see:
 * https://en.wikipedia.org/wiki/Voigt_profile
 *
 * /arg /c ADSimPeaksData object defining the peak position, shape and the array bin
 * /arg /c result This will be used to return the result of the calculation
 *
 * /return ADSimPeaksPeak::e_status
 */
ADSimPeaksPeak::e_status ADSimPeaksPeak::computePseudoVoigt(const ADSimPeaksData& data, epicsFloat64 &result)
{
  //This implementation assumes the FWHM of the Gaussian and Lorentz is the same. However, we
  //still use the full approximation for the Pseudo-Voigt total FWHM (fwhm_tot) and use two FWHM parameters
  //(fwhm_g and fwhm_l), so that this function can easily be modified to use a different Gaussian
  //and Lorentzian FWHM.
  
  epicsFloat64 fwhm_g = data.getFWHMX(); 
  epicsFloat64 fwhm_l = data.getFWHMX(); 
  epicsFloat64 eta = 0.0;
  epicsFloat64 gaussian = 0.0;
  epicsFloat64 lorentz = 0.0;

  fwhm_g = std::max(1.0, fwhm_g);
  fwhm_l = std::max(1.0, fwhm_l);
  
  computePseudoVoigtEta(fwhm_g, fwhm_l, &eta);
  computeGaussian(data, gaussian);
  computeLorentz(data, lorentz);

  result = ((1.0 - eta)*gaussian) + (eta*lorentz);

  return e_status::success;
}

/**
 * Implementation of a Laplace function.
 * 
 * For more information on this see:
 * https://en.wikipedia.org/wiki/Laplace_distribution
 *
 * The FWHM can be calculated by determining the height when pos=bin, then taking 1/2 
 * that value and determining the value of 'bin' when the function equals that height, 
 * then doubling the result. Then we can calculate 'b' from our input FWHM.
 *
 * /arg /c ADSimPeaksData object defining the peak position, shape and the array bin
 * /arg /c result This will be used to return the result of the calculation
 *
 * /return ADSimPeaksPeak::e_status
 */
ADSimPeaksPeak::e_status ADSimPeaksPeak::computeLaplace(const ADSimPeaksData& data, epicsFloat64 &result)
{
  epicsFloat64 pos = data.getPositionX();
  epicsFloat64 fwhm = data.getFWHMX();
  epicsInt32 bin = data.getBinX();
  
  fwhm = std::max(1.0, fwhm);

  // This uses some class static constant data that has been pre-computed
  epicsFloat64 b = fwhm / s_2l2;
  result = (1.0/(2.0*b)) * exp(-((abs(bin - pos))/b));

  return e_status::success;
}


/**
 * Implementation of a simple isosceles triangle.
 *
 * /arg /c ADSimPeaksData object defining the peak position, shape and the array bin
 * /arg /c result This will be used to return the result of the calculation
 *
 * /return ADSimPeaksPeak::e_status
 */
ADSimPeaksPeak::e_status ADSimPeaksPeak::computeTriangle(const ADSimPeaksData& data, epicsFloat64 &result)
{
  epicsFloat64 pos = data.getPositionX();
  epicsFloat64 fwhm = data.getFWHMX();
  epicsInt32 bin = data.getBinX();  

  fwhm = std::max(1.0, fwhm);

  epicsFloat64 peak = 1.0;
  epicsFloat64 b = peak/fwhm;

  if (bin <= static_cast<epicsInt32>(pos)) {
    b = b*1.0;
  } else {
    b = b*-1.0;
  }

  result = peak + b*(bin-pos);
  result = std::max(0.0, result);

  return e_status::success;
}

/**
 * Implementation of a simple square.
 *
 * /arg /c ADSimPeaksData object defining the peak position, shape and the array bin
 * /arg /c result This will be used to return the result of the calculation
 *
 * /return ADSimPeaksPeak::e_status
 */
ADSimPeaksPeak::e_status ADSimPeaksPeak::computeSquare(const ADSimPeaksData& data, epicsFloat64 &result)
{
  epicsFloat64 pos = data.getPositionX();
  epicsFloat64 fwhm = data.getFWHMX();
  epicsInt32 bin = data.getBinX();  

  fwhm = std::max(1.0, fwhm);

  epicsFloat64 peak = 1.0;
  
  if ((bin > static_cast<epicsInt32>(pos - fwhm/2.0)) && (bin <= static_cast<epicsInt32>(pos + fwhm/2.0))) {
    result = peak;
  } else {
    result = 0.0;
  }

  return e_status::success;
}

/**
 * Implementation of a Moffat distribution. The Moffat function is determined by 
 * the alpha and beta 'seeing' parameters. We calculate alpha based on the input 
 * FWHM and beta. The beta parameter determins the shape of the function. Large 
 * values of beta (>>1) will cause the distribution to be similar to a gaussian, 
 * and small values (<1) will cause it to look like an exponential.
 *
 * For more information on this see:
 * https://en.wikipedia.org/wiki/Moffat_distribution
 *
 * /arg /c ADSimPeaksData object defining the peak position, shape and the array bin
 * /arg /c result This will be used to return the result of the calculation
 *
 * /return ADSimPeaksPeak::e_status
 */
ADSimPeaksPeak::e_status ADSimPeaksPeak::computeMoffat(const ADSimPeaksData& data, epicsFloat64 &result)
{
  epicsFloat64 pos = data.getPositionX();
  epicsFloat64 fwhm = data.getFWHMX();
  epicsFloat64 beta = data.getParam1();
  epicsInt32 bin = data.getBinX();  

  fwhm = std::max(1.0, fwhm);
  beta = zeroCheck(beta);

  epicsFloat64 alpha = fwhm / (2.0 * sqrt(pow(2.0,1.0/beta) - 1));
  epicsFloat64 alpha2 = alpha*alpha;
  
  result = ((beta-1)/(M_PI*alpha2)) * pow((1 + (((bin-pos)*(bin-pos))/alpha2)),-beta);

  return e_status::success;
}

/**
 * Implementation of a smooth step function.
 *
 * This is not really a peak function, but is useful for creating step functions.
 * The peak center is the center of the step distribution. The FWHM is used
 * for the width of the step.
 * 
 * For more information on this see:
 * https://en.wikipedia.org/wiki/Smoothstep
 *
 * /arg /c ADSimPeaksData object defining the peak position, shape and the array bin
 * /arg /c result This will be used to return the result of the calculation
 *
 * /return ADSimPeaksPeak::e_status
 */
ADSimPeaksPeak::e_status ADSimPeaksPeak::computeSmoothStep(const ADSimPeaksData& data, epicsFloat64 &result)
{
  epicsFloat64 pos = data.getPositionX();
  epicsFloat64 fwhm = data.getFWHMX();
  epicsInt32 bin = data.getBinX();  

  fwhm = std::max(1.0, fwhm);

  epicsFloat64 low_edge = pos - fwhm/2.0;
  result = std::max(0.0, std::min((bin-low_edge)/fwhm, 1.0));
  result = 6*pow(result,5) - 15*pow(result,4) + 10*pow(result,3);

  return e_status::success;
}


/**
 * Implementation of a bivariate Gaussian function.
 * 
 * For more information on this see:
 * https://en.wikipedia.org/wiki/Normal_distribution
 * https://en.wikipedia.org/wiki/Gaussian_function
 *
 * /arg /c ADSimPeaksData object defining the peak position, shape and the array bins (x,y)
 * /arg /c result This will be used to return the result of the calculation
 *
 * /return ADSimPeaksPeak::e_status
 */
ADSimPeaksPeak::e_status ADSimPeaksPeak::computeGaussian2D(const ADSimPeaksData& data, epicsFloat64 &result)
{
  epicsFloat64 x_pos = data.getPositionX();
  epicsFloat64 y_pos = data.getPositionY();
  epicsFloat64 x_fwhm = data.getFWHMX();
  epicsFloat64 y_fwhm = data.getFWHMY();
  epicsFloat64 rho = data.getCorrelation();
  epicsInt32 x_bin = data.getBinX();
  epicsInt32 y_bin = data.getBinY();
  
  x_fwhm = std::max(1.0, x_fwhm);
  y_fwhm = std::max(1.0, y_fwhm);
  rho = std::min(1.0, std::max(-1.0, rho));

  // This uses some class static constant data that has been pre-computed
  epicsFloat64 x_sig = x_fwhm / s_2s2l2;
  epicsFloat64 y_sig = y_fwhm / s_2s2l2;

  epicsFloat64 xy_amp = 1.0 / (2.0 * M_PI * x_sig * y_sig * sqrt(1-(rho*rho)));
  epicsFloat64 xy_factor = -1 / (2*(1-(rho*rho)));
  epicsFloat64 xy_calc1 = (x_bin-x_pos)/x_sig;
  epicsFloat64 xy_calc2 = (y_bin-y_pos)/y_sig;
    
  result = xy_amp * exp(xy_factor*(xy_calc1*xy_calc1 - 2*rho*xy_calc1*xy_calc2 + xy_calc2*xy_calc2));

  return e_status::success;
}

/**
 * Implementation of a bivariate Cauchy-Lorentz function.
 * 
 * For more information on this see:
 * https://en.wikipedia.org/wiki/Cauchy_distribution
 * 
 * I can only find bivariate Cauchy functions that are symmetric in X and Y so we just use
 * a single FWHM (taken as the X FWHM). 
 *
 * /arg /c ADSimPeaksData object defining the peak position, shape and the array bins (x,y)
 * /arg /c result This will be used to return the result of the calculation
 *
 * /return ADSimPeaksPeak::e_status
 */
ADSimPeaksPeak::e_status ADSimPeaksPeak::computeLorentz2D(const ADSimPeaksData& data, epicsFloat64 &result)
{
  epicsFloat64 x_pos = data.getPositionX();
  epicsFloat64 y_pos = data.getPositionY(); 
  epicsFloat64 fwhm = data.getFWHMX();
  epicsInt32 x_bin = data.getBinX();
  epicsInt32 y_bin = data.getBinY();

  fwhm = std::max(1.0, fwhm);
  
  epicsFloat64 gamma = fwhm / 2.0;
  epicsFloat64 xy_calc1 = x_bin-x_pos;
  epicsFloat64 xy_calc2 = y_bin-y_pos;
  
  result = (1 / (2*M_PI)) * (gamma / pow(((xy_calc1*xy_calc1) + (xy_calc2*xy_calc2) + gamma*gamma),1.5));

  return e_status::success;
}

/**
 * Implementation of the approximation of the bivariate Voigt function 
 * (known as the Psudo-Voigt). The Guassian part of the function 
 * can be defined with different FWHM parameters in X and Y, and with
 * a skewed shape, but for the purposes of this approximation we
 * assume it has zero skew and an average is taken 
 * as the Lorenztian FWHM component.
 * 
 * For more information on this see:
 * https://en.wikipedia.org/wiki/Voigt_profile
 *
 * /arg /c ADSimPeaksData object defining the peak position, shape and the array bins (x,y)
 * /arg /c result This will be used to return the result of the calculation
 *
 * /return ADSimPeaksPeak::e_status
 */
ADSimPeaksPeak::e_status ADSimPeaksPeak::computePseudoVoigt2D(const ADSimPeaksData& data, epicsFloat64 &result)
{
  epicsFloat64 x_fwhm = data.getFWHMX();
  epicsFloat64 y_fwhm = data.getFWHMY();   
  epicsFloat64 fwhm_av = 0.0;
  epicsFloat64 fwhm_g = 0.0;
  epicsFloat64 fwhm_l = 0.0;
  epicsFloat64 eta = 0.0;
  epicsFloat64 gaussian = 0.0;
  epicsFloat64 lorentz = 0.0;
  
  x_fwhm = std::max(1.0, x_fwhm);
  y_fwhm = std::max(1.0, y_fwhm);
  
  fwhm_av = (x_fwhm+y_fwhm)/2.0;
  fwhm_g = fwhm_av;
  fwhm_l = fwhm_av;

  computePseudoVoigtEta(fwhm_g, fwhm_l, &eta);
  computeGaussian2D(data, gaussian);
  computeLorentz2D(data, lorentz);

  result = ((1.0 - eta)*gaussian) + (eta*lorentz);

  return e_status::success;
}

ADSimPeaksPeak::e_status ADSimPeaksPeak::computePseudoVoigtEta(epicsFloat64 fwhm_g, epicsFloat64 fwhm_l, epicsFloat64 *eta)
{
  epicsFloat64 fwhm_sum = 0.0;
  epicsFloat64 fwhm_tot = 0.0;

  // This uses some class static constant data that has been pre-computed
  fwhm_sum = pow(fwhm_g,5) + (s_pv_p1*pow(fwhm_g,4)*fwhm_l) + (s_pv_p2*pow(fwhm_g,3)*pow(fwhm_l,2)) +
            (s_pv_p3*pow(fwhm_g,2)*pow(fwhm_l,3)) + (s_pv_p4*fwhm_g*pow(fwhm_l,4)) + pow(fwhm_l,5);
  fwhm_tot = pow(fwhm_sum,0.2);
  
  *eta = ((s_pv_e1*(fwhm_l/fwhm_tot)) - (s_pv_e2*pow((fwhm_l/fwhm_tot),2)) + (s_pv_e3*pow((fwhm_l/fwhm_tot),3)));
  
  return e_status::success;
}

/**
 * Implementation of a bivariate Laplace function.
 * 
 * For more information on this see:
 * https://en.wikipedia.org/wiki/Multivariate_Laplace_distribution
 *
 * We calculate the 'b' scale factor in the same way as for ADSimPeaksPeak::computeLaplace,
 * then we calculate the standard deviation. The actual bivariate Laplace uses a modified
 * Bessle function of the second kind, see:
 * https://en.wikipedia.org/wiki/Bessel_function#Modified_Bessel_functions:_I%CE%B1,_K%CE%B1
 * but to avoid having to calculate this we just assume a decaying expoential, which seems 
 * like a good approximation. 
 *
 * /arg /c ADSimPeaksData object defining the peak position, shape and the array bins (x,y)
 * /arg /c result This will be used to return the result of the calculation
 *
 * /return ADSimPeaksPeak::e_status
 */
ADSimPeaksPeak::e_status ADSimPeaksPeak::computeLaplace2D(const ADSimPeaksData& data, epicsFloat64 &result)
{
  epicsFloat64 x_pos = data.getPositionX();
  epicsFloat64 y_pos = data.getPositionY();
  epicsFloat64 x_fwhm = data.getFWHMX();
  epicsFloat64 y_fwhm = data.getFWHMY();
  epicsFloat64 rho = data.getCorrelation();
  epicsInt32 x_bin = data.getBinX();
  epicsInt32 y_bin = data.getBinY();
    
  x_fwhm = std::max(1.0, x_fwhm);
  y_fwhm = std::max(1.0, y_fwhm);
  rho = std::min(1.0, std::max(-1.0, rho));

  // This uses some class static constant data that has been pre-computed
  // Standard deviation is sqrt(2) * the scale factor b
  epicsFloat64 x_sig = sqrt(2.0) * (x_fwhm / s_2l2);
  epicsFloat64 y_sig = sqrt(2.0) * (y_fwhm / s_2l2);

  epicsFloat64 xy_amp = 1.0 / (M_PI * x_sig * y_sig * sqrt(1-(rho*rho)));
  epicsFloat64 xy_calc1 = (x_bin-x_pos)/x_sig;
  epicsFloat64 xy_calc2 = (y_bin-y_pos)/y_sig;
    
  result = xy_amp * exp(-sqrt((2.0*(xy_calc1*xy_calc1 - 2*rho*xy_calc1*xy_calc2 + xy_calc2*xy_calc2))/(1-(rho*rho))));

  return e_status::success;
}

/**
 * Implementation of a simple pyramid.
 *
 * /arg /c ADSimPeaksData object defining the peak position, shape and the array bins (x,y)
 * /arg /c result This will be used to return the result of the calculation
 *
 * /return ADSimPeaksPeak::e_status
 */
ADSimPeaksPeak::e_status ADSimPeaksPeak::computePyramid2D(const ADSimPeaksData& data, epicsFloat64 &result)
{
  epicsFloat64 x_pos = data.getPositionX();
  epicsFloat64 y_pos = data.getPositionY();
  epicsFloat64 x_fwhm = data.getFWHMX();
  epicsFloat64 y_fwhm = data.getFWHMY();
  epicsInt32 x_bin = data.getBinX();
  epicsInt32 y_bin = data.getBinY();
    
  x_fwhm = std::max(1.0, x_fwhm);
  y_fwhm = std::max(1.0, y_fwhm);

  epicsFloat64 peak = 1.0;
  epicsFloat64 b = peak/x_fwhm;
  epicsFloat64 c = peak/y_fwhm;
  if ((x_bin <= static_cast<epicsInt32>(x_pos)) && (y_bin <= static_cast<epicsInt32>(y_pos))) {
    b = b*1.0;
    c = c*1.0;
  } else if ((x_bin <= static_cast<epicsInt32>(x_pos)) && (y_bin > static_cast<epicsInt32>(y_pos))) {
    b = b*1.0;
    c = c*-1.0;
  } else if ((x_bin > static_cast<epicsInt32>(x_pos)) && (y_bin <= static_cast<epicsInt32>(y_pos))) {
    b = b*-1.0;
    c = c*1.0;
  } else {
    b = b*-1.0;
    c = c*-1.0;
  }
  
  result = peak + b*(x_bin-x_pos) + c*(y_bin-y_pos);
  result = std::max(0.0, result);
  
  return e_status::success;
}

/**
 * Implementation of an eliptical cone.
 *
 * /arg /c ADSimPeaksData object defining the peak position, shape and the array bins (x,y)
 * /arg /c result This will be used to return the result of the calculation
 *
 * /return ADSimPeaksPeak::e_status
 */
ADSimPeaksPeak::e_status ADSimPeaksPeak::computeCone2D(const ADSimPeaksData& data, epicsFloat64 &result)
{
  epicsFloat64 x_pos = data.getPositionX();
  epicsFloat64 y_pos = data.getPositionY();
  epicsFloat64 x_fwhm = data.getFWHMX();
  epicsFloat64 y_fwhm = data.getFWHMY();
  epicsInt32 x_bin = data.getBinX();
  epicsInt32 y_bin = data.getBinY();
  
  x_fwhm = std::max(1.0, x_fwhm);
  y_fwhm = std::max(1.0, y_fwhm);

  epicsFloat64 peak = x_fwhm + y_fwhm;

  epicsFloat64 height = 0.0;
  // Find the distance of this point from the center of the ellipse
  epicsFloat64 d = sqrt((x_bin-x_pos)*(x_bin-x_pos) + (y_bin-y_pos)*(y_bin-y_pos));
  if (d != 0) {
    // Find the angle of this point
    epicsFloat64 theta = asin((y_bin-y_pos)/d);
    // Find the radius of the ellipse defining the edge of the cone at this same angle 
    epicsFloat64 r = (x_fwhm * y_fwhm) / sqrt(pow(y_fwhm*cos(theta),2) + pow(x_fwhm*sin(theta),2));
    // Find the height of the cone inside the ellipse
    height = (r-d) * (peak/r);
  } else {
    height = peak;
  }
  
  result = height;
  result = std::max(0.0, result);
  
  return e_status::success;
}

/**
 * Implementation of a cube peak, which looks like a square from the top.
 *
 * /arg /c ADSimPeaksData object defining the peak position, shape and the array bins (x,y)
 * /arg /c result This will be used to return the result of the calculation
 *
 * /return ADSimPeaksPeak::e_status
 */
ADSimPeaksPeak::e_status ADSimPeaksPeak::computeSquare2D(const ADSimPeaksData& data, epicsFloat64 &result)
{
  epicsFloat64 x_pos = data.getPositionX();
  epicsFloat64 y_pos = data.getPositionY();
  epicsFloat64 x_fwhm = data.getFWHMX();
  epicsFloat64 y_fwhm = data.getFWHMY();
  epicsInt32 x_bin = data.getBinX();
  epicsInt32 y_bin = data.getBinY();
  
  x_fwhm = std::max(1.0, x_fwhm);
  y_fwhm = std::max(1.0, y_fwhm);

  epicsFloat64 peak = 1.0;

  if ((x_bin >  static_cast<epicsInt32>(x_pos - x_fwhm/2.0)) &&
      (x_bin <= static_cast<epicsInt32>(x_pos + x_fwhm/2.0)) &&
      (y_bin >  static_cast<epicsInt32>(y_pos - y_fwhm/2.0)) &&
      (y_bin <= static_cast<epicsInt32>(y_pos + y_fwhm/2.0))) {
    result = peak;
  } else {
    result = 0.0;
  }
  
  return e_status::success;
}

/**
 * Implementation of a bivariate Moffat distribution. The Moffat function is determined by the alpha 
 * and beta 'seeing' parameters. We calculate alpha based on the input FWHM and beta. The 
 * beta parameter determins the shape of the function. Large values of beta (>>1) will cause 
 * the distribution to be similar to a gaussian, and small values (<1) will cause it to look 
 * like an exponential.
 *
 * For more information on this see:
 * https://en.wikipedia.org/wiki/Moffat_distribution
 *
 * /arg /c ADSimPeaksData object defining the peak position, shape and the array bins (x,y)
 * /arg /c result This will be used to return the result of the calculation
 *
 * /return ADSimPeaksPeak::e_status
 */
ADSimPeaksPeak::e_status ADSimPeaksPeak::computeMoffat2D(const ADSimPeaksData& data, epicsFloat64 &result)
{
  epicsFloat64 x_pos = data.getPositionX();
  epicsFloat64 y_pos = data.getPositionY();
  epicsFloat64 fwhm = data.getFWHMX();
  epicsFloat64 beta = data.getParam1();
  epicsInt32 x_bin = data.getBinX();
  epicsInt32 y_bin = data.getBinY();
  
  fwhm = std::max(1.0, fwhm);
  beta = zeroCheck(beta);

  epicsFloat64 alpha = fwhm / (2.0 * sqrt(pow(2.0,1.0/beta) - 1));
  epicsFloat64 alpha2 = alpha*alpha;

  result = ((beta-1)/(M_PI*alpha2))
    * pow((1 + ((((x_bin-x_pos)*(x_bin-x_pos)) + ((y_bin-y_pos)*(y_bin-y_pos)))/alpha2)),-beta);
  
  return e_status::success;
}

/**
 * Implementation of a bivariate smooth step function.
 *
 * This is not really a peak function, but is useful for creating step functions.
 * The peak center is the center of the step distribution. The FWHM is used
 * for the width of the step.
 * 
 * For more information on this see:
 * https://en.wikipedia.org/wiki/Smoothstep
 *
 * /arg /c ADSimPeaksData object defining the peak position, shape and the array bin
 * /arg /c result This will be used to return the result of the calculation
 *
 * /return ADSimPeaksPeak::e_status
 */
ADSimPeaksPeak::e_status ADSimPeaksPeak::computeSmoothStep2D(const ADSimPeaksData& data, epicsFloat64 &result)
{
  epicsFloat64 x_pos = data.getPositionX();
  epicsFloat64 y_pos = data.getPositionY();
  epicsFloat64 x_fwhm = data.getFWHMX();
  epicsFloat64 y_fwhm = data.getFWHMY();
  epicsInt32 x_bin = data.getBinX();
  epicsInt32 y_bin = data.getBinY();

  x_fwhm = std::max(1.0, x_fwhm);
  y_fwhm = std::max(1.0, y_fwhm);

  epicsFloat64 x_low_edge = x_pos - x_fwhm/2.0;
  epicsFloat64 y_low_edge = y_pos - y_fwhm/2.0;
  
  result = (std::max(0.0, std::min((x_bin-x_low_edge)/x_fwhm, 1.0)) +
	    std::max(0.0, std::min((y_bin-y_low_edge)/y_fwhm, 1.0))) / 2.0;
  result = 6*pow(result,5) - 15*pow(result,4) + 10*pow(result,3);

  return e_status::success;
}


/**
 * Utility function to check if a floating point number is close to zero.
 *
 * /arg /c value The value to check 
 *
 * /return The original input value or 1.0 (if it was too close to zero)
 */
epicsFloat64 ADSimPeaksPeak::zeroCheck(epicsFloat64 value)
{
  if ((value > -s_zeroCheck) && (value < s_zeroCheck)) {
    return 1.0;
  } else {
    return value;
  }
}
 
