#ifndef B1MagneticCollimationField_h
#define B1MagneticCollimationField_h 1

#include "G4MagneticField.hh"
#include "globals.hh"

namespace B1
{

struct MagneticCollimationFieldConfig
{
  G4double baseField = 0.;
  G4double axialGradient = 0.;
  G4double splitAngle = 0.;
  G4double splitStartZ = 0.;
  G4double splitRampLength = 1.;
  G4double splitXScale = 1.;
  G4double activeRadius = 0.;
  G4double activeZMin = 0.;
  G4double activeZMax = 0.;
  G4bool fieldAlongY = false;
  G4bool fieldTowardOutlets = false;
};

class MagneticCollimationField final : public G4MagneticField
{
  public:
    explicit MagneticCollimationField(MagneticCollimationFieldConfig config);
    void GetFieldValue(const G4double point[4], G4double* field) const override;

  private:
    MagneticCollimationFieldConfig fConfig;
};

}  // namespace B1

#endif
