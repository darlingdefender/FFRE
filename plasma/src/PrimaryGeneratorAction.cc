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
/// \file PrimaryGeneratorAction.cc
/// \brief Implementation of the B1::PrimaryGeneratorAction class

#include "PrimaryGeneratorAction.hh"

#include "G4Event.hh"
#include "G4Geantino.hh"
#include "G4GeneralParticleSource.hh"
#include "G4IonTable.hh"
#include "G4ParticleTable.hh"
#include "G4ParticleDefinition.hh"
#include "G4SystemOfUnits.hh"
#include "Randomize.hh"

#include <algorithm>
#include <cmath>

namespace B1
{

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

PrimaryGeneratorAction::PrimaryGeneratorAction()
{
  fParticleSource = new G4GeneralParticleSource();
  auto* source = fParticleSource->GetCurrentSource();

  // Use a placeholder particle until the ion table is fully initialized.
  source->SetParticleDefinition(G4Geantino::Geantino());
  source->SetParticleCharge(0.0);
  source->SetParticleTime(0.0);
  source->SetNumberOfParticles(1);

  source->GetPosDist()->SetPosDisType("Point");
  source->GetPosDist()->SetCentreCoords(G4ThreeVector(0., 0., fSourceZ));
  source->GetAngDist()->SetParticleMomentumDirection(G4ThreeVector(0., 0., 1.));
  source->GetEneDist()->SetEnergyDisType("Mono");
  source->GetEneDist()->SetMonoEnergy(fHeavyMeanEnergy);
}

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

PrimaryGeneratorAction::~PrimaryGeneratorAction()
{
  delete fParticleSource;
}

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

void PrimaryGeneratorAction::GeneratePrimaries(G4Event* anEvent)
{
  auto* source = fParticleSource->GetCurrentSource();
  source->SetNumberOfParticles(1);
  source->GetPosDist()->SetPosDisType("Point");
  source->GetEneDist()->SetEnergyDisType("Mono");

  if (!fHeavyFragment) {
    fHeavyFragment = G4ParticleTable::GetParticleTable()->GetIonTable()->GetIon(56, 140, 0.0);
  }
  if (!fLightFragment) {
    fLightFragment = G4ParticleTable::GetParticleTable()->GetIonTable()->GetIon(36, 95, 0.0);
  }

  if (!fHeavyFragment || !fLightFragment) {
    source->SetParticleDefinition(G4Geantino::Geantino());
    source->SetParticleCharge(0.0);
    source->SetParticleTime(0.0);
    source->GetPosDist()->SetCentreCoords(G4ThreeVector(0., 0., fSourceZ));
    source->GetAngDist()->SetParticleMomentumDirection(G4ThreeVector(0., 0., 1.));
    source->GetEneDist()->SetMonoEnergy(1. * keV);
    fParticleSource->GeneratePrimaryVertex(anEvent);
    return;
  }

  const G4double cosTheta = 2.0 * G4UniformRand() - 1.0;
  const G4double sinTheta = std::sqrt(std::max(0.0, 1.0 - cosTheta * cosTheta));
  const G4double phi = CLHEP::twopi * G4UniformRand();
  G4ThreeVector emissionAxis(
    sinTheta * std::cos(phi), sinTheta * std::sin(phi), cosTheta);
  emissionAxis = emissionAxis.unit();

  const G4ThreeVector helper =
    (std::abs(emissionAxis.z()) < 0.9) ? G4ThreeVector(0., 0., 1.) : G4ThreeVector(1., 0., 0.);
  const G4ThreeVector transverseU = emissionAxis.cross(helper).unit();
  const G4ThreeVector transverseV = emissionAxis.cross(transverseU).unit();

  const G4ThreeVector bunchCenter(0., 0., fSourceZ);
  const G4double du = G4RandGauss::shoot(0.0, fFissionPointRadiusSigma);
  const G4double dv = G4RandGauss::shoot(0.0, fFissionPointRadiusSigma);
  const G4double ds = G4RandGauss::shoot(0.0, fFissionPointLengthSigma);
  const G4ThreeVector fissionPoint =
    bunchCenter + du * transverseU + dv * transverseV + ds * emissionAxis;
  const G4double fissionTime = G4RandGauss::shoot(0.0, fTimeSigma);

  auto emitFragment = [&](G4ParticleDefinition* fragmentIon, const G4ThreeVector& nominalDirection,
                          const G4double meanEnergy, const G4double energySigma) {
    const G4double thetaU = G4RandGauss::shoot(0.0, fAngularSigma);
    const G4double thetaV = G4RandGauss::shoot(0.0, fAngularSigma);
    const G4ThreeVector direction =
      (nominalDirection + thetaU * transverseU + thetaV * transverseV).unit();
    const G4double energy = std::max(1.0 * MeV, G4RandGauss::shoot(meanEnergy, energySigma));

    source->SetParticleDefinition(fragmentIon);
    source->SetParticleCharge(fFragmentChargeState);
    source->SetParticleTime(fissionTime);
    source->GetPosDist()->SetCentreCoords(fissionPoint);
    source->GetAngDist()->SetParticleMomentumDirection(direction);
    source->GetEneDist()->SetMonoEnergy(energy);
    fParticleSource->GeneratePrimaryVertex(anEvent);
  };

  emitFragment(fHeavyFragment, emissionAxis, fHeavyMeanEnergy, fHeavyEnergySigma);
  emitFragment(fLightFragment, -emissionAxis, fLightMeanEnergy, fLightEnergySigma);

  // Optional additional model particles (disabled by default).
  for (G4int i = 2; i < fFragmentsPerFission; ++i) {
    emitFragment(fLightFragment, -emissionAxis, fLightMeanEnergy, fLightEnergySigma);
  }

  fConfiguredSourceSummary =
    "Ba140/Kr95 fission-fragment pair at 70/95 MeV (q=+22e)";

  // Restore nominal values so UI summaries and follow-up runs stay stable.
  source->SetParticleDefinition(fHeavyFragment);
  source->SetParticleCharge(fFragmentChargeState);
  source->SetParticleTime(0.0);
  source->GetPosDist()->SetCentreCoords(bunchCenter);
  source->GetAngDist()->SetParticleMomentumDirection(G4ThreeVector(0., 0., 1.));
  source->GetEneDist()->SetMonoEnergy(fHeavyMeanEnergy);
}

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

}
