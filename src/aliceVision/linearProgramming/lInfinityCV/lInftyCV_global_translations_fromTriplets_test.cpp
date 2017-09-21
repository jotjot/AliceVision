// This file is part of the AliceVision project and is made available under
// the terms of the MPL2 license (see the COPYING.md file).

#include "aliceVision/linearProgramming/ISolver.hpp"
#include "aliceVision/linearProgramming/OSIXSolver.hpp"
#include "aliceVision/linearProgramming/lInfinityCV/global_translations_fromTriplets.hpp"

#include "aliceVision/multiview/translationAveraging/translationAveragingTest.hpp"
#include "testing/testing.h"

using namespace aliceVision;
using namespace aliceVision::linearProgramming;
using namespace lInfinityCV;
using namespace std;

TEST(translation_averaging, globalTi_from_tijs_Triplets) {

  const int focal = 1000;
  const int principal_Point = 500;
  //-- Setup a circular camera rig or "cardiod".
  const int iNviews = 12;
  const int iNbPoints = 6;

  const bool bCardiod = true;
  const bool bRelative_Translation_PerTriplet = true;
  std::vector<aliceVision::translationAveraging::relativeInfo > vec_relative_estimates;

  const NViewDataSet d =
    Setup_RelativeTranslations_AndNviewDataset
    (
      vec_relative_estimates,
      focal, principal_Point, iNviews, iNbPoints,
      bCardiod, bRelative_Translation_PerTriplet
    );

  d.ExportToPLY("global_translations_from_triplets_GT.ply");
  visibleCamPosToSVGSurface(d._C, "global_translations_from_triplets_GT.svg");

  //-- Compute the global translations from the triplets of heading directions
  //-   with the L_infinity optimization

  std::vector<double> vec_solution(iNviews*3 + vec_relative_estimates.size()/3 + 1);
  double gamma = -1.0;

  //- a. Setup the LP solver,
  //- b. Setup the constraints generator (for the dedicated L_inf problem),
  //- c. Build constraints and solve the problem,
  //- d. Get back the estimated parameters.

  //- a. Setup the LP solver,
  OSI_CISolverWrapper solverLP(vec_solution.size());

  //- b. Setup the constraints generator (for the dedicated L_inf problem),
  Tifromtij_ConstraintBuilder_OneLambdaPerTrif cstBuilder(vec_relative_estimates);

  //- c. Build constraints and solve the problem (Setup constraints and solver)
  LPConstraintsSparse constraint;
  cstBuilder.Build(constraint);
  solverLP.setup(constraint);
  //-- Solving
  EXPECT_TRUE(solverLP.solve()); // the linear program must have a solution

  //- d. Get back the estimated parameters.
  solverLP.getSolution(vec_solution);
  gamma = vec_solution[vec_solution.size()-1];

  //--
  //-- Unit test checking about the found solution
  //--
  EXPECT_NEAR(0.0, gamma, 1e-6); // Gamma must be 0, no noise, perfect data have been sent

  ALICEVISION_LOG_DEBUG("Found solution with gamma = " << gamma);

  //-- Get back computed camera translations
  std::vector<double> vec_camTranslation(iNviews*3,0);
  std::copy(&vec_solution[0], &vec_solution[iNviews*3], &vec_camTranslation[0]);

  //-- Get back computed lambda factors
  std::vector<double> vec_camRelLambdas(&vec_solution[iNviews*3], &vec_solution[iNviews*3 + vec_relative_estimates.size()/3]);
  // lambda factors must be equal to 1.0 (no compression, no dilation);
  EXPECT_NEAR(vec_relative_estimates.size()/3, std::accumulate (vec_camRelLambdas.begin(), vec_camRelLambdas.end(), 0.0), 1e-6);

  // Get back the camera translations in the global frame:
  ALICEVISION_LOG_DEBUG(std::endl << "Camera centers (Computed): ");
  for (size_t i = 0; i < iNviews; ++i)
  {
    const Vec3 C_GT = d._C[i] - d._C[0]; //First camera supposed to be at Identity

    const Vec3 t(vec_camTranslation[i*3], vec_camTranslation[i*3+1], vec_camTranslation[i*3+2]);
    const Mat3 & Ri = d._R[i];
    const Vec3 C_computed = - Ri.transpose() * t;

    //-- Check that found camera position is equal to GT value
    EXPECT_NEAR(0.0, DistanceLInfinity(C_computed, C_GT), 1e-6);
  }
}

/* ************************************************************************* */
int main() { TestResult tr; return TestRegistry::runAllTests(tr);}
/* ************************************************************************* */
