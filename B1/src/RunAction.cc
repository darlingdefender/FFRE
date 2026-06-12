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
/// \file RunAction.cc
/// \brief Implementation of the B1::RunAction class

#include "RunAction.hh"
#include "DetectorConstruction.hh"
// #include "Run.hh"

#include "G4RunManager.hh"
#include "G4Run.hh"
#include "G4AccumulableManager.hh"
#include "G4LogicalVolume.hh"
#include "G4ParticleTable.hh"
#include "G4ParticleDefinition.hh"
#include "G4ProcessManager.hh"
#include "G4ProcessVector.hh"
#include "G4VProcess.hh"
#include "G4UnitsTable.hh"
#include "G4SystemOfUnits.hh"

#include <iomanip>
#include <limits>
#include <sstream>

namespace B1
{

namespace
{

const std::array<G4double, kEscapeSpeedBinCount + 1> kEscapeSpeedBinEdgesKmPerS = {
  0., 8000., 10000., 12000., 14000., 16000., 18000.,
  std::numeric_limits<G4double>::infinity()
};

std::string EscapeSpeedBinLabel(std::size_t index)
{
  std::ostringstream os;

  if (index == 0) {
    os << "< 8000";
    return os.str();
  }

  if (index + 1 == kEscapeSpeedBinCount) {
    os << ">= 18000";
    return os.str();
  }

  os << "["
     << static_cast<G4int>(kEscapeSpeedBinEdgesKmPerS[index])
     << ", "
     << static_cast<G4int>(kEscapeSpeedBinEdgesKmPerS[index + 1])
     << ")";
  return os.str();
}

}

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

RunAction::RunAction()
  : fOuterFuelShellEscapeSpeedBins{
      G4Accumulable<G4int>(0),
      G4Accumulable<G4int>(0),
      G4Accumulable<G4int>(0),
      G4Accumulable<G4int>(0),
      G4Accumulable<G4int>(0),
      G4Accumulable<G4int>(0),
      G4Accumulable<G4int>(0)
    }
{
  // add new units for dose
  //
  const G4double milligray = 1.e-3*gray;
  const G4double microgray = 1.e-6*gray;
  const G4double nanogray  = 1.e-9*gray;
  const G4double picogray  = 1.e-12*gray;

  new G4UnitDefinition("milligray", "milliGy" , "Dose", milligray);
  new G4UnitDefinition("microgray", "microGy" , "Dose", microgray);
  new G4UnitDefinition("nanogray" , "nanoGy"  , "Dose", nanogray);
  new G4UnitDefinition("picogray" , "picoGy"  , "Dose", picogray);

  // Register accumulable to the accumulable manager
  G4AccumulableManager* accumulableManager = G4AccumulableManager::Instance();
  accumulableManager->RegisterAccumulable(fEdep);
  accumulableManager->RegisterAccumulable(fEdep2);
  accumulableManager->RegisterAccumulable(fFissionEventCount);
  accumulableManager->RegisterAccumulable(fTotalNFission);
  accumulableManager->RegisterAccumulable(fTotalFissionFragments);
  accumulableManager->RegisterAccumulable(fMultiFissionEventCount);
  accumulableManager->RegisterAccumulable(fTotalOuterFuelShellEscapes);
  accumulableManager->RegisterAccumulable(fShape3EventCount);
  accumulableManager->RegisterAccumulable(fShape4EventCount);
  accumulableManager->RegisterAccumulable(fShape34EventCount);
  accumulableManager->RegisterAccumulable(fTotalShape3Entries);
  accumulableManager->RegisterAccumulable(fTotalShape4Entries);
  for (auto& bin : fOuterFuelShellEscapeSpeedBins) {
    accumulableManager->RegisterAccumulable(bin);
  }
  accumulableManager->RegisterAccumulable(fTotalOuterFuelShellEscapeEnergy);
  accumulableManager->RegisterAccumulable(fTotalOuterFuelShellEscapeSpeed);
  accumulableManager->RegisterAccumulable(fTotalOuterFuelShellEscapeSpeed2);
  accumulableManager->RegisterAccumulable(fTotalDirectFragmentInitialEnergy);
}

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

RunAction::~RunAction()
{}

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

void RunAction::BeginOfRunAction(const G4Run*)
{
  // inform the runManager to save random number seed
  G4RunManager::GetRunManager()->SetRandomNumberStore(false);

  if (!fRadioactiveDecayDisabled) {
    DisableRadioactiveDecayProcesses();
  }

  // reset accumulables to their initial values
  G4AccumulableManager* accumulableManager = G4AccumulableManager::Instance();
  accumulableManager->Reset();

}

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

void RunAction::DisableRadioactiveDecayProcesses()
{
  auto* particleTable = G4ParticleTable::GetParticleTable();
  auto* particleIterator = particleTable->GetIterator();
  particleIterator->reset();

  G4int disabledProcessCount = 0;
  G4int affectedParticleCount = 0;

  while ((*particleIterator)()) {
    auto* particle = particleIterator->value();
    auto* processManager = particle->GetProcessManager();
    if (!processManager) continue;

    auto* processList = processManager->GetProcessList();
    if (!processList) continue;

    G4bool disabledForThisParticle = false;
    for (G4int i = 0; i < processManager->GetProcessListLength(); ++i) {
      auto* process = (*processList)[i];
      if (!process) continue;

      const G4String& processName = process->GetProcessName();
      if (processName == "RadioactiveDecay" ||
          processName == "RadioactiveDecayBase") {
        processManager->SetProcessActivation(process, false);
        ++disabledProcessCount;
        disabledForThisParticle = true;
      }
    }

    if (disabledForThisParticle) {
      ++affectedParticleCount;
    }
  }

  fRadioactiveDecayDisabled = true;

  if (IsMaster()) {
    G4cout
      << "[RunAction] Radioactive decay disabled: "
      << disabledProcessCount << " process entries across "
      << affectedParticleCount << " particle definitions."
      << G4endl;
  }
}

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

void RunAction::EndOfRunAction(const G4Run* run)
{
  G4int nofEvents = run->GetNumberOfEvent();
  if (nofEvents == 0) return;

  // Merge accumulables
  G4AccumulableManager* accumulableManager = G4AccumulableManager::Instance();
  accumulableManager->Merge();

  // Compute dose = total energy deposit in a run and its variance
  //
  G4double edep  = fEdep.GetValue();
  G4double edep2 = fEdep2.GetValue();

  G4double rms = edep2 - edep*edep/nofEvents;
  if (rms > 0.) rms = std::sqrt(rms); else rms = 0.;

  const DetectorConstruction* detConstruction
   = static_cast<const DetectorConstruction*>
     (G4RunManager::GetRunManager()->GetUserDetectorConstruction());
  G4double mass = detConstruction->GetScoringVolume()->GetMass();
  G4double dose = edep/mass;
  G4double rmsDose = rms/mass;

  G4int fissionEventCount = fFissionEventCount.GetValue();
  G4int totalNFission = fTotalNFission.GetValue();
  G4int totalFissionFragments = fTotalFissionFragments.GetValue();
  G4int multiFissionEventCount = fMultiFissionEventCount.GetValue();
  G4int totalOuterFuelShellEscapes = fTotalOuterFuelShellEscapes.GetValue();
  G4int shape3EventCount = fShape3EventCount.GetValue();
  G4int shape4EventCount = fShape4EventCount.GetValue();
  G4int shape34EventCount = fShape34EventCount.GetValue();
  G4int totalShape3Entries = fTotalShape3Entries.GetValue();
  G4int totalShape4Entries = fTotalShape4Entries.GetValue();
  EscapeSpeedBinArray totalEscapeSpeedBins{};
  for (std::size_t i = 0; i < totalEscapeSpeedBins.size(); ++i) {
    totalEscapeSpeedBins[i] = fOuterFuelShellEscapeSpeedBins[i].GetValue();
  }
  G4double totalOuterFuelShellEscapeEnergy =
    fTotalOuterFuelShellEscapeEnergy.GetValue();
  G4double totalOuterFuelShellEscapeSpeed =
    fTotalOuterFuelShellEscapeSpeed.GetValue();
  G4double totalOuterFuelShellEscapeSpeed2 =
    fTotalOuterFuelShellEscapeSpeed2.GetValue();
  G4double totalDirectFragmentInitialEnergy =
    fTotalDirectFragmentInitialEnergy.GetValue();

  // Run conditions
  //  note: There is no primary generator action object for "master"
  //        run manager for multi-threaded mode.
  G4String runCondition="GPS source";

  // Print
  //
  if (IsMaster()) {
    G4cout
     << G4endl
     << "--------------------End of Global Run-----------------------";
  }
  else {
    G4cout
     << G4endl
     << "--------------------End of Local Run------------------------";
  }

  G4cout
     << G4endl
     << " The run consists of " << nofEvents << " "<< runCondition
     << G4endl
     << " Cumulated dose per run, in scoring volume : "
     << G4BestUnit(dose,"Dose") << " rms = " << G4BestUnit(rmsDose,"Dose")
     << G4endl
     << "------------------------------------------------------------"
     << G4endl
     << G4endl;

  if (IsMaster()) {
    auto percent = [](G4int numerator, G4int denominator) -> G4double {
      if (denominator == 0) return 0.;
      return 100.*static_cast<G4double>(numerator)/denominator;
    };
    auto percentDouble = [](G4double numerator, G4double denominator) -> G4double {
      if (denominator <= 0.) return 0.;
      return 100.*numerator/denominator;
    };
    auto speedRms = [](G4double speedSum, G4double speed2Sum,
                       G4int count) -> G4double {
      if (count <= 0) return 0.;
      const G4double mean = speedSum/count;
      const G4double variance = speed2Sum/count - mean*mean;
      return variance > 0. ? std::sqrt(variance) : 0.;
    };

    const G4double meanEscapeSpeed =
      totalOuterFuelShellEscapes > 0
        ? totalOuterFuelShellEscapeSpeed/totalOuterFuelShellEscapes
        : 0.;
    const G4double rmsEscapeSpeed =
      speedRms(totalOuterFuelShellEscapeSpeed,
               totalOuterFuelShellEscapeSpeed2,
               totalOuterFuelShellEscapes);

    G4cout
      << "================ Fission Transport Summary ================="
      << G4endl
      << std::fixed << std::setprecision(3)
      << " Fission events                     : " << fissionEventCount
      << " / " << nofEvents
      << " (" << percent(fissionEventCount, nofEvents) << "%)"
      << G4endl
      << " Total nFission reactions           : " << totalNFission
      << G4endl
      << " Total direct fission fragments     : " << totalFissionFragments
      << G4endl
      << " Multi-fission events               : " << multiFissionEventCount
      << G4endl
      << " Outward fuel-coating escapes       : " << totalOuterFuelShellEscapes
      << " ("
      << percent(totalOuterFuelShellEscapes, totalFissionFragments)
      << "% of direct fragments)"
      << G4endl
      << " Outward coating escape energy      : "
      << totalOuterFuelShellEscapeEnergy/MeV
      << " MeV ("
      << percentDouble(totalOuterFuelShellEscapeEnergy,
                       totalDirectFragmentInitialEnergy)
      << "% of direct-fragment initial energy)"
      << G4endl
      << " Outward coating escape speed       : "
      << meanEscapeSpeed/(km/s)
      << " km/s (beta = " << meanEscapeSpeed/CLHEP::c_light
      << ", RMS = " << rmsEscapeSpeed/(km/s)
      << " km/s)"
      << G4endl
      << " Escape speed distribution (km/s)   :"
      << G4endl;

    for (std::size_t i = 0; i < totalEscapeSpeedBins.size(); ++i) {
      G4cout
        << "   " << std::setw(15) << std::left << EscapeSpeedBinLabel(i)
        << " : " << std::setw(6) << std::right << totalEscapeSpeedBins[i]
        << " ("
        << percent(totalEscapeSpeedBins[i], totalOuterFuelShellEscapes)
        << "%)"
        << G4endl;
    }

    G4cout
      << std::right
      << " Shape3 hit events                  : " << shape3EventCount
      << " / " << nofEvents
      << " (" << percent(shape3EventCount, nofEvents) << "%)"
      << ", entries = " << totalShape3Entries
      << G4endl
      << " Shape4 hit events                  : " << shape4EventCount
      << " / " << nofEvents
      << " (" << percent(shape4EventCount, nofEvents) << "%)"
      << ", entries = " << totalShape4Entries
      << G4endl
      << " Shape3 or Shape4 hit events        : " << shape34EventCount
      << " / " << nofEvents
      << " (" << percent(shape34EventCount, nofEvents) << "%)"
      << G4endl;

    if (fissionEventCount > 0) {
      G4cout
        << " Shape3 hit rate among fission evts : "
        << percent(shape3EventCount, fissionEventCount) << "%"
        << G4endl
        << " Shape4 hit rate among fission evts : "
        << percent(shape4EventCount, fissionEventCount) << "%"
        << G4endl;
    }

    G4cout
      << std::defaultfloat
      << "============================================================"
      << G4endl
      << G4endl;
  }
}

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

void RunAction::AddEdep(G4double edep)
{
  fEdep  += edep;
  fEdep2 += edep*edep;
}

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

void RunAction::AddEventFissionStats(G4int nFission, G4int nFissionFragments,
                                     G4int nEnterShape3, G4int nEnterShape4,
                                     G4int nOuterFuelShellEscapes,
                                     G4double outerFuelShellEscapeEnergy,
                                     G4double outerFuelShellEscapeSpeed,
                                     G4double outerFuelShellEscapeSpeed2,
                                     const EscapeSpeedBinArray& escapeSpeedBins,
                                     G4double directFragmentInitialEnergy)
{
  if (nFission > 0) {
    ++fFissionEventCount;
    fTotalNFission += nFission;
    fTotalFissionFragments += nFissionFragments;
    if (nFission > 1) {
      ++fMultiFissionEventCount;
    }
  }

  fTotalOuterFuelShellEscapes += nOuterFuelShellEscapes;
  fTotalOuterFuelShellEscapeEnergy += outerFuelShellEscapeEnergy;
  fTotalOuterFuelShellEscapeSpeed += outerFuelShellEscapeSpeed;
  fTotalOuterFuelShellEscapeSpeed2 += outerFuelShellEscapeSpeed2;
  for (std::size_t i = 0; i < escapeSpeedBins.size(); ++i) {
    fOuterFuelShellEscapeSpeedBins[i] += escapeSpeedBins[i];
  }
  fTotalDirectFragmentInitialEnergy += directFragmentInitialEnergy;

  if (nEnterShape3 > 0) {
    ++fShape3EventCount;
    fTotalShape3Entries += nEnterShape3;
  }

  if (nEnterShape4 > 0) {
    ++fShape4EventCount;
    fTotalShape4Entries += nEnterShape4;
  }

  if (nEnterShape3 > 0 || nEnterShape4 > 0) {
    ++fShape34EventCount;
  }
}

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

}
