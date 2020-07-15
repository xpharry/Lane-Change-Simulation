#include "inference.h"

namespace Inference {

//******************************************************************************
// Helper functions
//******************************************************************************

// to produce the permuation of a list of states
vector<vector<string>> product(const vector<string>& states, int repeat = 2) {
  vector<vector<string>> res(states.size());
  vector<vector<string>> output;
  for (int i = 0; i < states.size(); i++) res[i].push_back(states[i]);

  if (repeat == 1) {
    return res;
  }

  vector<vector<string>> middle = product(states, repeat - 1);

  for (int i = 0; i < res.size(); i++) {
    for (int j = 0; j < middle.size(); j++) {
      vector<string> temp(res[i]);
      temp.insert(temp.end(), middle[j].begin(), middle[j].end());
      output.push_back(temp);
    }
  }

  return output;
}

// to produce pdf
double pdf(float mean, float std, float value) {
  double u = double(value - mean) / abs(std);
  double y = (1.0 / (sqrt(2 * PI) * abs(std))) * exp(-u * u / 2.0);
  if (y == 0) return 0.00000001;
  // cout << "I am here!" << endl;
  return y;
}

//******************************************************************************
// JointParticles member functions (declared in simulation.h)
//******************************************************************************

void JointParticles::initializeUniformly(const Simulation& simulation,
                                         const vector<string>& intentions) {
  // stores infomraiton about the game, then initialize the particles
  numAgents = simulation.getOtherCars().size();
  legalIntentions = intentions;
  beliefs = Counter<vector<string>>();
  initializeParticles();
}

void JointParticles::initializeParticles() {
  std::random_device rd;
  std::mt19937 g(rd());
  vector<vector<string>> jointstates = product(legalIntentions, numAgents);
  std::shuffle(jointstates.begin(), jointstates.end(), g);
  int n = numParticles;
  int p = jointstates.size();
  particles.clear();

  while (n > p) {
    particles.insert(particles.end(), jointstates.begin(), jointstates.end());
    n -= p;
  }

  particles.insert(particles.end(), jointstates.begin(),
                   jointstates.begin() + n);
}

void JointParticles::observe(const Simulation& simulation) {
  if (beliefs.size() == 1) initializeParticles();

  vector<Car*> othercars = simulation.getOtherCars();
  Counter<vector<string>> tempCounter = Counter<vector<string>>();

  for (int i = 0; i < particles.size(); i++) {
    float prob = 1;
    vector<string> intentions = particles[i];
    for (int index = 0; index < numAgents; index++) {
      queue<float> history = ((Agent*)othercars[index])->getHistory();
      float observ = history.back();
      string intention = intentions[index];
      pff res = getMeanStandard(history, intention);
      prob *= pdf(res.first, res.second, observ);
    }
    tempCounter[intentions] += prob;
  }

  beliefs = tempCounter;
  cout << "-----------------------------------------------------------" << endl;
  cout << "[Simulation]: " << endl;
  for (const auto& item : beliefs) {
    cout << "\tBelief: ";
    for (int i = 0; i < item.first.size(); i++) {
      cout << "\t" << item.first[i] << " ";
    }
    cout << "\t" << item.second << endl;
  }

  cout << "[Simulation]: Now it has finished!" << endl;

  // resampling
  if (tempCounter.size() == 0) {
    initializeParticles();
  }
  else {
    beliefs.normalize();
    for (int i = 0; i < particles.size(); i++) {
      vector<string> newPos = sample(beliefs);
      particles[i] = newPos;
    }
  }
}

Counter<vector<string>> JointParticles::getBelief() {
  Counter<vector<string>> beliefDist = Counter<vector<string>>();

  for (int index = 0; index < particles.size(); index++) {
    beliefDist[particles[index]] += 1;
  }

  beliefDist.normalize();

  return beliefDist;
}

vector<string> JointParticles::sample(Counter<vector<string>>& distribution) {
  if (distribution.sum() != 1) distribution.normalize();

  std::vector<std::pair<vector<string>, float>> elems(distribution.begin(),
                                                      distribution.end());

  std::sort(elems.begin(), elems.end(),
            [](const std::pair<vector<string>, float>& a,
               const std::pair<vector<string>, float>& b) -> bool {
              return a.second < b.second;
            });

  vector<vector<string>> keys;
  vector<float> values;

  for (auto item : elems) {
    keys.push_back(item.first);
    values.push_back(item.second);
  }

  double choice = ((double)rand() / (RAND_MAX));
  int i = 0;
  double total = values[0];

  while (choice > total) {
    i += 1;
    total += values[i];
  }
  return keys[i > keys.size() - 1 ? keys.size() - 1 : i];
}

pff JointParticles::getMeanStandard(queue<float>& history,
                                    const string& intention) {
  int total = history.front();
  // for (int i = 0; i < history.size(); i++) {
  //   total += history[i];
  // }
  float vref = total;

  if (vref == 0) vref = 0.01;

  float sigma = 0.3 * vref;
  int index = Intention_To_Index.at(intention);

  if (index == 0) {
    return pff(0.7 * vref, sigma);
  }
  else if (index == 1) {
    return pff(vref, sigma);
  }
  // else if (index == 2) {
  //   return pff(1.5*vref,sigma);
  // }

  return pff(0, 0);
}

// to construct a joint inference array
//    JointParticles jointInference = JointParticles();
//    class MarginalInference {
//    private:
//        vector<string> legalIntentions;
//        int index;
//    public:
//        MarginalInference(int index, const Simulation& simulation) {
//            this->index = index;
//            legalIntentions = intentions;
//            initializeUniformly(simulation);
//        }
//
//        void initializeUniformly(const Simulation& gameState) {
//            if (index == 1)
//                jointInference.initializeUniformly(gameState,
//                legalIntentions);
//        }
//
//        void observe(const Simulation& gameState) {
//            if (index == 1)
//                jointInference.observe(gameState);
//        }
//        vector<float> getBelief() {
//            Counter<vector<string>> jointDistribution =
//            jointInference.getBelief(); Counter<int> dist = Counter<int>();
//            for (const auto& item: jointDistribution) {
//                int i = Intention_To_Index[item.first[index-1]];
//                dist[i] += item.second;
//            }
//            vector<float> result = vector<float>();
//            result.resize(legalIntentions.size());
//            for (const auto& item: dist)
//                result[item.first] = item.second;
//            return result;
//        }
//    };

//******************************************************************************
// MarginalInference member functions (declared in simulation.h)
//******************************************************************************

MarginalInference::MarginalInference(int index, const Simulation& simulation) {
  this->index = index;
  legalIntentions = intentions;
  initializeUniformly(simulation);
}

void MarginalInference::initializeUniformly(const Simulation& gameState) {
  if (index == 1)
    jointInference.initializeUniformly(gameState, legalIntentions);
}

void MarginalInference::observe(const Simulation& gameState) {
  if (index == 1) jointInference.observe(gameState);
}

std::vector<float> MarginalInference::getBelief() {
  Counter<vector<string>> jointDistribution = jointInference.getBelief();
  Counter<int> dist = Counter<int>();

  for (const auto& item : jointDistribution) {
    int i = Intention_To_Index.at(item.first[index - 1]);
    dist[i] += item.second;
  }

  vector<float> result = vector<float>();

  result.resize(legalIntentions.size());

  for (const auto& item : dist) result[item.first] = item.second;
  // if (result[0] > result[1]) {
  //   cout << "I am here" << endl;
  // }
  return result;
}

}
