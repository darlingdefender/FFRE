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
/// \file RunAction.hh
/// \brief Definition of the B1::RunAction class

#ifndef B1RunAction_h
#define B1RunAction_h 1

#include "G4UserRunAction.hh"
#include "G4Accumulable.hh"
#include "globals.hh"

#include <array>

class G4Run;

/// Run action class
///
/// In EndOfRunAction(), it calculates the dose in the selected volume
/// from the energy deposit accumulated via stepping and event actions.
/// The computed dose is then printed on the screen.

namespace B1
{

inline constexpr std::size_t kEscapeSpeedBinCount = 7;
using EscapeSpeedBinArray = std::array<G4int, kEscapeSpeedBinCount>;

class RunAction : public G4UserRunAction
{
  public:
    RunAction();
    ~RunAction() override;

    void BeginOfRunAction(const G4Run*) override;
    void   EndOfRunAction(const G4Run*) override;

    void AddEdep (G4double edep);
    void AddEventFissionStats(G4int nFission, G4int nFissionFragments,
                              G4int nEnterShape3, G4int nEnterShape4,
                              G4int nOuterFuelShellEscapes,
                              G4double outerFuelShellEscapeEnergy,
                              G4double outerFuelShellEscapeSpeed,
                              G4double outerFuelShellEscapeSpeed2,
                              const EscapeSpeedBinArray& escapeSpeedBins,
                              G4double directFragmentInitialEnergy);
    void AddEventFuelBedHydrogenStats(G4double hydrogenEdep,
                                      G4int nHydrogenEntries,
                                      G4int nHydrogenExits,
                                      G4double hydrogenPzIn,
                                      G4double hydrogenPzOut,
                                      G4double hydrogenExitEnergy,
                                      G4double hydrogenExitSpeed,
                                      G4double hydrogenExitSpeed2,
                                      G4double hydrogenExitCosTheta);

  private:
    void DisableRadioactiveDecayProcesses();

    G4Accumulable<G4double> fEdep = 0.;
    G4Accumulable<G4double> fEdep2 = 0.;
    G4Accumulable<G4int> fFissionEventCount = 0;
    G4Accumulable<G4int> fTotalNFission = 0;
    G4Accumulable<G4int> fTotalFissionFragments = 0;
    G4Accumulable<G4int> fMultiFissionEventCount = 0;
    G4Accumulable<G4int> fTotalOuterFuelShellEscapes = 0;
    G4Accumulable<G4int> fShape3EventCount = 0;
    G4Accumulable<G4int> fShape4EventCount = 0;
    G4Accumulable<G4int> fShape34EventCount = 0;
    G4Accumulable<G4int> fTotalShape3Entries = 0;
    G4Accumulable<G4int> fTotalShape4Entries = 0;
    std::array<G4Accumulable<G4int>, kEscapeSpeedBinCount>
      fOuterFuelShellEscapeSpeedBins;
    G4Accumulable<G4double> fTotalOuterFuelShellEscapeEnergy = 0.;
    G4Accumulable<G4double> fTotalOuterFuelShellEscapeSpeed = 0.;
    G4Accumulable<G4double> fTotalOuterFuelShellEscapeSpeed2 = 0.;
    G4Accumulable<G4double> fTotalDirectFragmentInitialEnergy = 0.;
    G4Accumulable<G4int> fFuelBedHydrogenActiveEventCount = 0;
    G4Accumulable<G4int> fTotalFuelBedHydrogenEntries = 0;
    G4Accumulable<G4int> fTotalFuelBedHydrogenExits = 0;
    G4Accumulable<G4double> fTotalFuelBedHydrogenEdep = 0.;
    G4Accumulable<G4double> fTotalFuelBedHydrogenPzIn = 0.;
    G4Accumulable<G4double> fTotalFuelBedHydrogenPzOut = 0.;
    G4Accumulable<G4double> fTotalFuelBedHydrogenExitEnergy = 0.;
    G4Accumulable<G4double> fTotalFuelBedHydrogenExitSpeed = 0.;
    G4Accumulable<G4double> fTotalFuelBedHydrogenExitSpeed2 = 0.;
    G4Accumulable<G4double> fTotalFuelBedHydrogenExitCosTheta = 0.;
    G4bool fRadioactiveDecayDisabled = false;
};

}

#endif
