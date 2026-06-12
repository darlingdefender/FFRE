//
// ********************************************************************
// * License and Disclaimer                                           *
// *                                                                  *
// * The  Geant4 software  is  copyright of the Copyright Holders  of *
// * the Geant4 Collaboration.  It is provided  under  the terms  and *
// * conditions of the Geant4 Software License,  included in the file *
// * LICENSE and available at  http://cern.ch/geant4/license .  These *
// * include a list of copyright holders.                             *
// *                                                                  *
// * Neither the authors of this software system, nor their employing *
// * institutes,nor the agencies providing financial support for this *
// * work  make  any representation or  warranty, express or implied, *
// * regarding  this  software system or assume any liability for its *
// * use.  Please see the license in the file  LICENSE  and URL above *
// * for the full disclaimer and the limitation of liability.         *
// *                                                                  *
// * This  code  implementation is the result of  the  scientific and *
// * technical work of the GEANT4 collaboration.                      *
// * By using,  copying,  modifying or  distributing the software (or *
// * any work based  on the software)  you  agree  to acknowledge its *
// * use  in  resulting  scientific  publications,  and indicate your *
// * acceptance of all terms of the Geant4 Software license.          *
// ********************************************************************
//

#include "ThrusterMagneticField.hh"

#include "G4SystemOfUnits.hh"

#include <array>
#include <cmath>

namespace B1
{
namespace
{

struct FieldLobe
{
  G4double z0;
  G4double amplitude;
  G4double sigma;
};

constexpr std::array<FieldLobe, 3> kFieldLobes = {{
  { -150.0 * cm, 0.25 * tesla, 40.0 * cm },
  {  -90.0 * cm, 0.38 * tesla, 45.0 * cm },
  {   70.0 * cm, 0.18 * tesla, 70.0 * cm },
}};

constexpr G4double kGuideField = 0.04 * tesla;
constexpr G4double kInnerFieldRadius = 32.0 * cm;
constexpr G4double kOuterFieldRadius = 45.0 * cm;
constexpr G4double kInnerFieldHalfLength = 240.0 * cm;
constexpr G4double kOuterFieldHalfLength = 300.0 * cm;

G4double SmoothCutoff(const G4double value, const G4double inner, const G4double outer)
{
  if (value <= inner) {
    return 1.0;
  }

  if (value >= outer) {
    return 0.0;
  }

  const G4double t = (value - inner) / (outer - inner);
  return 1.0 - t * t * (3.0 - 2.0 * t);
}

G4double AxialFieldOnAxis(const G4double z)
{
  G4double field = kGuideField;

  for (const auto& lobe : kFieldLobes) {
    const G4double dz = z - lobe.z0;
    const G4double sigma2 = lobe.sigma * lobe.sigma;
    field += lobe.amplitude * std::exp(-0.5 * dz * dz / sigma2);
  }

  return field;
}

G4double AxialFieldFirstDerivative(const G4double z)
{
  G4double derivative = 0.0;

  for (const auto& lobe : kFieldLobes) {
    const G4double dz = z - lobe.z0;
    const G4double sigma2 = lobe.sigma * lobe.sigma;
    derivative += -lobe.amplitude * dz * std::exp(-0.5 * dz * dz / sigma2) / sigma2;
  }

  return derivative;
}

G4double AxialFieldSecondDerivative(const G4double z)
{
  G4double derivative = 0.0;

  for (const auto& lobe : kFieldLobes) {
    const G4double dz = z - lobe.z0;
    const G4double sigma2 = lobe.sigma * lobe.sigma;
    derivative += lobe.amplitude * std::exp(-0.5 * dz * dz / sigma2)
                * ((dz * dz) / (sigma2 * sigma2) - 1.0 / sigma2);
  }

  return derivative;
}

}  // namespace

ThrusterMagneticField::ThrusterMagneticField()
{}

ThrusterMagneticField::~ThrusterMagneticField()
{}

void ThrusterMagneticField::GetFieldValue(const G4double point[4], G4double* field) const
{
  const G4double x = point[0];
  const G4double y = point[1];
  const G4double z = point[2];
  const G4double r = std::sqrt(x * x + y * y);

  field[0] = 0.0;
  field[1] = 0.0;
  field[2] = 0.0;

  if (r >= kOuterFieldRadius || std::abs(z) >= kOuterFieldHalfLength) {
    return;
  }

  const G4double taper = SmoothCutoff(r, kInnerFieldRadius, kOuterFieldRadius)
                       * SmoothCutoff(std::abs(z), kInnerFieldHalfLength, kOuterFieldHalfLength);

  const G4double axialField = AxialFieldOnAxis(z);
  const G4double firstDerivative = AxialFieldFirstDerivative(z);
  const G4double secondDerivative = AxialFieldSecondDerivative(z);

  const G4double bz = taper * (axialField - 0.25 * r * r * secondDerivative);
  const G4double br = taper * (-0.5 * r * firstDerivative);

  if (r > 0.0) {
    field[0] = br * x / r;
    field[1] = br * y / r;
  }

  field[2] = bz;
}

}  // namespace B1
