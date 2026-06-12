//
// ********************************************************************
// * License and Disclaimer                                           *
// *                                                                  *
// * The  Geant4 software  is  copyright of the Copyright Holders  of *
// * the Geant4 Collaboration.  It is provided  under  the terms  and *
// * conditions of the Geant4 Software License,  included in the file *
// * LICENSE and available at  http://cern.ch/geant4/license .  These *
// * include a list of copyright holders.                             *
// * Geant4 软件的版权归 Geant4 协作组织的版权所有者所有。
// * 本软件根据 Geant4 软件许可证的条款和条件提供，该许可证包含在 LICENSE 文件中，
// * 并可在 http://cern.ch/geant4/license 获取。其中包括版权所有者列表。
// *                                                                  *
// * Neither the authors of this software system, nor their employing *
// * institutes,nor the agencies providing financial support for this *
// * work  make  any representation or  warranty, express or implied, *
// * regarding  this  software system or assume any liability for its *
// * use.  Please see the license in the file  LICENSE  and URL above *
// * for the full disclaimer and the limitation of liability.         *
// * 本软件系统的作者、其雇佣机构以及为这项工作提供财务支持的机构，均不对本软件系统
// * 作出任何明示或暗示的陈述或保证，也不对其使用承担任何责任。请参阅上述 LICENSE 
// * 文件和URL 中的完整免责声明和责任限制。       
// *                                                                  
// * This  code  implementation is the result of  the  scientific and *
// * technical work of the GEANT4 collaboration.                      *
// * By using,  copying,  modifying or  distributing the software (or *
// * any work based  on the software)  you  agree  to acknowledge its *
// * use  in  resulting  scientific  publications,  and indicate your *
// * acceptance of all terms of the Geant4 Software license.          *
// * 此代码实现是 GEANT4 协作组织科学和技术工作的成果。通过使用、复制、修改或分发本
// * 软件（或任何基于本软件的作品），您同意在由此产生的科学出版物中承认其使用，并表明
// * 您接受 Geant4 软件许可证的所有条款。
// ********************************************************************
//
//
/// \file DetectorConstruction.cc
/// \brief Implementation of the B1::DetectorConstruction class

#include "DetectorConstruction.hh"
#include "MagneticCollimationField.hh"
#include "G4Isotope.hh"
#include "G4Element.hh"
#include "G4Material.hh"
#include "G4SystemOfUnits.hh"
#include "G4RunManager.hh"
#include "G4NistManager.hh"
#include "G4Box.hh"
#include "G4Cons.hh"
#include "G4Orb.hh"
#include "G4Sphere.hh"
#include "G4Trd.hh"
#include "G4LogicalVolume.hh"
#include "G4PVPlacement.hh"
#include "G4SystemOfUnits.hh"
#include "G4Tubs.hh"
#include "G4Torus.hh"
#include "G4Region.hh"
#include "G4ProductionCuts.hh"
#include "G4FieldManager.hh"
#include "G4TransportationManager.hh"
#include "G4VisAttributes.hh"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <string>

namespace B1
{

namespace
{

enum class MagneticFieldMode
{
  kOff,
  kUniform,
  kCollimation,
};

G4bool ReadEnvFlag(const char* name, G4bool defaultValue)
{
  const char* value = std::getenv(name);
  if (!value) {
    return defaultValue;
  }

  const G4String text(value);
  if (text == "0" || text == "false" || text == "FALSE" ||
      text == "off" || text == "OFF" ||
      text == "no" || text == "NO") {
    return false;
  }

  return true;
}

G4double ReadEnvDouble(const char* name, G4double defaultValue)
{
  const char* value = std::getenv(name);
  if (!value) {
    return defaultValue;
  }

  char* end = nullptr;
  const double parsedValue = std::strtod(value, &end);
  if (end == value) {
    return defaultValue;
  }

  return parsedValue;
}

G4bool SingleFuelParticleViewEnabled()
{
  return ReadEnvFlag("FFRE_SINGLE_FUEL_PARTICLE_VIEW", false);
}

G4bool GeometryVisualizationViewEnabled()
{
  return ReadEnvFlag("FFRE_GEOMETRY_VIS_VIEW", false);
}

G4double FuelLayoutMarkerScale()
{
  G4double scale = ReadEnvDouble("FFRE_FUEL_LAYOUT_MARKER_SCALE", 1.);
  if (scale < 1.) {
    scale = 1.;
  }
  return scale;
}

MagneticFieldMode ReadFieldMode()
{
  if (SingleFuelParticleViewEnabled() || GeometryVisualizationViewEnabled()) {
    return MagneticFieldMode::kOff;
  }

  if (!ReadEnvFlag("FFRE_ENABLE_COLLIMATION_FIELD", true)) {
    return MagneticFieldMode::kOff;
  }

  const char* value = std::getenv("FFRE_FIELD_MODE");
  if (!value || *value == '\0') {
    return MagneticFieldMode::kCollimation;
  }

  std::string mode(value);
  std::transform(mode.begin(), mode.end(), mode.begin(),
                 [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

  if (mode == "off" || mode == "none") {
    return MagneticFieldMode::kOff;
  }
  if (mode == "uniform") {
    return MagneticFieldMode::kUniform;
  }

  return MagneticFieldMode::kCollimation;
}

}  // namespace

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

DetectorConstruction::DetectorConstruction()
{}

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

DetectorConstruction::~DetectorConstruction()
{}

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

G4VPhysicalVolume* DetectorConstruction::Construct()
{
  fFuelShellCenters.clear();
  fBoundaryFuelShellCenters.clear();
  fFuelShellInnerRadius = 0.;
  fFuelShellOuterRadius = 0.;

  const G4bool singleFuelParticleView = SingleFuelParticleViewEnabled();

  // Get nist material manager
  G4NistManager* nist = G4NistManager::Instance();
  // Fuel shell material uses the first metastable state of Am-242.
  G4Element* elO = nist->FindOrBuildElement("O");

  const G4double am242mMass = 242.059600689*g/mole;
  G4Isotope* Am242m =
    new G4Isotope("Am242m1", 95, 242, am242mMass, 1);

  G4Element* metallicAm = new G4Element("Americium242m1", "Am", 1);
  metallicAm->AddIsotope(Am242m, 100.0*perCent);

  G4double density = 13.6*g/cm3;
  G4Material* fuel_mat = new G4Material("Am242mMetal", density, 1);
  fuel_mat->AddElement(metallicAm, 1);
  auto moderator_mat = nist->FindOrBuildMaterial("G4_GRAPHITE");        // graphite
  G4Element* elBe = nist->FindOrBuildElement("Be");
  G4Material* beO_mat = new G4Material("BeO", 3.01*g/cm3, 2);
  beO_mat->AddElement(elBe, 1);
  beO_mat->AddElement(elO, 1);
  auto pipe_mat      = nist->FindOrBuildMaterial("G4_STAINLESS-STEEL"); // pipe
  // Envelope parameters
  //
  G4double env_sizeXY =
    singleFuelParticleView ? 20.*mm : 30.*m;
  G4double env_sizeZ =
    singleFuelParticleView ? 20.*mm : 35.*m;
  G4Material* env_mat = nist->FindOrBuildMaterial("G4_Galactic"); // vacuum

  // Option to switch on/off checking of volumes overlaps
  //
  // Keep overlap checks off in normal interactive runs. With thousands of
  // repeated coated particles, enabling this noticeably slows startup and Qt
  // visualization. Re-enable temporarily when validating geometry changes.
  G4bool checkOverlaps = false;

  //
  // World
  //
  G4double world_sizeXY = 1.2*env_sizeXY;
  G4double world_sizeZ  = 1.2*env_sizeZ;
  G4Material* world_mat = nist->FindOrBuildMaterial("G4_Galactic"); // vacuum

  G4Box* solidWorld =
    new G4Box("World",                       //its name
       0.5*world_sizeXY, 0.5*world_sizeXY, 0.5*world_sizeZ);     //its size

  G4LogicalVolume* logicWorld =
    new G4LogicalVolume(solidWorld,          //its solid
                        world_mat,           //its material
                        "World");            //its name

  G4VPhysicalVolume* physWorld =
    new G4PVPlacement(0,                     //no rotation
                      G4ThreeVector(),       //at (0,0,0)
                      logicWorld,            //its logical volume
                      "World",               //its name
                      0,                     //its mother  volume
                      false,                 //no boolean operation
                      0,                     //copy number
                      checkOverlaps);        //overlaps checking

  //
  // Envelope
  //
  G4Box* solidEnv =
    new G4Box("Envelope",                    //its name
        0.5*env_sizeXY, 0.5*env_sizeXY, 0.5*env_sizeZ); //its size

  G4LogicalVolume* logicEnv =
    new G4LogicalVolume(solidEnv,            //its solid
                        env_mat,             //its material
                        "Envelope");         //its name

  new G4PVPlacement(0,                       //no rotation
                    G4ThreeVector(),         //at (0,0,0)
                    logicEnv,                //its logical volume
                    "Envelope",              //its name
                    logicWorld,              //its mother  volume
                    false,                   //no boolean operation
                    0,                       //copy number
                    checkOverlaps);          //overlaps checking

  //
  // Shape 1
  //
  G4Material* shape1_mat = env_mat;
  G4ThreeVector pos1 = G4ThreeVector(0*m, 0*m, 0*m);

  // Shape1 is a cylinder 3 m long and 1 m in radius.
  G4double shape1Radius =
    singleFuelParticleView ? 4.5*mm : 1.0*m;
  G4double shape1HalfLength =
    singleFuelParticleView ? 4.5*mm : 1.5*m;
  G4Tubs* solidShape1 =
    new G4Tubs("Shape1", 0.*m, shape1Radius, shape1HalfLength,
               0.*deg, 360.*deg);

  G4LogicalVolume* logicShape1 =
    new G4LogicalVolume(solidShape1,         //its solid
                        shape1_mat,          //its material
                        "Shape1");           //its name
  auto rot1 = new G4RotationMatrix();
  rot1->rotateY(90*deg);

auto fuelRegion = new G4Region("FuelRegion");
fuelRegion->AddRootLogicalVolume(logicShape1);

auto cuts = new G4ProductionCuts();
// Coarser cuts are sufficient for fragment-counting studies and reduce CPU load.
cuts->SetProductionCut(1*mm, "proton");
cuts->SetProductionCut(1*mm, "e-");
cuts->SetProductionCut(1*mm, "gamma");
fuelRegion->SetProductionCuts(cuts);

  // Literature-inspired coated particle:
  // - Zhang et al. (2024) model a graphite inner layer with an Am-242m outer
  //   coating for the pelletized direct-propulsion concept;
  // - Ronen and Shwageraus (2000) emphasize a metallic ultra-thin Am-242m fuel
  //   layer combined with strong BeO moderation.
  //
  // Preserve the validated pellet-array scale used elsewhere in this model, but
  // retain the paper-consistent treatment of Am-242m as a metallic micron-scale
  // coating on a moderator carrier.
  G4double kernelRadius = 2.0*mm;
  G4double fuelShellThickness = 1.0*um;
  G4double particleOuterRadius = kernelRadius + fuelShellThickness;
  fFuelShellInnerRadius = kernelRadius;
  fFuelShellOuterRadius = particleOuterRadius;
  const G4double fuelLayoutMarkerScale =
    singleFuelParticleView ? 1. : FuelLayoutMarkerScale();

  G4Sphere* solidFuelShell =
    new G4Sphere("FuelShell", kernelRadius, particleOuterRadius,
                 0*deg, 360*deg, 0*deg, 180*deg);
  G4LogicalVolume* logicFuelShell =
    new G4LogicalVolume(solidFuelShell, fuel_mat, "FuelShell");

  G4Orb* solidKernel = new G4Orb("ModeratorKernel", kernelRadius);
  G4LogicalVolume* logicKernel =
    new G4LogicalVolume(solidKernel, moderator_mat, "ModeratorKernel");

  new G4PVPlacement(nullptr, G4ThreeVector(), logicKernel, "ModeratorKernel",
                    logicFuelShell, false, 0, checkOverlaps);

  G4LogicalVolume* logicFuelLayoutMarker = nullptr;
  if (fuelLayoutMarkerScale > 1.) {
    const G4double markerRadius = fuelLayoutMarkerScale*particleOuterRadius;
    G4Orb* solidFuelLayoutMarker =
      new G4Orb("FuelLayoutMarker", markerRadius);
    logicFuelLayoutMarker =
      new G4LogicalVolume(solidFuelLayoutMarker, env_mat, "FuelLayoutMarker");
    auto* markerVis =
      new G4VisAttributes(G4Colour(0.95, 0.22, 0.04, 0.95));
    markerVis->SetForceSolid(true);
    logicFuelLayoutMarker->SetVisAttributes(markerVis);

    G4cout
      << "[Visualization] Fuel layout marker scale = "
      << fuelLayoutMarkerScale
      << " (marker radius = " << markerRadius/mm
      << " mm). Use for geometry inspection only, not transport runs."
      << G4endl;
  }

  auto placeFuelLayoutMarker =
    [logicFuelLayoutMarker, logicShape1](const G4ThreeVector& particlePos,
                                         G4int copyNo)
    {
      if (!logicFuelLayoutMarker) {
        return;
      }
      new G4PVPlacement(nullptr, particlePos, logicFuelLayoutMarker,
                        "FuelLayoutMarker", logicShape1, false, copyNo, false);
    };

  if (singleFuelParticleView) {
    const G4ThreeVector particlePos;
    const G4ThreeVector particleWorldPos = rot1->inverse()*particlePos + pos1;

    new G4PVPlacement(nullptr, particlePos, logicFuelShell, "FuelShell",
                      logicShape1, false, 0, false);
    placeFuelLayoutMarker(particlePos, 0);
    fFuelShellCenters.push_back(particleWorldPos);
    fBoundaryFuelShellCenters.push_back(particleWorldPos);
  } else {
    // Paper-inspired staggered pellet array:
    // - the active fuel zone remains a 1 m radius, 3 m long cylinder;
    // - the fuel balls are arranged throughout the active zone instead of only
    //   on the outer surface;
    // - the staggered layout follows the article's optimized multiple-lattice
    //   study qualitatively, using numx = 25 and numy = numz = 10.
    constexpr G4int paperAxialBallCount = 25;
    constexpr G4int paperRadialBallCountY = 10;
    constexpr G4int paperRadialBallCountX = 10;

    const G4double latticePitchX =
      2.*(shape1Radius - particleOuterRadius)/(paperRadialBallCountX - 1);
    const G4double latticePitchY =
      2.*(shape1Radius - particleOuterRadius)/(paperRadialBallCountY - 1);
    const G4double latticePitchZ =
      2.*(shape1HalfLength - particleOuterRadius)/(paperAxialBallCount - 1);

    const G4double xStart = -0.5*(paperRadialBallCountX - 1)*latticePitchX;
    const G4double yStart = -0.5*(paperRadialBallCountY - 1)*latticePitchY;
    const G4double zStart = -0.5*(paperAxialBallCount - 1)*latticePitchZ;
    const G4double radialBoundaryBand =
      0.55*std::min(latticePitchX, latticePitchY);
    const G4double axialBoundaryBand = 0.55*latticePitchZ;

    G4int particleCopyNo = 0;
    for (G4int iz = 0; iz < paperAxialBallCount; ++iz) {
      const G4double z = zStart + iz*latticePitchZ;
      const G4double yLayerOffset = (iz % 2 == 0) ? 0. : 0.5*latticePitchY;

      for (G4int iy = 0; iy < paperRadialBallCountY; ++iy) {
        const G4double y = yStart + iy*latticePitchY + yLayerOffset;
        const G4double xRowOffset =
          ((iy + iz) % 2 == 0) ? 0. : 0.5*latticePitchX;

        for (G4int ix = 0; ix < paperRadialBallCountX; ++ix) {
          const G4double x = xStart + ix*latticePitchX + xRowOffset;
          const G4double radialDistance = std::sqrt(x*x + y*y);
          if (radialDistance + particleOuterRadius > shape1Radius) {
            continue;
          }

          G4ThreeVector particlePos(x, y, z);
          G4ThreeVector particleWorldPos = rot1->inverse()*particlePos + pos1;

          const G4int currentCopyNo = particleCopyNo++;
          new G4PVPlacement(nullptr, particlePos, logicFuelShell, "FuelShell",
                            logicShape1, false, currentCopyNo, false);
          placeFuelLayoutMarker(particlePos, currentCopyNo);
          fFuelShellCenters.push_back(particleWorldPos);

          const G4bool nearRadialBoundary =
            (shape1Radius - radialDistance) <= radialBoundaryBand;
          const G4bool nearAxialBoundary =
            (shape1HalfLength - std::abs(z)) <= axialBoundaryBand;
          if (nearRadialBoundary || nearAxialBoundary) {
            fBoundaryFuelShellCenters.push_back(particleWorldPos);
          }
        }
      }
    }
  }

new G4PVPlacement(rot1,                     //rotate Shape1 around y
                  pos1,                    //at position
                  logicShape1,             //its logical volume
                  "Shape1",                //its name
                  logicEnv,                //its mother  volume
                  false,                   //no boolean operation
                  0,                       //copy number
                  checkOverlaps);          //overlaps checking
  // Set Shape1 as scoring volume
  //
  fScoringVolume = logicShape1;

  if (!singleFuelParticleView) {
    //
    // Shape 2
    //
    G4Material* shape2_mat = beO_mat;
    G4ThreeVector pos2 = G4ThreeVector(0*m, 0*m, 0*m);

    // Shape2 is a cylindrical moderator shell around Shape1.
    G4double shape2InnerRadius = 1.0*m;
    G4double shape2OuterRadius = 2.0*m;
    G4double shape2HalfLength = 1.5*m;
    G4Tubs* solidShape2 =
      new G4Tubs("Shape2", shape2InnerRadius, shape2OuterRadius,
                 shape2HalfLength, 0.*deg, 360.*deg);

    G4LogicalVolume* logicShape2 =
      new G4LogicalVolume(solidShape2,         //its solid
                          shape2_mat,          //its material
                          "Shape2");           //its name
    auto rot2 = new G4RotationMatrix();
    rot2->rotateY(90*deg);
    new G4PVPlacement(rot2,                     //rotate Shape2 around y
                      pos2,                    //at position
                      logicShape2,             //its logical volume
                      "Shape2",                //its name
                      logicEnv,                //its mother  volume
                      false,                   //no boolean operation
                      0,                       //copy number
                      checkOverlaps);          //overlaps checking

    //
    // Shape 3
    //
    G4Material* shape3_mat = beO_mat;
    G4ThreeVector pos3 = G4ThreeVector(1.5*m, 0*m, 2.8*m);

    // Shape3 is a 90-degree torus segment.
    G4double pR3min = 1.0*m;
    G4double pR3max = 1.9*m;
    G4double pR3tor = 2.8*m;
    G4double pS3Phi = 0*deg;
    G4double pD3Phi = 90*deg;
    G4Torus* solidShape3 =
      new G4Torus("Shape3", pR3min,pR3max, pR3tor, pS3Phi,pD3Phi);

    G4LogicalVolume* logicShape3 =
      new G4LogicalVolume(solidShape3,         //its solid
                          shape3_mat,          //its material
                          "Shape3");           //its name
    auto rot3 = new G4RotationMatrix();
    rot3->rotateX(90*deg);
    new G4PVPlacement(rot3,                     //no rotation
                      pos3,                    //at position
                      logicShape3,             //its logical volume
                      "Shape3",                //its name
                      logicEnv,                //its mother  volume
                      false,                   //no boolean operation
                      0,                       //copy number
                      checkOverlaps);          //overlaps checking

    // Shape 4
    //
    G4Material* shape4_mat = beO_mat;
    G4ThreeVector pos4 = G4ThreeVector(-1.5*m, 0*m, 2.8*m);

    // Shape4 is the neighboring 90-degree torus segment.
    G4double pR4min = 1.0*m;
    G4double pR4max = 1.9*m;
    G4double pR4tor = 2.8*m;
    G4double pS4Phi = 90*deg;
    G4double pD4Phi = 90*deg;
    G4Torus* solidShape4 =
      new G4Torus("Shape4", pR4min,pR4max, pR4tor, pS4Phi,pD4Phi);

    G4LogicalVolume* logicShape4 =
      new G4LogicalVolume(solidShape4,         //its solid
                          shape4_mat,          //its material
                          "Shape4");           //its name
    auto rot4 = new G4RotationMatrix();
    rot4->rotateX(90*deg);
    new G4PVPlacement(rot4,                     //no rotation
                      pos4,                    //at position
                      logicShape4,             //its logical volume
                      "Shape4",                //its name
                      logicEnv,                //its mother  volume
                      false,                   //no boolean operation
                      0,                       //copy number
                      checkOverlaps);          //overlaps checking
  }

  //
  //always return the physical World
  //
  return physWorld;
}

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

void DetectorConstruction::ConstructSDandField()
{
  const MagneticFieldMode fieldMode = ReadFieldMode();
  auto* globalFieldManager =
    G4TransportationManager::GetTransportationManager()->GetFieldManager();

  if (fieldMode != MagneticFieldMode::kOff) {
    MagneticCollimationFieldConfig fieldConfig;
    fieldConfig.baseField = ReadEnvDouble("FFRE_B0_T", 1.5)*tesla;
    fieldConfig.splitXScale =
      ReadEnvDouble("FFRE_SPLIT_X_SCALE_M", 1.5)*m;
    fieldConfig.activeRadius =
      ReadEnvDouble("FFRE_FIELD_ACTIVE_RADIUS_M", 6.5)*m;
    fieldConfig.activeZMin =
      ReadEnvDouble("FFRE_FIELD_ACTIVE_Z_MIN_M", -1.2)*m;
    fieldConfig.activeZMax =
      ReadEnvDouble("FFRE_FIELD_ACTIVE_Z_MAX_M", 7.8)*m;

    if (fieldMode == MagneticFieldMode::kUniform) {
      fieldConfig.axialGradient = 0.;
      fieldConfig.splitAngle = 0.;
      fieldConfig.splitStartZ = 0.;
      fieldConfig.splitRampLength = 0.;
      fieldConfig.fieldAlongY = true;
      fieldConfig.fieldTowardOutlets = false;
    } else {
      fieldConfig.axialGradient =
        ReadEnvDouble("FFRE_GRADB_T_PER_M", -0.04)*tesla/m;
      fieldConfig.splitAngle =
        ReadEnvDouble("FFRE_SPLIT_ANGLE_DEG", 10.)*deg;
      fieldConfig.splitStartZ =
        ReadEnvDouble("FFRE_SPLIT_START_Z_M", -1.2)*m;
      fieldConfig.splitRampLength =
        ReadEnvDouble("FFRE_SPLIT_RAMP_Z_M", 0.2)*m;
      fieldConfig.fieldAlongY = false;
      fieldConfig.fieldTowardOutlets = true;
    }

    auto* magneticField = new MagneticCollimationField(fieldConfig);
    globalFieldManager->SetDetectorField(magneticField);
    globalFieldManager->CreateChordFinder(magneticField);

    G4cout
      << "[MagneticField] mode = "
      << (fieldMode == MagneticFieldMode::kUniform ? "uniform" : "collimation")
      << ": "
      << "B0 = " << fieldConfig.baseField/tesla << " T, gradB = "
      << fieldConfig.axialGradient/(tesla/m) << " T/m, split angle = "
      << fieldConfig.splitAngle/deg << " deg, axis = "
      << (fieldConfig.fieldTowardOutlets ? "Outlets" :
          (fieldConfig.fieldAlongY ? "Y" : "Z"))
      << G4endl;
  } else {
    globalFieldManager->SetDetectorField(nullptr);
    G4cout << "[MagneticField] mode = off" << G4endl;
  }
}

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

}
