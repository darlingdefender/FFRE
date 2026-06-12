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
/// \file exampleB1.cc
/// \brief Main program of the B1 example

#include "DetectorConstruction.hh"
#include "ActionInitialization.hh"

#include "G4RunManagerFactory.hh"
#include "G4SteppingVerbose.hh"
#include "G4UImanager.hh"
#include "G4HadronicParameters.hh"
#include "G4SystemOfUnits.hh"
#include "G4VModularPhysicsList.hh"
#include "G4VisExecutive.hh"
#include "G4UIExecutive.hh"
#include "G4ParticleHPManager.hh"
#include "G4DecayPhysics.hh"
#include "G4EmStandardPhysics.hh"
#include "Randomize.hh"
#include "FTFP_BERT_HP.hh"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

using namespace B1;

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

namespace {

namespace fs = std::filesystem;

constexpr const char* kAm242mFsFile = "Fission/FS/95_242m1_Americium.z";
constexpr const char* kAm242mFfFile = "Fission/FF/95_242m1_Americium.z";

G4bool ReadEnvFlag(const char* name, G4bool defaultValue)
{
  const char* value = std::getenv(name);
  if (!value) {
    return defaultValue;
  }

  const std::string text(value);
  if (text == "0" || text == "false" || text == "FALSE" ||
      text == "off" || text == "OFF" ||
      text == "no" || text == "NO") {
    return false;
  }

  return true;
}

G4bool MacroRequestsVisualization(const char* macroPath)
{
  std::ifstream macroStream(macroPath);
  if (!macroStream) {
    return false;
  }

  std::string line;
  while (std::getline(macroStream, line)) {
    if (line.find("/vis/") != std::string::npos ||
        line.find("/gui/") != std::string::npos) {
      return true;
    }
  }

  return false;
}

fs::path CanonicalPath(fs::path path)
{
  std::error_code ec;
  path = fs::weakly_canonical(path, ec);
  if (ec) {
    return {};
  }
  return path;
}

void ConfigureMacroDefaults(int argc, char** argv)
{
  if (argc != 2 || !argv[1]) {
    return;
  }

  const fs::path macroPath(argv[1]);
  const std::string macroName = macroPath.filename().string();
  const G4bool enlargedFuelLayoutMacro =
    macroName == "show_fuel_layout_enlarged.mac" ||
    macroName == "show_fuel_layout_enlarged_axial.mac" ||
    macroName == "show_fuel_layout_enlarged_y_axis.mac" ||
    macroName == "show_fuel_layout_enlarged_z_axis.mac";
  const G4bool fuelLayoutMacro =
    enlargedFuelLayoutMacro || macroName == "show_fuel_layout.mac";

  if (enlargedFuelLayoutMacro && !std::getenv("FFRE_FUEL_LAYOUT_MARKER_SCALE")) {
    ::setenv("FFRE_FUEL_LAYOUT_MARKER_SCALE", "10", 0);
  }
  if (fuelLayoutMacro && !std::getenv("FFRE_GEOMETRY_VIS_VIEW")) {
    ::setenv("FFRE_GEOMETRY_VIS_VIEW", "1", 0);
  }
  if (fuelLayoutMacro && !std::getenv("FFRE_ENABLE_COLLIMATION_FIELD")) {
    ::setenv("FFRE_ENABLE_COLLIMATION_FIELD", "0", 0);
  }
}

fs::path ResolveExecutableDir(const char* argv0)
{
  if (!argv0 || *argv0 == '\0') {
    return {};
  }

  std::error_code ec;
  fs::path executablePath(argv0);
  if (executablePath.is_relative()) {
    executablePath = fs::current_path(ec) / executablePath;
    if (ec) {
      return {};
    }
  }

  executablePath = CanonicalPath(executablePath);
  if (executablePath.empty()) {
    return {};
  }

  return executablePath.parent_path();
}

G4bool HasAm242mFragmentData(const fs::path& dataRoot)
{
  if (dataRoot.empty()) {
    return false;
  }

  std::error_code ec;
  return fs::exists(dataRoot / kAm242mFsFile, ec) &&
         fs::exists(dataRoot / kAm242mFfFile, ec);
}

fs::path FindBundledAm242mDataRoot(const char* argv0)
{
  std::vector<fs::path> candidates;
  const std::vector<fs::path> relativeDataRoots = {
    "data/G4NDL4.6_patched",
    "data/G4NDL4.6_overlay",
  };

  std::error_code ec;
  const fs::path cwd = fs::current_path(ec);
  if (!ec) {
    for (const auto& relativeDataRoot : relativeDataRoots) {
      candidates.push_back(cwd / relativeDataRoot);
      candidates.push_back(cwd.parent_path() / relativeDataRoot);
    }
  }

  const fs::path executableDir = ResolveExecutableDir(argv0);
  if (!executableDir.empty()) {
    for (const auto& relativeDataRoot : relativeDataRoots) {
      candidates.push_back(executableDir / relativeDataRoot);
      candidates.push_back(executableDir.parent_path() / relativeDataRoot);
    }
  }

  for (const auto& candidate : candidates) {
    const fs::path canonicalCandidate = CanonicalPath(candidate);
    if (HasAm242mFragmentData(canonicalCandidate)) {
      return canonicalCandidate;
    }
  }

  return {};
}

struct HPDataSelection
{
  fs::path dataRoot;
  G4bool hasAm242mFragmentData = false;
  G4bool usingBundledOverlay = false;
};

class GeometryVisPhysicsList : public G4VModularPhysicsList
{
  public:
    GeometryVisPhysicsList()
    {
      SetVerboseLevel(0);
      RegisterPhysics(new G4EmStandardPhysics(0));
      RegisterPhysics(new G4DecayPhysics(0));
    }

    void SetCuts() override
    {
      SetCutsWithDefault();
    }
};

HPDataSelection SelectNeutronHPData(const char* argv0)
{
  HPDataSelection selection;

  if (const char* hpDataEnv = std::getenv("G4NEUTRONHPDATA")) {
    selection.dataRoot = CanonicalPath(fs::path(hpDataEnv));
    selection.hasAm242mFragmentData = HasAm242mFragmentData(selection.dataRoot);
  }

  if (selection.hasAm242mFragmentData) {
    return selection;
  }

  const fs::path bundledOverlay = FindBundledAm242mDataRoot(argv0);
  if (bundledOverlay.empty()) {
    return selection;
  }

  const std::string bundledOverlayString = bundledOverlay.string();
  if (::setenv("G4NEUTRONHPDATA", bundledOverlayString.c_str(), 1) == 0) {
    selection.dataRoot = bundledOverlay;
    selection.hasAm242mFragmentData = true;
    selection.usingBundledOverlay = true;
  }

  return selection;
}

}  // namespace

int main(int argc,char** argv)
{
  ConfigureMacroDefaults(argc, argv);

  const G4bool singleFuelParticleView =
    ReadEnvFlag("FFRE_SINGLE_FUEL_PARTICLE_VIEW", false);
  const G4bool geometryVisView =
    ReadEnvFlag("FFRE_GEOMETRY_VIS_VIEW", false);

  // Detect interactive mode (if no arguments) and define UI session
  //
  G4UIExecutive* ui = nullptr;
  G4bool keepSessionOpenAfterMacro = false;
  if (argc == 1) {
    ui = new G4UIExecutive(argc, argv, "Qt");
  } else if (argc == 2 && MacroRequestsVisualization(argv[1])) {
    ui = new G4UIExecutive(1, argv, "Qt");
    keepSessionOpenAfterMacro = true;
  }

  // Optionally: choose a different Random engine...
  // G4Random::setTheEngine(new CLHEP::MTwistEngine);

  //use G4SteppingVerboseWithUnits
  G4int precision = 4;
  G4SteppingVerbose::UseBestUnit(precision);

  // Construct the default run manager
  //
  auto* runManager =
    G4RunManagerFactory::CreateRunManager(
      (singleFuelParticleView || geometryVisView) ? G4RunManagerType::SerialOnly
                                                  : G4RunManagerType::Default);

  // Set mandatory initialization classes
  //
  // Detector construction
  runManager->SetUserInitialization(new DetectorConstruction());

  // Physics list
  if (singleFuelParticleView || geometryVisView) {
    runManager->SetUserInitialization(new GeometryVisPhysicsList);
    G4cout
      << "[Visualization] Geometry view: using serial run manager and "
         "lightweight EM/decay physics only."
      << G4endl;
  } else {
    // Configure HP data handling before physics initialization.
    auto* hpManager = G4ParticleHPManager::GetInstance();
    hpManager->SetSkipMissingIsotopes(true);
    const HPDataSelection hpDataSelection =
      SelectNeutronHPData(argc > 0 ? argv[0] : nullptr);

    runManager->SetUserInitialization(new FTFP_BERT_HP);

    auto* hadronicParameters = G4HadronicParameters::Instance();
    hadronicParameters->SetVerboseLevel(0);

    // Get the pointer to the User Interface manager
    G4UImanager* UImanager = G4UImanager::GetUIpointer();

    UImanager->ApplyCommand("/process/had/particle_hp/skip_missing_isotopes true");
    UImanager->ApplyCommand("/process/had/particle_hp/produce_fission_fragment true");

    // Native Geant4 11.0.3 Am-242m1 data do not emit direct fragment nuclei and
    // can segfault in the default HP final-state path. Prefer the bundled overlay
    // when it is available; otherwise keep the Wendt fallback for stability.
    if (hpDataSelection.hasAm242mFragmentData) {
      UImanager->ApplyCommand("/process/had/particle_hp/use_Wendt_fission_model false");

      G4cout
        << "[HP data] Using Am-242m fragment overlay: "
        << hpDataSelection.dataRoot
        << G4endl
        << "[HP data] Am-242/Am-242m FF entries are surrogate data for fragment "
           "transport/statistics, not physics-grade yield predictions."
        << G4endl;
    } else {
      UImanager->ApplyCommand("/process/had/particle_hp/use_Wendt_fission_model true");

      G4cout
        << "[HP data] No Am-242m FF data found. Falling back to the Wendt model; "
           "nFission reactions remain available, but direct fragment nuclei may "
           "not be emitted."
        << G4endl;
    }
  }

  // User action initialization
  runManager->SetUserInitialization(new ActionInitialization());
  // Initialize visualization in both interactive and batch modes so that
  // geometry-inspection macros executed from the command line can use /vis/*.
  G4VisManager* visManager = new G4VisExecutive;
  visManager->Initialize();

  // Get the pointer to the User Interface manager
  G4UImanager* UImanager = G4UImanager::GetUIpointer();

  // Process macro or start UI session
  //
  if (argc > 1) {
    // Batch macro execution. If the macro uses visualization commands, keep an
    // interactive UI session alive after the macro so the viewer remains open.
    G4String command = "/control/execute ";
    G4String fileName = argv[1];
    UImanager->ApplyCommand(command+fileName);
    if (keepSessionOpenAfterMacro && ui) {
      ui->SessionStart();
      delete ui;
      ui = nullptr;
    }
  }
  else {
    // interactive mode
    UImanager->ApplyCommand("/control/execute init_vis.mac");
    ui->SessionStart();
    delete ui;
  }

  // Job termination
  // Free the store: user actions, physics_list and detector_description are
  // owned a  nd deleted by the run manager, so they should not be deleted
  // in the main() program !

  delete visManager;
  delete runManager;
}

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo.....
