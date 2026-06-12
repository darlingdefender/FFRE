/// \file SteppingAction.cc
/// \brief Implementation of the B1::SteppingAction class

#include "SteppingAction.hh"
#include "EventAction.hh"
#include "DetectorConstruction.hh"

#include "G4Step.hh"
#include "G4Event.hh"
#include "G4RunManager.hh"
#include "G4LogicalVolume.hh"
#include "G4Track.hh"
#include "G4ParticleDefinition.hh"
#include "G4VProcess.hh"
#include "G4TouchableHandle.hh"
#include "G4SystemOfUnits.hh"
#include "G4StepPoint.hh"

#include <sstream>

namespace B1
{

namespace
{
G4int gPrintedFragmentStops = 0;
constexpr G4int kMaxPrintedFragmentStops = 12;
G4int gPrintedFragmentMoves = 0;
constexpr G4int kMaxPrintedFragmentMoves = 24;

G4String FormatVolumeCopy(const G4StepPoint* stepPoint)
{
  if (!stepPoint) {
    return "None";
  }

  auto touchable = stepPoint->GetTouchableHandle();
  auto* volume = touchable ? touchable->GetVolume() : nullptr;
  if (!volume) {
    return "None";
  }

  std::ostringstream os;
  os << volume->GetName() << "[copy=" << touchable->GetCopyNumber() << "]";
  return os.str();
}

G4bool IsParticleBedVolume(const G4String& volumeName)
{
  return volumeName == "FuelShell" || volumeName == "ModeratorKernel";
}

G4double AxialMomentum(const G4StepPoint* stepPoint)
{
  if (!stepPoint) {
    return 0.;
  }

  return stepPoint->GetMomentum().z();
}

G4double CosThetaZ(const G4StepPoint* stepPoint)
{
  if (!stepPoint) {
    return 0.;
  }

  const G4ThreeVector momentum = stepPoint->GetMomentum();
  if (momentum.mag() <= 0.) {
    return 0.;
  }

  return momentum.z()/momentum.mag();
}

G4int FindEscapeSpeedBinIndex(G4double speed)
{
  const G4double speedKmPerS = speed/(km/s);

  if (speedKmPerS < 8000.) return 0;
  if (speedKmPerS < 10000.) return 1;
  if (speedKmPerS < 12000.) return 2;
  if (speedKmPerS < 14000.) return 3;
  if (speedKmPerS < 16000.) return 4;
  if (speedKmPerS < 18000.) return 5;
  return 6;
}
}

SteppingAction::SteppingAction(EventAction* eventAction)
: fEventAction(eventAction)
{}

SteppingAction::~SteppingAction()
{}

void SteppingAction::UserSteppingAction(const G4Step* step)
{
  // -------------------------------
  // 0) 新事件开始时，清空去重集合
  // -------------------------------
  G4int eventID =
    G4RunManager::GetRunManager()->GetCurrentEvent()->GetEventID();

  if (eventID != fCurrentEventID) {
    fCurrentEventID = eventID;
    fCountedOutwardFuelShellEscapeTracks.clear();
    fCountedFuelBedHydrogenEntryTracks.clear();
    fCountedFuelBedHydrogenExitTracks.clear();
    fCountedShape3Tracks.clear();
    fCountedShape4Tracks.clear();
  }

  G4Track* track = step->GetTrack();

  if (!fScoringVolume) {
    const DetectorConstruction* detConstruction
      = static_cast<const DetectorConstruction*>
        (G4RunManager::GetRunManager()->GetUserDetectorConstruction());
    fScoringVolume = detConstruction->GetScoringVolume();
    fFuelBedHydrogenEnabled =
      detConstruction->IsFuelBedHydrogenEnabled();
  }

  if (eventID == 0 && track->GetTrackID() == 1 && track->GetCurrentStepNumber() == 1) {
    G4cout
      << "[Primary first step] volume = "
      << FormatVolumeCopy(step->GetPreStepPoint())
      << G4endl;
  }

  // 预先判断：这个 track 是否为裂变产生的核碎片
  G4ParticleDefinition* particle = track->GetDefinition();
  G4String particleType = particle->GetParticleType();

  G4int trackID = track->GetTrackID();
  G4int parentID = track->GetParentID();

  const G4VProcess* creator = track->GetCreatorProcess();
  G4String creatorName = "none";
  if (creator) creatorName = creator->GetProcessName();

  G4bool isDirectNFissionProduct =
    (parentID > 0 && creatorName == "nFission");

  G4bool isFissionFragment =
    (isDirectNFissionProduct && particleType == "nucleus");

  if (track->GetCurrentStepNumber() == 1 && isDirectNFissionProduct) {
    G4String productLabel =
      particle->GetParticleName() + " <" + particleType + ">";
    fEventAction->CountNFissionProduct(productLabel);

    if (isFissionFragment) {
      fEventAction->CountFissionFragment();
      fEventAction->AddDirectFragmentInitialEnergy(track->GetKineticEnergy());
    }
  }

  const G4VProcess* stepProcess = step->GetPostStepPoint()->GetProcessDefinedStep();
  if (stepProcess && stepProcess->GetProcessName() == "nFission") {
    fEventAction->CountNFission();
  }

  // -------------------------------
  // 1) 统计裂变碎片首次进入 Shape3 / Shape4
  // -------------------------------
  if (isFissionFragment) {
    auto prePV  = step->GetPreStepPoint()->GetTouchableHandle()->GetVolume();
    auto postPV = step->GetPostStepPoint()->GetTouchableHandle()->GetVolume();

    G4String preName  = (prePV)  ? prePV->GetName()  : "None";
    G4String postName = (postPV) ? postPV->GetName() : "None";
    G4int preCopyNo =
      step->GetPreStepPoint()->GetTouchableHandle()->GetCopyNumber();
    G4int postCopyNo =
      step->GetPostStepPoint()->GetTouchableHandle()->GetCopyNumber();

    if (postName == "Shape1" && preName != "Shape1") {
      if (fCountedFuelBedHydrogenEntryTracks.insert(trackID).second) {
        fEventAction->CountFuelBedHydrogenEntry(
          AxialMomentum(step->GetPostStepPoint()));
      }
    }

    if (preName == "Shape1" && postName != "Shape1" &&
        !IsParticleBedVolume(postName)) {
      if (fCountedFuelBedHydrogenExitTracks.insert(trackID).second) {
        auto* postStepPoint = step->GetPostStepPoint();
        fEventAction->CountFuelBedHydrogenExit(
          AxialMomentum(postStepPoint),
          postStepPoint->GetKineticEnergy(),
          postStepPoint->GetVelocity(),
          CosThetaZ(postStepPoint));
      }
    }

    if (gPrintedFragmentMoves < kMaxPrintedFragmentMoves &&
        (IsParticleBedVolume(preName) || IsParticleBedVolume(postName)) &&
        (preName != postName || preCopyNo != postCopyNo)) {
      G4cout
        << "[Fragment move] event " << eventID
        << ", track " << trackID
        << ", particle " << particle->GetParticleName()
        << ", " << FormatVolumeCopy(step->GetPreStepPoint())
        << " -> " << FormatVolumeCopy(step->GetPostStepPoint())
        << G4endl;
      ++gPrintedFragmentMoves;
    }

    if (track->GetTrackStatus() == fStopButAlive &&
        gPrintedFragmentStops < kMaxPrintedFragmentStops) {
      G4cout
        << "[Fragment stop] event " << eventID
        << ", track " << trackID
        << ", particle " << particle->GetParticleName()
        << ", volume " << FormatVolumeCopy(step->GetPostStepPoint())
        << ", track length " << track->GetTrackLength()/mm << " mm"
        << ", kinetic energy " << track->GetKineticEnergy()/keV << " keV"
        << G4endl;
      ++gPrintedFragmentStops;
    }

    // 文献中的 FER/ESR 关注碎片穿出燃料涂层后的外向逃逸，
    // 这里统计 FuelShell -> Shape1 的首次越界。
    if (preName == "FuelShell" && postName == "Shape1") {
      if (fCountedOutwardFuelShellEscapeTracks.insert(trackID).second) {
        const G4double escapeSpeed = step->GetPostStepPoint()->GetVelocity();
        fEventAction->CountOuterFuelShellEscape(
          step->GetPostStepPoint()->GetKineticEnergy(),
          escapeSpeed,
          FindEscapeSpeedBinIndex(escapeSpeed));
      }
    }

    // 从别的体进入 Shape3
    if (postName == "Shape3" && preName != "Shape3") {
      if (fCountedShape3Tracks.insert(trackID).second) {
        fEventAction->CountEnterShape3();
      }
    }

    // 从别的体进入 Shape4
    if (postName == "Shape4" && preName != "Shape4") {
      if (fCountedShape4Tracks.insert(trackID).second) {
        fEventAction->CountEnterShape4();
      }
    }
  }

  // With radioactive decay disabled, stopped nuclei can remain in
  // fStopButAlive without an AtRest process. Retire them once transport
  // has reached this terminal state so the event can continue.
  if (particleType == "nucleus" && track->GetTrackStatus() == fStopButAlive) {
    track->SetTrackStatus(fStopAndKill);
    return;
  }

  // -------------------------------
  // 3) 原始 B1 的剂量统计逻辑
  // -------------------------------
  G4LogicalVolume* volume
    = step->GetPreStepPoint()->GetTouchableHandle()
      ->GetVolume()->GetLogicalVolume();

  if (volume != fScoringVolume) return;

  G4double edepStep = step->GetTotalEnergyDeposit();
  fEventAction->AddEdep(edepStep);
  if (fFuelBedHydrogenEnabled && isFissionFragment) {
    fEventAction->AddFuelBedHydrogenEdep(edepStep);
  }
}

}  // namespace B1
