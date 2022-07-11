/*
 * Simulate 1D and 2D peaks with 
 * background profiles and noise. 
 *
 * Matt Pearson 
 * July 11th, 2022 
 *
 */

#ifndef SIMPEAKS_H
#define SIMPEAKS_H

#include <vector>

class SimPeaks {

 public:
  SimPeaks(int sizeX, int sizeY);

  virtual ~SimPeaks();

  void addPeak(void);
  
 private:

  vector<epicsFloat64> *m_data;

};


#endif /* ADSIMPEAKS_H */
