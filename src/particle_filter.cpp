/**
 * particle_filter.cpp
 *
 * Created on: Dec 12, 2016
 * Author: Tiffany Huang
 */

#include "particle_filter.h"

#include <math.h>
#include <algorithm>
#include <iostream>
#include <iterator>
#include <numeric>
#include <string>
#include <vector>

#include "helper_functions.h"

using std::discrete_distribution;
using std::normal_distribution;
using std::string;
using std::vector;

void ParticleFilter::init(double x, double y, double theta, double std[]) {
  /**
   * Set the number of particles. Initialize all particles to
   *   first position (based on estimates of x, y, theta and their uncertainties
   *   from GPS) and all weights to 1. 
   * Add random Gaussian noise to each particle.
   * NOTE: Consult particle_filter.h for more information about this method 
   *   (and others in this file).
   */
  num_particles = 100;  // Set the number of particles

  // This line creates a normal (Gaussian) distribution for x
  normal_distribution<double> dist_x(x, std[0]);

  // Create normal distributions for y and theta
  normal_distribution<double> dist_y(y, std[1]);
  normal_distribution<double> dist_theta(theta, std[2]);

  particles.resize(num_particles);

  for (auto& particle : particles) {
    particle.x = dist_x(gen);
    particle.y = dist_y(gen);
    particle.theta = dist_theta(gen);
    particle.weight = 1.0;
  }

}

void ParticleFilter::prediction(double delta_t, double std_pos[], 
                                double velocity, double yaw_rate) {
  /**
   * Add measurements to each particle and add random Gaussian noise.
   * NOTE: When adding noise you may find std::normal_distribution 
   *   and std::default_random_engine useful.
   *  http://en.cppreference.com/w/cpp/numeric/random/normal_distribution
   *  http://www.cplusplus.com/reference/random/default_random_engine/
   */
  normal_distribution<double> dist_x(0, std_pos[0]);
  normal_distribution<double> dist_y(0, std_pos[1]);
  normal_distribution<double> dist_theta(0, std_pos[2]);

  if (fabs(yaw_rate) > 1E-6) {
    for (auto& particle : particles) {
      particle.x = particle.x + velocity*(sin(particle.theta+yaw_rate*delta_t)-sin(particle.theta)) / yaw_rate + dist_x(gen);
      particle.y = particle.y + velocity*(cos(particle.theta+yaw_rate*delta_t)-cos(particle.theta)) / yaw_rate + dist_y(gen);
      particle.theta = particle.theta + yaw_rate * delta_t + dist_theta(gen);
    }
  } else {
    for (auto& particle : particles) {
      particle.x = particle.x + velocity*delta_t*cos(particle.theta) + dist_x(gen);
      particle.y = particle.y + velocity*delta_t*sin(particle.theta) + dist_y(gen);
      particle.theta = particle.theta + yaw_rate * delta_t + dist_theta(gen);
    }
  }

}

void ParticleFilter::dataAssociation(vector<LandmarkObs> predicted, 
                                     vector<LandmarkObs>& observations) {
  /**
   * TODO: Find the predicted measurement that is closest to each 
   *   observed measurement and assign the observed measurement to this 
   *   particular landmark.
   * NOTE: this method will NOT be called by the grading code. But you will 
   *   probably find it useful to implement this method and use it as a helper 
   *   during the updateWeights phase.
   */

}

const Map::single_landmark_s& findNearestNeighbour(double x, double y, const Map& map) {
  double minDist = 1E9;
  const Map::single_landmark_s* nn = &(*map.landmark_list.begin());
  for (auto& landmark : map.landmark_list) {
    double dist = (x - landmark.x_f)*(x - landmark.x_f) + (y - landmark.y_f)*(y - landmark.y_f);
    if (dist < minDist) {
      minDist = dist;
      nn = &landmark;
    }
  }
  return *nn;
}

void ParticleFilter::updateWeights(double sensor_range, double std_landmark[], 
                                   const vector<LandmarkObs> &observations, 
                                   const Map &map_landmarks) {
  /**
   * Update the weights of each particle using a mult-variate Gaussian
   *   distribution. You can read more about this distribution here: 
   *   https://en.wikipedia.org/wiki/Multivariate_normal_distribution
   * NOTE: The observations are given in the VEHICLE'S coordinate system. 
   *   Your particles are located according to the MAP'S coordinate system. 
   *   You will need to transform between the two systems. Keep in mind that
   *   this transformation requires both rotation AND translation (but no scaling).
   *   The following is a good resource for the theory:
   *   https://www.willamette.edu/~gorr/classes/GeneralGraphics/Transforms/transforms2d.htm
   *   and the following is a good resource for the actual equation to implement
   *   (look at equation 3.33) http://planning.cs.uiuc.edu/node99.html
   */

  double sigX2 = std_landmark[0]*std_landmark[0];
  double sigY2 = std_landmark[1]*std_landmark[1];

  for (auto& particle : particles) {
    particle.weight = 1.0;
    for (auto& obs : observations) {
      double x_map = particle.x + (cos(particle.theta) * obs.x) - (sin(particle.theta) * obs.y);
      double y_map = particle.y + (sin(particle.theta) * obs.x) + (cos(particle.theta) * obs.y);
      const auto& lm = findNearestNeighbour(x_map, y_map, map_landmarks);
      double P = exp(-((x_map-lm.x_f)*(x_map-lm.x_f)/2*std_landmark[0] / sigX2 + (y_map-lm.y_f)*(y_map-lm.y_f)/sigY2)) /
                  (2*M_PI*std_landmark[0]*std_landmark[1]);
      particle.weight *= P;
    }
  }
  weights.resize(particles.size());
  std::transform(particles.begin(), particles.end(), weights.begin(),[](Particle& particle) { return particle.weight; });
}

void ParticleFilter::resample() {
  /**
   * Resample particles with replacement with probability proportional
   *   to their weight. 
   * NOTE: You may find std::discrete_distribution helpful here.
   *   http://en.cppreference.com/w/cpp/numeric/random/discrete_distribution
   */
  vector<Particle> newParticles(particles.size());
  discrete_distribution<> dist_w(weights.begin(), weights.end());

  for (auto& newPart : newParticles) {
    newPart = particles[dist_w(gen)];
  }
  particles = newParticles;
}

void ParticleFilter::SetAssociations(Particle& particle, 
                                     const vector<int>& associations, 
                                     const vector<double>& sense_x, 
                                     const vector<double>& sense_y) {
  // particle: the particle to which assign each listed association, 
  //   and association's (x,y) world coordinates mapping
  // associations: The landmark id that goes along with each listed association
  // sense_x: the associations x mapping already converted to world coordinates
  // sense_y: the associations y mapping already converted to world coordinates
  particle.associations= associations;
  particle.sense_x = sense_x;
  particle.sense_y = sense_y;
}

string ParticleFilter::getAssociations(Particle best) {
  vector<int> v = best.associations;
  std::stringstream ss;
  copy(v.begin(), v.end(), std::ostream_iterator<int>(ss, " "));
  string s = ss.str();
  s = s.substr(0, s.length()-1);  // get rid of the trailing space
  return s;
}

string ParticleFilter::getSenseCoord(Particle best, string coord) {
  vector<double> v;

  if (coord == "X") {
    v = best.sense_x;
  } else {
    v = best.sense_y;
  }

  std::stringstream ss;
  copy(v.begin(), v.end(), std::ostream_iterator<float>(ss, " "));
  string s = ss.str();
  s = s.substr(0, s.length()-1);  // get rid of the trailing space
  return s;
}