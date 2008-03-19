/* -------------------------------------------------------------------------- *
 *                      SimTK Core: SimTK Simbody(tm)                         *
 * -------------------------------------------------------------------------- *
 * This is part of the SimTK Core biosimulation toolkit originating from      *
 * Simbios, the NIH National Center for Physics-Based Simulation of           *
 * Biological Structures at Stanford, funded under the NIH Roadmap for        *
 * Medical Research, grant U54 GM072970. See https://simtk.org.               *
 *                                                                            *
 * Portions copyright (c) 2008 Stanford University and the Authors.           *
 * Authors: Peter Eastman                                                     *
 * Contributors:                                                              *
 *                                                                            *
 * Permission is hereby granted, free of charge, to any person obtaining a    *
 * copy of this software and associated documentation files (the "Software"), *
 * to deal in the Software without restriction, including without limitation  *
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,   *
 * and/or sell copies of the Software, and to permit persons to whom the      *
 * Software is furnished to do so, subject to the following conditions:       *
 *                                                                            *
 * The above copyright notice and this permission notice shall be included in *
 * all copies or substantial portions of the Software.                        *
 *                                                                            *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR *
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,   *
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL    *
 * THE AUTHORS, CONTRIBUTORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,    *
 * DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR      *
 * OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE  *
 * USE OR OTHER DEALINGS IN THE SOFTWARE.                                     *
 * -------------------------------------------------------------------------- */

#include "SimTKsimbody.h"

using namespace SimTK;
using namespace std;

const Real TOL = 1e-10;
const Real BOND_LENGTH = 0.5;

#define ASSERT(cond) {SimTK_ASSERT_ALWAYS(cond, "Assertion failed");}

template <class T>
void assertEqual(T val1, T val2) {
    ASSERT(abs(val1-val2) < TOL);
}

template <int N>
void assertEqual(Vec<N> val1, Vec<N> val2) {
    for (int i = 0; i < N; ++i)
        ASSERT(abs(val1[i]-val2[i]) < TOL);
}

template<>
void assertEqual(Vector val1, Vector val2) {
    ASSERT(val1.size() == val2.size());
    for (int i = 0; i < val1.size(); ++i)
        assertEqual(val1[i], val2[i]);
}

template<>
void assertEqual(SpatialVec val1, SpatialVec val2) {
    assertEqual(val1[0], val2[0]);
    assertEqual(val1[1], val2[1]);
}

template<>
void assertEqual(Transform val1, Transform val2) {
    assertEqual(val1.T(), val2.T());
    ASSERT(val1.R().isSameRotationToWithinAngle(val2.R(), TOL));
}

void compareReactionToConstraint(SpatialVec reactionForce, const Constraint& constraint, const State& state) {
    Vector_<SpatialVec> constraintForce(constraint.getNumConstrainedBodies());
    Vector mobilityForce(constraint.getNumConstrainedMobilizers());
    constraint.calcConstraintForcesFromMultipliers(state, constraint.getMultipliersAsVector(state), constraintForce, mobilityForce);
    assertEqual(reactionForce, -1*constraint.getAncestorMobilizedBody().getBodyRotation(state)*constraintForce[1]);
}

int main() {
    MultibodySystem system;
    SimbodyMatterSubsystem matter(system);
    GeneralForceSubsystem forces(system);
    Force::UniformGravity(forces, matter, Vec3(0, -9.8, 0));
    
    // Create two free joints (which should produce no reaction forces).
    
    Body::Rigid body = Body::Rigid(MassProperties(1.3, Vec3(0), Inertia(1.3)));
    MobilizedBody::Free f1(matter.updGround(), Transform(Vec3(0)), body, Transform(Vec3(BOND_LENGTH, 0, 0)));
    MobilizedBody::Free f2(f1, Transform(Vec3(0)), body, Transform(Vec3(BOND_LENGTH, 0, 0)));
    
    // Two ball joints, and two free joints constrained to act like ball joints.
    
    MobilizedBody::Free fb1(matter.updGround(), Transform(Vec3(0)), body, Transform(Vec3(BOND_LENGTH, 0, 0)));
    MobilizedBody::Free fb2(fb1, Transform(Vec3(0, 0, BOND_LENGTH)), body, Transform(Vec3(BOND_LENGTH, 0, 0)));
    Constraint::Ball fb1constraint(matter.updGround(), Vec3(0, 0, 0), fb1, Vec3(BOND_LENGTH, 0, 0));
    Constraint::Ball fb2constraint(fb1, Vec3(0, 0, BOND_LENGTH), fb2, Vec3(BOND_LENGTH, 0, 0));
    MobilizedBody::Ball b1(matter.updGround(), Transform(Vec3(0)), body, Transform(Vec3(BOND_LENGTH, 0, 0)));
    MobilizedBody::Ball b2(b1, Transform(Vec3(0, 0, BOND_LENGTH)), body, Transform(Vec3(BOND_LENGTH, 0, 0)));
    Force::ConstantTorque(forces, fb2, Vec3(0.1, 0.1, 1.0));
    Force::ConstantTorque(forces, b2, Vec3(0.1, 0.1, 1.0));
    
    // Two translation joints, and two free joints constrained to act like translation joints.

    MobilizedBody::Free ft1(matter.updGround(), Transform(Vec3(0)), body, Transform(Vec3(BOND_LENGTH, 0, 0)));
    MobilizedBody::Free ft2(ft1, Transform(Vec3(0)), body, Transform(Vec3(0, BOND_LENGTH, 0)));
    Constraint::ConstantOrientation ft1constraint(matter.updGround(), Rotation(0, Vec3(1)), ft1, Rotation(0, Vec3(1)));
    Constraint::ConstantOrientation ft2constraint(ft1, Rotation(0, Vec3(1)), ft2, Rotation(0, Vec3(1)));
    MobilizedBody::Translation t1(matter.updGround(), Transform(Vec3(0)), body, Transform(Vec3(BOND_LENGTH, 0, 0)));
    MobilizedBody::Translation t2(t1, Transform(Vec3(0)), body, Transform(Vec3(0, BOND_LENGTH, 0)));
    Force::ConstantTorque(forces, ft2, Vec3(0.1, 0.1, 1.0));
    Force::ConstantTorque(forces, t2, Vec3(0.1, 0.1, 1.0));
    
    // Create the state.
    
    system.realizeTopology();
    State state = system.getDefaultState();
    Random::Gaussian random;
    int nq = state.getNQ()/2;
    for (int i = 0; i < state.getNY(); ++i)
        state.updY()[i] = random.getValue();
    system.realize(state, Stage::Velocity);
    Transform b1transform = b1.getMobilizerTransform(state);
    Transform b2transform = b2.getMobilizerTransform(state);
    SpatialVec b1velocity = b1.getMobilizerVelocity(state);
    SpatialVec b2velocity = b2.getMobilizerVelocity(state);
    Transform t1transform = t1.getMobilizerTransform(state);
    Transform t2transform = t2.getMobilizerTransform(state);
    SpatialVec t1velocity = t1.MobilizedBody::getMobilizerVelocity(state);
    SpatialVec t2velocity = t2.MobilizedBody::getMobilizerVelocity(state);
    fb1.setQToFitTransform(state, b1transform);
    fb2.setQToFitTransform(state, b2transform);
    fb1.setUToFitVelocity(state, b1velocity);
    fb2.setUToFitVelocity(state, b2velocity);
    ft1.setQToFitTransform(state, t1transform);
    ft2.setQToFitTransform(state, t2transform);
    ft1.setUToFitVelocity(state, t1velocity);
    ft2.setUToFitVelocity(state, t2velocity);
    system.project(state, TOL, Vector(state.getNY(), 1.0), Vector(state.getNYErr(), 1.0), Vector());
    system.realize(state, Stage::Acceleration);
    
    // Make sure the free and constrained bodies really are identical.
    
    assertEqual(b1.getBodyTransform(state), fb1.getBodyTransform(state));
    assertEqual(b2.getBodyTransform(state), fb2.getBodyTransform(state));
    assertEqual(b1.getBodyVelocity(state), fb1.getBodyVelocity(state));
    assertEqual(b2.getBodyVelocity(state), fb2.getBodyVelocity(state));
    assertEqual(t1.getBodyTransform(state), ft1.getBodyTransform(state));
    assertEqual(t2.getBodyTransform(state), ft2.getBodyTransform(state));
    assertEqual(t1.getBodyVelocity(state), ft1.getBodyVelocity(state));
    assertEqual(t2.getBodyVelocity(state), ft2.getBodyVelocity(state));
    
    // Calculate the mobility reaction forces.

    Vector_<SpatialVec> reactionForce(matter.getNBodies());
    matter.calcMobilizerReactionForces(state, reactionForce);

    // Make sure all free bodies have no reaction force on them.
    
    assertEqual((reactionForce[f1.getMobilizedBodyIndex()]), SpatialVec(Vec3(0), Vec3(0)));
    assertEqual((reactionForce[f2.getMobilizedBodyIndex()]), SpatialVec(Vec3(0), Vec3(0)));
    assertEqual((reactionForce[fb1.getMobilizedBodyIndex()]), SpatialVec(Vec3(0), Vec3(0)));
    assertEqual((reactionForce[fb2.getMobilizedBodyIndex()]), SpatialVec(Vec3(0), Vec3(0)));
    assertEqual((reactionForce[ft1.getMobilizedBodyIndex()]), SpatialVec(Vec3(0), Vec3(0)));
    assertEqual((reactionForce[ft2.getMobilizedBodyIndex()]), SpatialVec(Vec3(0), Vec3(0)));
    
    // The reaction forces should match the corresponding constraint forces.
    
    compareReactionToConstraint(reactionForce[b1.getMobilizedBodyIndex()], fb1constraint, state);
    compareReactionToConstraint(reactionForce[b2.getMobilizedBodyIndex()], fb2constraint, state);
    compareReactionToConstraint(reactionForce[t1.getMobilizedBodyIndex()], ft1constraint, state);
    compareReactionToConstraint(reactionForce[t2.getMobilizedBodyIndex()], ft2constraint, state);

    std::cout << "Done" << std::endl;
}
