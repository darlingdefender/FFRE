#include "MagneticCollimationField.hh"

#include <algorithm>
#include <cmath>

namespace B1
{

MagneticCollimationField::MagneticCollimationField(
  MagneticCollimationFieldConfig config)
: fConfig(config)
{}

void MagneticCollimationField::GetFieldValue(const G4double point[4],
                                             G4double* field) const
{
  field[0] = 0.;
  field[1] = 0.;
  field[2] = 0.;

  const G4double x = point[0];
  const G4double y = point[1];
  const G4double z = point[2];

  const G4double radialDistance = std::hypot(x, y);
  if (radialDistance > fConfig.activeRadius ||
      z < fConfig.activeZMin ||
      z > fConfig.activeZMax) {
    return;
  }

  const G4double axialDistance = std::max(0., z);
  const G4double axialField =
    std::max(0., fConfig.baseField + fConfig.axialGradient*axialDistance);

  if (fConfig.fieldTowardOutlets) {
    const G4double splitRamp = (fConfig.splitAngle != 0. &&
                                fConfig.splitRampLength > 0.)
      ? std::clamp((z - fConfig.splitStartZ)/fConfig.splitRampLength, 0., 1.)
      : 1.;
    const G4double tilt = fConfig.splitAngle*splitRamp;
    const G4double branch = (x >= 0.) ? 1. : -1.;

    field[0] = branch*axialField*std::cos(tilt);
    field[1] = 0.;
    field[2] = axialField*std::sin(tilt);
  } else if (fConfig.fieldAlongY) {
    G4double bx = 0.;
    G4double by = axialField;
    G4double bz = 0.;

    if (fConfig.splitAngle != 0. && fConfig.splitRampLength > 0.) {
      const G4double splitRamp = std::clamp(
        (z - fConfig.splitStartZ)/fConfig.splitRampLength, 0., 1.);
      const G4double splitWeight = splitRamp*std::clamp(
        x/fConfig.splitXScale, -1., 1.);
      bx += by*std::tan(fConfig.splitAngle)*splitWeight;
    }

    field[0] = bx;
    field[1] = by;
    field[2] = bz;
  } else {
    // The paper uses an axial gradient field. The extra left/right flare used
    // here maps the paper's "degree" parameter onto the two toroidal outlets in
    // this Geant4 geometry so the right and left halves of the core are steered
    // toward Shape3 and Shape4, respectively.
    G4double bx = -0.5*fConfig.axialGradient*x;
    G4double by = -0.5*fConfig.axialGradient*y;
    G4double bz = axialField;

    if (fConfig.splitAngle != 0. && fConfig.splitRampLength > 0.) {
      const G4double splitRamp = std::clamp(
        (z - fConfig.splitStartZ)/fConfig.splitRampLength, 0., 1.);
      const G4double splitWeight = splitRamp*std::clamp(
        x/fConfig.splitXScale, -1., 1.);
      bx += bz*std::tan(fConfig.splitAngle)*splitWeight;
    }

    field[0] = bx;
    field[1] = by;
    field[2] = bz;
  }

}

}  // namespace B1
