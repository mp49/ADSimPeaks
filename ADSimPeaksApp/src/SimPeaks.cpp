
#include <algorithm>

#include "SimPeaks.h"

using std::cout;
using std::cerr;
using std::endl;


SimPeaks::SimPeaks(int size)
{
  m_data = new vector<epicsFloat64>(size, 0.0);
}

SimPeaks::~SimPeaks()
{

}


