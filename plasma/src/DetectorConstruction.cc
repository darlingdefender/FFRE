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
#include "ThrusterMagneticField.hh"

#include "G4NistManager.hh"
#include "G4Box.hh"
#include "G4Cons.hh"
#include "G4Element.hh"
#include "G4FieldManager.hh"
#include "G4LogicalVolume.hh"
#include "G4Material.hh"
#include "G4PVPlacement.hh"
#include "G4Sphere.hh"
#include "G4SystemOfUnits.hh"
#include "G4Tubs.hh"
#include "G4VisAttributes.hh"
#include "G4Colour.hh"

namespace B1
{

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

G4ThreadLocal ThrusterMagneticField* DetectorConstruction::fMagneticField = nullptr;
G4ThreadLocal G4FieldManager* DetectorConstruction::fFieldMgr = nullptr;

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

DetectorConstruction::DetectorConstruction()
{}

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

DetectorConstruction::~DetectorConstruction()
{}

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

G4VPhysicalVolume* DetectorConstruction::Construct()
{
  // Get nist material manager
  G4NistManager* nist = G4NistManager::Instance();

  // Option to switch on/off checking of volumes overlaps
  //
  G4bool checkOverlaps = true;

  // Materials
  //
  G4Material* worldMat = nist->FindOrBuildMaterial("G4_AIR");
  G4Material* vacuumMat = nist->FindOrBuildMaterial("G4_Galactic");
  G4Material* copperMat = nist->FindOrBuildMaterial("G4_Cu");
  G4Material* electrodeMat = nist->FindOrBuildMaterial("G4_Al");
  G4Material* beoMat = G4Material::GetMaterial("BeOModerator", false);
  if (!beoMat) {
    auto* be = nist->FindOrBuildElement("Be");
    auto* o = nist->FindOrBuildElement("O");
    beoMat = new G4Material("BeOModerator", 3.01 * g / cm3, 2);
    beoMat->AddElement(be, 1);
    beoMat->AddElement(o, 1);
  }

  G4Material* dustPlasmaMat = G4Material::GetMaterial("DustyPlasma", false);
  if (!dustPlasmaMat) {
    auto* u = nist->FindOrBuildElement("U");
    auto* o = nist->FindOrBuildElement("O");

    // Separate the solid grain density from the cloud-average density.
    // The papers use grain-density thermal estimates (~14.3 g/cm3) together
    // with a low-density active dust cloud (~1e-4 g/cm3). We model the
    // transport medium with the cloud-average density and keep the grain
    // density as an independent thermal-design parameter.
    fDustPackingFraction = fDustCloudAverageDensity / fFuelGrainDensity;
    dustPlasmaMat = new G4Material("DustyPlasma", fDustCloudAverageDensity, 2);
    dustPlasmaMat->AddElement(u, 1);
    dustPlasmaMat->AddElement(o, 2);
  }

  // Global dimensions
  //
  const G4double envelopeXY = 260. * cm;
  const G4double envelopeZ = 700. * cm;
  const G4double housingXY = 140. * cm;
  const G4double housingZ = 560. * cm;

  const G4double dischargeRadius = 30. * cm;
  const G4double dischargeHalfLength = 60. * cm;
  const G4double nozzleHalfLength = 20. * cm;
  const G4double driftRadius = 40. * cm;
  const G4double driftHalfLength = 100. * cm;

  //
  // World
  //
  auto* solidWorld = new G4Box("World", 0.7 * envelopeXY, 0.7 * envelopeXY, 0.7 * envelopeZ);
  auto* logicWorld = new G4LogicalVolume(solidWorld, worldMat, "World");
  auto* physWorld = new G4PVPlacement(nullptr, G4ThreeVector(), logicWorld, "World", nullptr,
                                      false, 0, checkOverlaps);

  //
  // Envelope kept as a simple vacuum container so the original example
  // infrastructure continues to work.
  //
  auto* solidEnv = new G4Box("Envelope", 0.5 * envelopeXY, 0.5 * envelopeXY, 0.5 * envelopeZ);
  auto* logicEnv = new G4LogicalVolume(solidEnv, vacuumMat, "Envelope");
  fFieldLogical = logicEnv;

  new G4PVPlacement(nullptr, G4ThreeVector(), logicEnv, "Envelope", logicWorld, false, 0,
                    checkOverlaps);

  //
  // BeO moderator / outer housing
  //
  auto* solidHousing =
    new G4Box("ModeratorHousing", 0.5 * housingXY, 0.5 * housingXY, 0.5 * housingZ);
  auto* logicHousing = new G4LogicalVolume(solidHousing, beoMat, "ModeratorHousing");

  new G4PVPlacement(nullptr, G4ThreeVector(), logicHousing, "ModeratorHousing", logicEnv, false, 0,
                    checkOverlaps);

  //
  // Internal thruster channel
  //
  auto* solidDischarge =
    new G4Tubs("DischargeChamber", 0., dischargeRadius, dischargeHalfLength, 0., 360. * deg);
  auto* logicDischarge = new G4LogicalVolume(solidDischarge, vacuumMat, "DischargeChamber");

  new G4PVPlacement(nullptr, G4ThreeVector(0., 0., -120. * cm), logicDischarge,
                    "DischargeChamber", logicHousing, false, 0, checkOverlaps);

  auto* solidNozzle = new G4Cons("ExpansionSection", 0., dischargeRadius, 0., driftRadius,
                                 nozzleHalfLength, 0., 360. * deg);
  auto* logicNozzle = new G4LogicalVolume(solidNozzle, vacuumMat, "ExpansionSection");

  new G4PVPlacement(nullptr, G4ThreeVector(0., 0., -40. * cm), logicNozzle,
                    "ExpansionSection", logicHousing, false, 0, checkOverlaps);

  auto* solidDrift =
    new G4Tubs("DriftSection", 0., driftRadius, driftHalfLength, 0., 360. * deg);
  auto* logicDrift = new G4LogicalVolume(solidDrift, vacuumMat, "DriftSection");

  new G4PVPlacement(nullptr, G4ThreeVector(0., 0., 80. * cm), logicDrift, "DriftSection",
                    logicHousing, false, 0, checkOverlaps);

  //
  // Dust cloud / dusty plasma core
  //
  // Literature-consistent transport cloud used in the escape-probability study:
  // 20 cm radius and 40 cm total thickness at 1e-4 g/cm3 average density.
  auto* solidDustCloud =
    new G4Tubs("DustCloud", 0., 20.0 * cm, 20.0 * cm, 0., 360. * deg);
  auto* logicDustCloud = new G4LogicalVolume(solidDustCloud, dustPlasmaMat, "DustCloud");
  fModeledDustMass = solidDustCloud->GetCubicVolume() * fDustCloudAverageDensity;

  new G4PVPlacement(nullptr, G4ThreeVector(), logicDustCloud, "DustCloud", logicDischarge, false, 0,
                    checkOverlaps);

  //
  // Magnetic containment coils and mirror section
  //
  auto* solidContainmentCoil =
    new G4Tubs("ContainmentCoil", 31.0 * cm, 35.0 * cm, 4.0 * cm, 0., 360. * deg);
  auto* logicContainmentCoil =
    new G4LogicalVolume(solidContainmentCoil, copperMat, "ContainmentCoil");

  new G4PVPlacement(nullptr, G4ThreeVector(0., 0., -150. * cm), logicContainmentCoil,
                    "ContainmentCoil", logicHousing, false, 0, checkOverlaps);
  new G4PVPlacement(nullptr, G4ThreeVector(0., 0., -95. * cm), logicContainmentCoil,
                    "ContainmentCoil", logicHousing, false, 1, checkOverlaps);

  //
  // RF induction coils wrapped around the discharge chamber
  //
  auto* solidRFCoil =
    new G4Tubs("RFInductionCoil", 36.0 * cm, 40.0 * cm, 3.5 * cm, 0., 360. * deg);
  auto* logicRFCoil = new G4LogicalVolume(solidRFCoil, copperMat, "RFInductionCoil");

  new G4PVPlacement(nullptr, G4ThreeVector(0., 0., -165. * cm), logicRFCoil,
                    "RFInductionCoil", logicHousing, false, 0, checkOverlaps);
  new G4PVPlacement(nullptr, G4ThreeVector(0., 0., -120. * cm), logicRFCoil,
                    "RFInductionCoil", logicHousing, false, 1, checkOverlaps);
  new G4PVPlacement(nullptr, G4ThreeVector(0., 0., -75. * cm), logicRFCoil,
                    "RFInductionCoil", logicHousing, false, 2, checkOverlaps);

  //
  // Electron separation electrodes near the throat
  //
  auto* solidElectronElectrode =
    new G4Tubs("ElectronSeparationElectrode", 33.0 * cm, 39.0 * cm, 1.5 * cm, 0., 360. * deg);
  auto* logicElectronElectrode =
    new G4LogicalVolume(solidElectronElectrode, electrodeMat, "ElectronSeparationElectrode");

  new G4PVPlacement(nullptr, G4ThreeVector(0., 0., -35. * cm), logicElectronElectrode,
                    "ElectronSeparationElectrode", logicDrift, false, 0, checkOverlaps);
  new G4PVPlacement(nullptr, G4ThreeVector(0., 0., 5. * cm), logicElectronElectrode,
                    "ElectronSeparationElectrode", logicDrift, false, 1, checkOverlaps);

  //
  // Deceleration and ion collection electrodes in the downstream channel
  //
  auto* solidCollectorElectrode =
    new G4Tubs("IonCollectionElectrode", 31.0 * cm, 39.5 * cm, 2.0 * cm, 0., 360. * deg);
  auto* logicCollectorElectrode =
    new G4LogicalVolume(solidCollectorElectrode, electrodeMat, "IonCollectionElectrode");

  new G4PVPlacement(nullptr, G4ThreeVector(0., 0., -55. * cm), logicCollectorElectrode,
                    "IonCollectionElectrode", logicDrift, false, 0, checkOverlaps);
  new G4PVPlacement(nullptr, G4ThreeVector(0., 0., -15. * cm), logicCollectorElectrode,
                    "IonCollectionElectrode", logicDrift, false, 1, checkOverlaps);
  new G4PVPlacement(nullptr, G4ThreeVector(0., 0., 25. * cm), logicCollectorElectrode,
                    "IonCollectionElectrode", logicDrift, false, 2, checkOverlaps);
  new G4PVPlacement(nullptr, G4ThreeVector(0., 0., 65. * cm), logicCollectorElectrode,
                    "IonCollectionElectrode", logicDrift, false, 3, checkOverlaps);

  // Set the dust cloud as scoring volume
  //
  fScoringVolume = logicDustCloud;

  // Visualization
  //
  logicWorld->SetVisAttributes(G4VisAttributes::GetInvisible());
  logicEnv->SetVisAttributes(G4VisAttributes::GetInvisible());

  auto* housingVis = new G4VisAttributes(G4Colour(0.82, 0.82, 0.84, 0.30));
  housingVis->SetForceSolid(true);
  logicHousing->SetVisAttributes(housingVis);

  auto* chamberVis = new G4VisAttributes(G4Colour(0.60, 0.66, 0.74, 0.08));
  chamberVis->SetForceWireframe(true);
  logicDischarge->SetVisAttributes(chamberVis);
  logicNozzle->SetVisAttributes(chamberVis);
  logicDrift->SetVisAttributes(chamberVis);

  auto* dustVis = new G4VisAttributes(G4Colour(0.15, 0.95, 0.20, 0.55));
  dustVis->SetForceSolid(true);
  logicDustCloud->SetVisAttributes(dustVis);

  auto* containmentVis = new G4VisAttributes(G4Colour(0.04, 0.18, 0.75));
  containmentVis->SetForceSolid(true);
  logicContainmentCoil->SetVisAttributes(containmentVis);

  auto* rfVis = new G4VisAttributes(G4Colour(0.05, 0.35, 0.95));
  rfVis->SetForceSolid(true);
  logicRFCoil->SetVisAttributes(rfVis);

  auto* electrodeVis = new G4VisAttributes(G4Colour(0.88, 0.88, 0.92));
  electrodeVis->SetForceSolid(true);
  logicElectronElectrode->SetVisAttributes(electrodeVis);
  logicCollectorElectrode->SetVisAttributes(electrodeVis);

  //
  //always return the physical World
  //
  return physWorld;
}

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

void DetectorConstruction::ConstructSDandField()
{
  if (!fFieldMgr) {
    fMagneticField = new ThrusterMagneticField();
    fFieldMgr = new G4FieldManager();
    fFieldMgr->SetDetectorField(fMagneticField);
    fFieldMgr->CreateChordFinder(fMagneticField);
  }

  if (fFieldLogical) {
    const G4bool forceToAllDaughters = true;
    fFieldLogical->SetFieldManager(fFieldMgr, forceToAllDaughters);
  }
}

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

}
