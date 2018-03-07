#include "MPC.h"

using CppAD::AD;
using namespace std;

// TODO: Set the timestep length and duration
size_t N = 6;
double dt = 0.275;

double dCost_fac_ctr = 22.0;
double dCost_fac_epsi = 20.0;
double dCost_fac_v = 2.0;
double dCost_fac_steering = 30000.0;
double dCost_fac_steering_quad = 400.0;
double dCost_fac_throttle = 1.0;
double dCost_fac_steering_slope = 30.0;

double dCost_fac_throttle_slope = 1.0;
double dDt_Latency = 0.10;

double ref_v = 95*0.44704; // [m/s]
double ref_cte = 0;
double ref_epsi = 0;

// It was obtained by measuring the radius formed by running the vehicle in the
// simulator around in a circle with a constant steering angle and velocity on a
// flat terrain.
//
// Lf was tuned until the the radius formed by the simulating the model
// presented in the classroom matched the previous radius.
//
// This is the length from front to CoG that has a similar radius.
const double Lf = 2.67;

// The solver takes all the state variables and actuator
// variables in a singular vector. Thus, we should to establish
// when one variable starts and another ends to make our lifes easier.
size_t x_start = 0;
size_t y_start = x_start + N;
size_t psi_start = y_start + N;
size_t v_start = psi_start + N;
size_t cte_start = v_start + N;
size_t epsi_start = cte_start + N;
size_t delta_start = epsi_start + N;
size_t a_start = delta_start + N - 1;
//*/

class FG_eval {
 public:
  // Fitted polynomial coefficients
  Eigen::VectorXd coeffs;
  FG_eval(Eigen::VectorXd coeffs) { this->coeffs = coeffs; }

  typedef CPPAD_TESTVECTOR(AD<double>) ADvector;
  void operator()(ADvector& fg, const ADvector& vars) {
    // Implementation of MPC
    // `fg` is a vector of the cost constraints
    // `vars` is a vector of variable values (state & actuators)
    // We go back and forth between this function and
    // the Solver function below.
    // The cost is stored is the first element of `fg`.
    // Any additions to the cost should be added to `fg[0]`.

/***
1. Cost Vector
***/
    fg[0] = 0;

    // Reference State Cost
    // Define the cost related to the reference state and
    // anything beneficial.
    for(unsigned int t = 0; t < N; t++){
      fg[0] += dCost_fac_ctr  * CppAD::pow(vars[cte_start + t] - ref_cte, 2);
      fg[0] += dCost_fac_epsi * CppAD::pow(vars[epsi_start + t] - ref_epsi, 2);
      fg[0] += dCost_fac_v    * CppAD::pow(ref_v-vars[v_start+t], 2);
    }
    for(unsigned int t = 0; t < N - 1; t++){
      //fg[0] += dCost_fac_steering * CppAD::pow(vars[delta_start+t],4);
      fg[0] += dCost_fac_steering * CppAD::pow(vars[delta_start+t],2);
      fg[0] += dCost_fac_steering_quad * CppAD::pow(vars[delta_start+t],4);
      fg[0] += dCost_fac_throttle * CppAD::pow(vars[a_start + t], 2);
    }

    // Minimize the value gap between sequential actuations.
    for (unsigned int t = 0; t < N - 2; t++) {
      fg[0] += dCost_fac_steering_slope * CppAD::pow(vars[delta_start + t + 1] - vars[delta_start + t], 2);
      fg[0] += dCost_fac_throttle_slope * CppAD::pow(vars[a_start + t + 1] - vars[a_start + t], 2);
    }

    //
    // Setup Constraints
    //
    // setup the model constraints.

    // Initial constraints
    //
    // We add 1 to each of the starting indices due to cost being located at
    // index 0 of `fg`.
    // This bumps up the position of all the other values.

    fg[1 + x_start] = vars[x_start];
    fg[1 + y_start] = vars[y_start];
    fg[1 + psi_start] = vars[psi_start];
    fg[1 + v_start] = vars[v_start];
    fg[1 + cte_start] = vars[cte_start];
    fg[1 + epsi_start] = vars[epsi_start];

/***
2. Kinematic Model
***/

    // The rest of the constraints
    for (unsigned int t = 1; t < N; t++) {
      // The state at time t+1 .
      AD<double> x1 = vars[x_start + t];
      AD<double> y1 = vars[y_start + t];
      AD<double> psi1 = vars[psi_start + t];
      AD<double> v1 = vars[v_start + t];
      AD<double> cte1 = vars[cte_start + t];
      AD<double> epsi1 = vars[epsi_start + t];

      // The state at time t.
      AD<double> x0 = vars[x_start + t - 1];
      AD<double> y0 = vars[y_start + t - 1];
      AD<double> psi0 = vars[psi_start + t - 1];
      AD<double> v0 = vars[v_start + t - 1];
      AD<double> cte0 = vars[cte_start + t - 1];
      AD<double> epsi0 = vars[epsi_start + t - 1];
      AD<double> delta0 = vars[delta_start + t -1];
      AD<double> a0 = vars[a_start + t - 1];

      // Here's `x` to get you started.
      // The idea here is to constraint this value to be 0.
      //
      // Use of `AD<double>` and use of `CppAD`!
      // This is also CppAD can compute derivatives and pass
      // these to the solver.

      AD<double> f0 = cppadPolyeval(coeffs, x0, false);
      AD<double> psides0 = CppAD::atan(cppadPolyeval(coeffs, x0, true));
      AD<double> psi1_ = psi0 + v0*dt*delta0 / Lf;

      fg[1 + x_start + t] = x1 - (x0 + v0 * CppAD::cos(psi0) * dt);
      fg[1 + y_start + t] = y1 - (y0 + v0 * CppAD::sin(psi0) * dt);
      fg[1 + psi_start + t] = psi1 - psi1_;
      fg[1 + v_start + t] = v1 - (v0 + a0*dt);
      fg[1 + cte_start + t] = cte1 - (f0 - y0 + v0*dt*CppAD::sin(epsi0));
      fg[1 + epsi_start + t] = epsi1 - (psi1_ - psides0);
    }
  }
};

//
// MPC class definition implementation.
//
MPC::MPC() {}
MPC::~MPC() {}


void MPC::Init(){
  //typedef CPPAD_TESTVECTOR(double) Dvector;
  // Initialize last control vector
  dvLast_con = Dvector(2);
  dvLast_con[0] = 0.0;
  dvLast_con[1] = 1.0;
}


CONFIG_OUTPUT MPC::Solve(Eigen::VectorXd state, Eigen::VectorXd coeffs) {
  bool ok = true;
  //size_t i;
  //typedef CPPAD_TESTVECTOR(double) Dvector;

  double x = state[0];
  double y = state[1];
  double psi = state[2];
  double v = state[3];
  double cte = state[4];
  double epsi = state[5];
  // Set the number of model variables (includes both states and inputs).
  // For example: If the state is a 4 element vector, the actuators is a 2
  // element vector and there are 10 timesteps. The number of variables is:
  //
  // 4 * 10 + 2 * 9
  size_t n_vars = N * 6 + (N - 1) * 2;
  // Set the number of constraints
  size_t n_constraints = N * 6;

  // Initial value of the independent variables.
  // SHOULD BE 0 besides initial state.
  Dvector vars(n_vars);
  for (unsigned int i = 0; i < n_vars; i++) {
    vars[i] = 0.0;
  }

   // Apply Latency regard
  double dDelta_Lat = dvLast_con[0];
  double dA_Lat = dvLast_con[1];

  x = x + v*cos(psi)*dDt_Latency;
  y = y + v*sin(psi)*dDt_Latency;
  psi = psi + v * dDt_Latency * dDelta_Lat  / Lf;
  v = v + dA_Lat * dDt_Latency;
  cte = cte + (v * sin(epsi) * dDt_Latency);
  epsi = epsi + v * dDt_Latency * dDelta_Lat / Lf ;


  // Set the initial variable values
  vars[x_start] = x;
  vars[y_start] = y;
  vars[psi_start] = psi;
  vars[v_start] = v;
  vars[cte_start] = cte;
  vars[epsi_start] = epsi;
  // Lower and upper limits for x
  Dvector vars_lowerbound(n_vars);
  Dvector vars_upperbound(n_vars);
  // Set lower and upper limits for variables.
  for (unsigned int i = 0; i < delta_start; i++) {
    vars_lowerbound[i] = -1.0e19;
    vars_upperbound[i] = 1.0e19;
  }

  // The upper and lower limits of delta are set to -25 and 25
  // degrees (values in radians).
  for (unsigned int i = delta_start; i < a_start; i++) {
    vars_lowerbound[i] = -0.436332;
    vars_upperbound[i] = 0.436332;
  }

  // Acceleration/decceleration upper and lower limits.
  for (unsigned int i = a_start; i < n_vars; i++) {
    vars_lowerbound[i] = -1.0;
    vars_upperbound[i] = 1.0;
  }
  // Lower and upper limits for the constraints
  // Should be 0 besides initial state.
  Dvector constraints_lowerbound(n_constraints);
  Dvector constraints_upperbound(n_constraints);
  for (unsigned int i = 0; i < n_constraints; i++) {
    constraints_lowerbound[i] = 0;
    constraints_upperbound[i] = 0;
  }

  constraints_lowerbound[x_start] = x;
  constraints_lowerbound[y_start] = y;
  constraints_lowerbound[psi_start] = psi;
  constraints_lowerbound[v_start] = v;
  constraints_lowerbound[cte_start] = cte;
  constraints_lowerbound[epsi_start] = epsi;

  constraints_upperbound[x_start] = x;
  constraints_upperbound[y_start] = y;
  constraints_upperbound[psi_start] = psi;
  constraints_upperbound[v_start] = v;
  constraints_upperbound[cte_start] = cte;
  constraints_upperbound[epsi_start] = epsi;

  // object that computes objective and constraints
  FG_eval fg_eval(coeffs);

  // options for IPOPT solver
  std::string options;
  // Uncomment this if you'd like more print information
  options += "Integer print_level  0\n";
  // NOTE: Setting sparse to true allows the solver to take advantage
  // of sparse routines, this makes the computation MUCH FASTER. If you
  // can uncomment 1 of these and see if it makes a difference or not but
  // if you uncomment both the computation time should go up in orders of
  // magnitude.
  options += "Sparse  true        forward\n";
  options += "Sparse  true        reverse\n";
  // NOTE: Currently the solver has a maximum time limit of 0.5 seconds.
  // Change this as you see fit.
  options += "Numeric max_cpu_time          0.5\n";

  // place to return solution
  CppAD::ipopt::solve_result<Dvector> solution;

  // solve the problem
  CppAD::ipopt::solve<Dvector, FG_eval>(
      options, vars, vars_lowerbound, vars_upperbound, constraints_lowerbound,
      constraints_upperbound, fg_eval, solution);

  // Check some of the solution values
  ok &= solution.status == CppAD::ipopt::solve_result<Dvector>::success;

  // Cost
  //auto cost = solution.obj_value;
  //std::cout << "Cost " << cost << std::endl;

  // Return the first actuator values. The variables can be accessed with
  // `solution.x[i]`.
  //
  // {...} is shorthand for creating a vector, so auto x1 = {1.0,2.0}
  // creates a 2 element double vector.
  //return {};

  vector<double> next_x_vals(solution.x.data()+x_start,solution.x.data()+y_start);
  vector<double> next_y_vals(solution.x.data()+y_start, solution.x.data()+psi_start);

  double dX = solution.x[x_start];
  double dY = solution.x[y_start];
  double dPsi = solution.x[psi_start];
  double dV = solution.x[v_start];
  double dCte = solution.x[cte_start];
  double dEpsi = solution.x[epsi_start];


  double dSteering = solution.x[delta_start];
  double dThrottle = solution.x[a_start];
  //std::cout << "dSteering " << dSteering << std::endl;
  //std::cout << "dThrottle " << dThrottle << std::endl;
  auto sol_output = CONFIG_OUTPUT(dX, dY, dPsi, dV, dCte, dEpsi, dSteering, dThrottle, next_x_vals, next_y_vals);

  //last_sol_ = solution;
  dvLast_con[0] = dSteering;
  dvLast_con[1] = dThrottle;

  return sol_output;
}