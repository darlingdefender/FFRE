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
#include "PrimaryGeneratorAction.hh"
#include "DetectorConstruction.hh"
// #include "Run.hh"

#include "G4RunManager.hh"
#include "G4Run.hh"
#include "G4AccumulableManager.hh"
#include "G4LogicalVolumeStore.hh"
#include "G4LogicalVolume.hh"
#include "G4UnitsTable.hh"
#include "G4PhysicalConstants.hh"
#include "G4SystemOfUnits.hh"

#include <algorithm>
#include <cmath>

namespace B1
{

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

RunAction::RunAction()
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
}

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

RunAction::~RunAction()
{}

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

void RunAction::BeginOfRunAction(const G4Run*)
{
  // inform the runManager to save random number seed
  G4RunManager::GetRunManager()->SetRandomNumberStore(false);

  // reset accumulables to their initial values
  G4AccumulableManager* accumulableManager = G4AccumulableManager::Instance();
  accumulableManager->Reset();

}

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

void RunAction::EndOfRunAction(const G4Run* run)
{
  G4int nofEvents = run->GetNumberOfEvent();
  if (nofEvents == 0) return;

  // Merge accumulables
  G4AccumulableManager* accumulableManager = G4AccumulableManager::Instance();
  accumulableManager->Merge();

  // Simulation tally (unscaled): deposited energy and event statistics.
  G4double edep  = fEdep.GetValue();
  G4double edep2 = fEdep2.GetValue();

  const G4double meanEdepPerEvent = edep / nofEvents;
  G4double varianceEdepPerEvent =
    edep2 / nofEvents - meanEdepPerEvent * meanEdepPerEvent;
  varianceEdepPerEvent = std::max(0.0, varianceEdepPerEvent);
  const G4double sigmaEdepPerEvent = std::sqrt(varianceEdepPerEvent);

  const DetectorConstruction* detConstruction
   = static_cast<const DetectorConstruction*>
     (G4RunManager::GetRunManager()->GetUserDetectorConstruction());

  const G4double mass = detConstruction->GetModeledDustMass();
  G4double dose = 0.;
  G4double meanDosePerEvent = 0.;
  G4double sigmaDosePerEvent = 0.;
  if (mass > 0.) {
    dose = edep / mass;
    meanDosePerEvent = meanEdepPerEvent / mass;
    sigmaDosePerEvent = sigmaEdepPerEvent / mass;
  }

  const G4double referenceTemperature = detConstruction->GetReferenceDustTemperature();
  const G4double targetTemperature = detConstruction->GetTargetDustTemperature();
  const G4double effectiveHeatCapacity = detConstruction->GetDustEffectiveHeatCapacity();
  const G4double reactorThermalPower = detConstruction->GetReferenceThermalPower();
  const G4double operationDuration = detConstruction->GetReferenceOperationDuration();
  const G4double fissionEnergyRelease = detConstruction->GetFissionEnergyRelease();
  const G4double fragmentEnergyFraction = detConstruction->GetFissionFragmentEnergyFraction();
  const G4double referenceCriticalMass = detConstruction->GetReferenceCriticalFissileMass();
  const G4double referenceThermalDesignFuelMass =
    detConstruction->GetReferenceThermalDesignFuelMass();
  const G4double dustGrainRadius = detConstruction->GetDustGrainRadius();
  const G4double fuelGrainDensity = detConstruction->GetFuelGrainDensity();
  const G4double frictionFraction = detConstruction->GetFrictionFraction();
  const G4double stabilityTemperature = 3800. * kelvin;
  const G4double stefanBoltzmannConstant =
    5.670374419e-8 * watt / (m2 * kelvin * kelvin * kelvin * kelvin);

  // Map MC events to physical operation window at reference power.
  G4double fissionRate = 0.;
  G4double expectedFissions = 0.;
  G4double eventWeight = 0.;
  if (fissionEnergyRelease > 0. && operationDuration > 0.) {
    fissionRate = reactorThermalPower / fissionEnergyRelease;
    expectedFissions = fissionRate * operationDuration;
    eventWeight = expectedFissions / nofEvents;
  }

  const G4double normalizedEdep = edep * eventWeight;
  G4double normalizedDose = 0.;
  if (mass > 0.) {
    normalizedDose = normalizedEdep / mass;
  }

  const G4double thermalMass =
    (mass > 0. && effectiveHeatCapacity > 0.) ? mass * effectiveHeatCapacity : 0.;
  G4double deltaTRun = 0.;
  G4double meanDeltaTPerEvent = 0.;
  G4double sigmaDeltaTPerEvent = 0.;
  G4double normalizedDeltaTRun = 0.;
  G4double finalDustTemperature = referenceTemperature;
  G4double targetReachFraction = 0.;
  if (thermalMass > 0.) {
    deltaTRun = edep / thermalMass;
    meanDeltaTPerEvent = meanEdepPerEvent / thermalMass;
    sigmaDeltaTPerEvent = sigmaEdepPerEvent / thermalMass;
    normalizedDeltaTRun = normalizedEdep / thermalMass;
    finalDustTemperature = referenceTemperature + normalizedDeltaTRun;

    const G4double energyToTarget =
      thermalMass * std::max(0.0, targetTemperature - referenceTemperature);
    if (energyToTarget > 0.) {
      targetReachFraction = normalizedEdep / energyToTarget;
    }
  }

  G4double depositedPowerInScoring = 0.;
  if (operationDuration > 0.) {
    depositedPowerInScoring = normalizedEdep / operationDuration;
  }
  G4double depositionVsThermal = 0.;
  if (reactorThermalPower > 0.) {
    depositionVsThermal = depositedPowerInScoring / reactorThermalPower;
  }
  const G4double fragmentPowerBudget = reactorThermalPower * fragmentEnergyFraction;
  G4double depositionVsFragment = 0.;
  if (fragmentPowerBudget > 0.) {
    depositionVsFragment = depositedPowerInScoring / fragmentPowerBudget;
  }

  G4double maxRadiativePowerAtTarget = 0.;
  G4double maxRadiativePowerAtStability = 0.;
  G4double thermalOverloadAtTarget = 0.;
  G4double thermalOverloadAtStability = 0.;
  G4double requiredMassAtTarget = 0.;
  G4double requiredMassAtStability = 0.;
  if (mass > 0. && dustGrainRadius > 0. && fuelGrainDensity > 0. && frictionFraction > 0.) {
    const G4double denom = dustGrainRadius * fuelGrainDensity * frictionFraction;
    maxRadiativePowerAtTarget =
      3.0 * mass * stefanBoltzmannConstant * std::pow(targetTemperature, 4) / denom;
    maxRadiativePowerAtStability =
      3.0 * mass * stefanBoltzmannConstant * std::pow(stabilityTemperature, 4) / denom;

    if (maxRadiativePowerAtTarget > 0.) {
      thermalOverloadAtTarget = depositedPowerInScoring / maxRadiativePowerAtTarget;
    }
    if (maxRadiativePowerAtStability > 0.) {
      thermalOverloadAtStability = depositedPowerInScoring / maxRadiativePowerAtStability;
    }

    requiredMassAtTarget =
      depositedPowerInScoring * denom /
      (3.0 * stefanBoltzmannConstant * std::pow(targetTemperature, 4));
    requiredMassAtStability =
      depositedPowerInScoring * denom /
      (3.0 * stefanBoltzmannConstant * std::pow(stabilityTemperature, 4));
  }

  G4double criticalityMassRatio = 0.;
  if (referenceCriticalMass > 0.) {
    criticalityMassRatio = mass / referenceCriticalMass;
  }

  G4double thermalDesignMassRatio = 0.;
  if (referenceThermalDesignFuelMass > 0.) {
    thermalDesignMassRatio = mass / referenceThermalDesignFuelMass;
  }

  G4double timeToTargetTemperature = -1.;
  G4double timeToStabilityTemperature = -1.;
  if (depositedPowerInScoring > 0. && thermalMass > 0.) {
    const G4double dTTarget = std::max(0.0, targetTemperature - referenceTemperature);
    const G4double dTStability = std::max(0.0, stabilityTemperature - referenceTemperature);
    timeToTargetTemperature = thermalMass * dTTarget / depositedPowerInScoring;
    timeToStabilityTemperature = thermalMass * dTStability / depositedPowerInScoring;
  }

  const G4String noGoCriticality = (criticalityMassRatio >= 1.0) ? "PASS" : "FAIL";
  const G4String noGoThermalMass = (thermalDesignMassRatio >= 1.0) ? "PASS" : "FAIL";
  const G4String noGoThermalTarget = (thermalOverloadAtTarget <= 1.0) ? "PASS" : "FAIL";
  const G4String noGoThermalStability = (thermalOverloadAtStability <= 1.0) ? "PASS" : "FAIL";
  const G4bool noGoOverheat =
    (timeToTargetTemperature >= 0.0 && timeToTargetTemperature < operationDuration);

  G4String thermalFeasibility = "INSUFFICIENT_HEATING";
  if (targetReachFraction >= 1.0) {
    thermalFeasibility = "POTENTIALLY_FEASIBLE";
  }
  else if (targetReachFraction >= 0.3) {
    thermalFeasibility = "MARGINAL";
  }

  G4String dustSurvivalRisk = "LOW";
  if (finalDustTemperature >= 3200. * kelvin) {
    dustSurvivalRisk = "HIGH";
  }
  else if (finalDustTemperature >= 2600. * kelvin) {
    dustSurvivalRisk = "MEDIUM";
  }

  const G4bool noGoConfirmed =
    thermalFeasibility != "POTENTIALLY_FEASIBLE"
    || noGoCriticality == "FAIL"
    || noGoThermalMass == "FAIL"
    || noGoThermalTarget == "FAIL"
    || noGoThermalStability == "FAIL"
    || noGoOverheat;
  const G4String noGoVerdict = noGoConfirmed ? "CONFIRMED" : "NOT_CONFIRMED";

  // Run conditions
  //  note: There is no primary generator action object for "master"
  //        run manager for multi-threaded mode.
  const PrimaryGeneratorAction* generatorAction
   = static_cast<const PrimaryGeneratorAction*>
     (G4RunManager::GetRunManager()->GetUserPrimaryGeneratorAction());
  G4String runCondition;
  if (generatorAction)
  {
    runCondition += generatorAction->GetConfiguredSourceSummary();
  }
  else {
    runCondition += "(source summary unavailable on master thread)";
  }

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
     << " Reference dusty-plasma core temperature: "
     << referenceTemperature / kelvin << " K"
     << " at "
     << reactorThermalPower / (1.0e6 * watt) << " MWth"
     << " (no-go screening assumption, not solved by Geant4)"
     << G4endl
     << " Screening design point: literature-based low-density dust cloud for no-go confirmation"
     << G4endl
     << " Dust thermal model assumptions: Cp_eff = "
     << effectiveHeatCapacity / (joule / (kg * kelvin))
     << " J/(kg*K), target temperature = " << targetTemperature / kelvin << " K"
     << ", packing fraction = " << detConstruction->GetDustPackingFraction()
     << ", bulk density = "
      << detConstruction->GetDustBulkDensity() / (g / cm3) << " g/cm3"
     << ", cloud-average density = "
     << detConstruction->GetDustCloudAverageDensity() / (g / cm3) << " g/cm3"
     << G4endl
     << " Unscaled MC tally: cumulated dose in scoring volume = "
     << G4BestUnit(dose,"Dose")
     << G4endl
     << " Unscaled MC event statistics: mean dose = " << G4BestUnit(meanDosePerEvent,"Dose")
     << ", sigma = " << G4BestUnit(sigmaDosePerEvent,"Dose")
     << G4endl
     << " Unscaled MC temperature rise: dT_run = " << deltaTRun / kelvin
     << " K, per-event mean = " << meanDeltaTPerEvent / kelvin
     << " K, sigma = " << sigmaDeltaTPerEvent / kelvin << " K"
     << G4endl
     << " Power normalization window: " << G4BestUnit(operationDuration, "Time")
     << " at " << reactorThermalPower / (1.0e6 * watt) << " MWth"
     << " => expected fissions = " << expectedFissions
     << ", event weight = " << eventWeight
     << G4endl
     << " Reactor-normalized scoring dose in window: " << G4BestUnit(normalizedDose, "Dose")
     << G4endl
     << " Reactor-normalized scoring power: " << G4BestUnit(depositedPowerInScoring, "Power")
     << " (fraction of reactor thermal power = " << depositionVsThermal * 100.0
     << "%, fraction of FF kinetic budget = " << depositionVsFragment * 100.0 << "%)"
     << G4endl
     << " Reactor-normalized temperature rise over window: " << normalizedDeltaTRun / kelvin
     << " K, estimated dust temperature = " << finalDustTemperature / kelvin << " K"
     << G4endl
     << " Thermal feasibility screen (normalized): " << thermalFeasibility
     << " (target reach = " << targetReachFraction * 100.0
     << "%, dust survival risk = " << dustSurvivalRisk << ")"
     << G4endl
     << " No-go confirmation verdict: " << noGoVerdict
     << " (fails if heating is insufficient or any dashboard gate fails)"
     << G4endl
     << " NO-GO evidence dashboard:"
     << G4endl
     << "  [1] Critical-mass proxy (U-235 11 kg): " << noGoCriticality
     << " (modeled dust mass = " << G4BestUnit(mass, "Mass")
     << ", ratio = " << criticalityMassRatio * 100.0 << "%)"
     << G4endl
     << "  [2] Thermal-design mass closure (15 kg literature point): " << noGoThermalMass
     << " (modeled/reference = " << thermalDesignMassRatio * 100.0 << "%)"
     << G4endl
     << "  [3] Grain-radiation limit @ " << targetTemperature / kelvin << " K: "
     << noGoThermalTarget
     << " (required/available cooling power = " << thermalOverloadAtTarget
     << "x, required dust mass = " << G4BestUnit(requiredMassAtTarget, "Mass")
     << ", modeled = " << G4BestUnit(mass, "Mass") << ")"
     << G4endl
     << "  [4] Grain-radiation limit @ " << stabilityTemperature / kelvin << " K: "
     << noGoThermalStability
     << " (required/available cooling power = " << thermalOverloadAtStability
     << "x, required dust mass = " << G4BestUnit(requiredMassAtStability, "Mass")
     << ", modeled = " << G4BestUnit(mass, "Mass") << ")"
     << G4endl
     << "  [5] Overheat time from " << referenceTemperature / kelvin << " K: "
     << "to " << targetTemperature / kelvin << " K in "
     << G4BestUnit(std::max(0.0, timeToTargetTemperature), "Time")
     << ", to " << stabilityTemperature / kelvin << " K in "
     << G4BestUnit(std::max(0.0, timeToStabilityTemperature), "Time")
     << G4endl
     << " Note: this feasibility is thermal-only from deposited energy with power normalization;"
     << " transport,"
     << " radiation loss, ionization balance and source reactor physics are not solved."
     << G4endl
     << "------------------------------------------------------------"
     << G4endl
     << G4endl;
}

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

void RunAction::AddEdep(G4double edep)
{
  fEdep  += edep;
  fEdep2 += edep*edep;
}

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

}
