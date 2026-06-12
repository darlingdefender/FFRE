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
/// \file PrimaryGeneratorAction.hh
/// \brief Definition of the B1::PrimaryGeneratorAction class

#ifndef B1PrimaryGeneratorAction_h
#define B1PrimaryGeneratorAction_h 1

#include "G4VUserPrimaryGeneratorAction.hh"
#include "G4GeneralParticleSource.hh"
#include "globals.hh"

class G4Event;
class G4ParticleDefinition;

/// The primary generator action class with general particle source (GPS).
///
/// The default source models a U-235 fission pair (heavy/light fragment).

namespace B1
{

class PrimaryGeneratorAction : public G4VUserPrimaryGeneratorAction
{
  public:
    PrimaryGeneratorAction();
    ~PrimaryGeneratorAction() override;

    // method from the base class
    void GeneratePrimaries(G4Event*) override;

    // method to access particle source
    const G4GeneralParticleSource* GetParticleSource() const { return fParticleSource; }
    G4int GetIonsPerBunch() const { return fFragmentsPerFission; }
    const G4String& GetConfiguredSourceSummary() const { return fConfiguredSourceSummary; }

  private:
    G4GeneralParticleSource* fParticleSource = nullptr;
    G4ParticleDefinition* fHeavyFragment = nullptr;
    G4ParticleDefinition* fLightFragment = nullptr;
    G4String fConfiguredSourceSummary = "U-235 fission fragments model";

    G4int fFragmentsPerFission = 2;
    G4double fFissionPointRadiusSigma = 1.2 * CLHEP::mm;
    G4double fFissionPointLengthSigma = 2.0 * CLHEP::mm;
    G4double fAngularSigma = 2.0 * CLHEP::deg;
    G4double fTimeSigma = 2.0 * CLHEP::ns;
    G4double fHeavyMeanEnergy = 70.0 * CLHEP::MeV;
    G4double fHeavyEnergySigma = 7.0 * CLHEP::MeV;
    G4double fLightMeanEnergy = 95.0 * CLHEP::MeV;
    G4double fLightEnergySigma = 9.5 * CLHEP::MeV;
    G4double fFragmentChargeState = 22.0 * CLHEP::eplus;
    G4double fSourceZ = -120.0 * CLHEP::cm;
};

}

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

#endif
