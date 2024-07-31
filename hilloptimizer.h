#ifndef gdoptimizer_h
#define gdoptimizer_h


// A simple hill-climbing optimizer.
template <typename floatlike>
class HillOptimizer {
private:
  vector<floatlike*> params;

public:
  void addParam(floatlike *p) {
    params.push_back(p);
  }

  void update(double loss) {
  }
};

#endif