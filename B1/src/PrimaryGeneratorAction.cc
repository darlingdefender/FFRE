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
#include "DetectorConstruction.hh"

#include "G4GeneralParticleSource.hh"
#include "G4Event.hh"
#include "G4PrimaryVertex.hh"
#include "G4RunManager.hh"
#include "G4SystemOfUnits.hh"
#include "Randomize.hh"

#include <cmath>

namespace B1
{

PrimaryGeneratorAction::PrimaryGeneratorAction()
{
  fGPS = new G4GeneralParticleSource();
}

PrimaryGeneratorAction::~PrimaryGeneratorAction()
{
  delete fGPS;
}

void PrimaryGeneratorAction::InitializeFuelShellSource()
{
  fFuelShellSourceInitialized = true;

  fDetector = static_cast<const DetectorConstruction*>(
    G4RunManager::GetRunManager()->GetUserDetectorConstruction());
  if (!fDetector) {
    return;
  }

  fSourceFuelShellCenters = fDetector->GetBoundaryFuelShellCenters();
  fUsingBoundaryFuelShellSource = !fSourceFuelShellCenters.empty();
  if (!fUsingBoundaryFuelShellSource) {
    fSourceFuelShellCenters = fDetector->GetFuelShellCenters();
  }
  fFuelShellInnerRadius = fDetector->GetFuelShellInnerRadius();
  fFuelShellOuterRadius = fDetector->GetFuelShellOuterRadius();
}

G4ThreeVector PrimaryGeneratorAction::SampleFuelShellPosition() const
{
  if (fSourceFuelShellCenters.empty()) {
    return G4ThreeVector();
  }

  std::size_t shellIndex =
    static_cast<std::size_t>(G4UniformRand()*fSourceFuelShellCenters.size());
  if (shellIndex >= fSourceFuelShellCenters.size()) {
    shellIndex = fSourceFuelShellCenters.size() - 1;
  }

  G4double innerR3 = std::pow(fFuelShellInnerRadius, 3);
  G4double outerR3 = std::pow(fFuelShellOuterRadius, 3);
  G4double radius =
    std::cbrt(innerR3 + G4UniformRand()*(outerR3 - innerR3));

  G4double cosTheta = 1. - 2.*G4UniformRand();
  G4double sinTheta = std::sqrt(1. - cosTheta*cosTheta);
  G4double phi = CLHEP::twopi*G4UniformRand();

  G4ThreeVector offset(radius*sinTheta*std::cos(phi),
                       radius*sinTheta*std::sin(phi),
                       radius*cosTheta);

  return fSourceFuelShellCenters[shellIndex] + offset;
}

void PrimaryGeneratorAction::GeneratePrimaries(G4Event* anEvent)
{
  fGPS->GeneratePrimaryVertex(anEvent);

  if (!fFuelShellSourceInitialized) {
    InitializeFuelShellSource();
  }

  if (fSourceFuelShellCenters.empty()) {
    return;
  }

  G4ThreeVector sampledPosition = SampleFuelShellPosition();
  for (G4PrimaryVertex* vertex = anEvent->GetPrimaryVertex();
       vertex != nullptr; vertex = vertex->GetNext()) {
    vertex->SetPosition(sampledPosition.x(),
                        sampledPosition.y(),
                        sampledPosition.z());
  }

  if (!fPrintedFuelShellSource && anEvent->GetEventID() == 0) {
    G4cout
      << "[PrimaryGeneratorAction] Sampling source points in "
      << fSourceFuelShellCenters.size() << " "
      << (fUsingBoundaryFuelShellSource ? "boundary " : "")
      << "FuelShell coatings, r = "
      << fFuelShellInnerRadius/mm << " to "
      << fFuelShellOuterRadius/mm << " mm"
      << G4endl;
    fPrintedFuelShellSource = true;
  }
}

} 
