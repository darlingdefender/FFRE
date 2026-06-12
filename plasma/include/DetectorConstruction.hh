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
//
/// \file DetectorConstruction.hh
/// \brief Definition of the B1::DetectorConstruction class

#ifndef B1DetectorConstruction_h
#define B1DetectorConstruction_h 1

#include "G4VUserDetectorConstruction.hh"
#include "G4SystemOfUnits.hh"
#include "globals.hh"

class G4VPhysicalVolume;
class G4LogicalVolume;
class G4FieldManager;

/// Detector construction class to define materials and geometry.

namespace B1
{

class ThrusterMagneticField;

class DetectorConstruction : public G4VUserDetectorConstruction
{
  public:
    DetectorConstruction();
    ~DetectorConstruction() override;

    G4VPhysicalVolume* Construct() override;
    void ConstructSDandField() override;

    G4LogicalVolume* GetScoringVolume() const { return fScoringVolume; }
    G4double GetReferenceDustTemperature() const { return fReferenceDustTemperature; }
    G4double GetTargetDustTemperature() const { return fTargetDustTemperature; }
    G4double GetDustEffectiveHeatCapacity() const { return fDustEffectiveHeatCapacity; }
    G4double GetFissionEnergyRelease() const { return fFissionEnergyRelease; }
    G4double GetFissionFragmentEnergyFraction() const { return fFissionFragmentEnergyFraction; }
    G4double GetReferenceOperationDuration() const { return fReferenceOperationDuration; }
    G4double GetReferenceCriticalFissileMass() const { return fReferenceCriticalFissileMass; }
    G4double GetReferenceThermalDesignFuelMass() const { return fReferenceThermalDesignFuelMass; }
    G4double GetModeledDustMass() const { return fModeledDustMass; }
    G4double GetDustPackingFraction() const { return fDustPackingFraction; }
    G4double GetDustGrainRadius() const { return fDustGrainRadius; }
    G4double GetDustBulkDensity() const { return fDustBulkDensity; }
    G4double GetDustCloudAverageDensity() const { return fDustCloudAverageDensity; }
    G4double GetFuelGrainDensity() const { return fFuelGrainDensity; }
    G4double GetFrictionFraction() const { return fFrictionFraction; }
    G4double GetReferenceThermalPower() const { return fReferenceThermalPower; }

  protected:
    static G4ThreadLocal ThrusterMagneticField* fMagneticField;
    static G4ThreadLocal G4FieldManager* fFieldMgr;

    G4LogicalVolume* fScoringVolume = nullptr;
    G4LogicalVolume* fFieldLogical = nullptr;
    G4double fReferenceDustTemperature = 2800. * kelvin;
    G4double fTargetDustTemperature = 3080. * kelvin;
    G4double fDustEffectiveHeatCapacity = 300.0 * joule / (kg * kelvin);
    G4double fFissionEnergyRelease = 207.0 * MeV;
    G4double fFissionFragmentEnergyFraction = 0.81;
    G4double fReferenceOperationDuration = 1.0 * microsecond;
    G4double fReferenceCriticalFissileMass = 11.0 * kg;
    G4double fReferenceThermalDesignFuelMass = 15.0 * kg;
    G4double fModeledDustMass = 0.;
    G4double fDustPackingFraction = 0.0001868;
    G4double fDustGrainRadius = 1000.0 * nanometer;
    G4double fFuelGrainDensity = 14300.0 * kg / m3;
    G4double fDustBulkDensity = 14300.0 * kg / m3;
    G4double fDustCloudAverageDensity = 1.0e-4 * g / cm3;
    G4double fFrictionFraction = 0.60;
    G4double fReferenceThermalPower = 350.0e6 * watt;
};

}

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

#endif
