#ifndef B1EventAction_h
#define B1EventAction_h 1

#include "G4UserEventAction.hh"
#include "RunAction.hh"
#include "globals.hh"

#include <array>
#include <map>

namespace B1
{

class RunAction;

class EventAction : public G4UserEventAction
{
  public:
    EventAction(RunAction* runAction);
    virtual ~EventAction();

    virtual void BeginOfEventAction(const G4Event*) override;
    virtual void EndOfEventAction(const G4Event*) override;

    void AddEdep(G4double edep) { fEdep += edep; }

    // Event-level fission statistics
    void CountNFission() { ++fNNFission; }
    void CountNFissionProduct(const G4String& product) { ++fNFissionProducts[product]; }
    void CountFissionFragment() { ++fNFissionFragments; }
    void AddDirectFragmentInitialEnergy(G4double energy)
    {
      fDirectFragmentInitialEnergy += energy;
    }
    void CountOuterFuelShellEscape(G4double escapedEnergy,
                                   G4double escapedSpeed,
                                   G4int speedBinIndex)
    {
      ++fNOuterFuelShellEscapes;
      fOuterFuelShellEscapeEnergy += escapedEnergy;
      fOuterFuelShellEscapeSpeed += escapedSpeed;
      fOuterFuelShellEscapeSpeed2 += escapedSpeed*escapedSpeed;
      if (speedBinIndex >= 0 &&
          speedBinIndex < static_cast<G4int>(fOuterFuelShellEscapeSpeedBins.size())) {
        ++fOuterFuelShellEscapeSpeedBins[speedBinIndex];
      }
    }
    void CountEnterShape3() { ++fNEnterShape3; }
    void CountEnterShape4() { ++fNEnterShape4; }
    void AddFuelBedHydrogenEdep(G4double edep)
    {
      fFuelBedHydrogenEdep += edep;
    }
    void CountFuelBedHydrogenEntry(G4double axialMomentum)
    {
      ++fNFuelBedHydrogenEntries;
      fFuelBedHydrogenPzIn += axialMomentum;
    }
    void CountFuelBedHydrogenExit(G4double axialMomentum,
                                  G4double kineticEnergy,
                                  G4double speed,
                                  G4double cosThetaZ)
    {
      ++fNFuelBedHydrogenExits;
      fFuelBedHydrogenPzOut += axialMomentum;
      fFuelBedHydrogenExitEnergy += kineticEnergy;
      fFuelBedHydrogenExitSpeed += speed;
      fFuelBedHydrogenExitSpeed2 += speed*speed;
      fFuelBedHydrogenExitCosTheta += cosThetaZ;
    }

  private:
    RunAction* fRunAction = nullptr;
    G4bool fPrintEventSummaries = true;

    G4double fEdep = 0.;

    // Event-level counters
    G4int fNNFission = 0;
    G4int fNFissionFragments = 0;
    G4int fNOuterFuelShellEscapes = 0;
    G4int fNEnterShape3 = 0;
    G4int fNEnterShape4 = 0;
    G4double fDirectFragmentInitialEnergy = 0.;
    G4double fOuterFuelShellEscapeEnergy = 0.;
    G4double fOuterFuelShellEscapeSpeed = 0.;
    G4double fOuterFuelShellEscapeSpeed2 = 0.;
    G4double fFuelBedHydrogenEdep = 0.;
    G4double fFuelBedHydrogenPzIn = 0.;
    G4double fFuelBedHydrogenPzOut = 0.;
    G4double fFuelBedHydrogenExitEnergy = 0.;
    G4double fFuelBedHydrogenExitSpeed = 0.;
    G4double fFuelBedHydrogenExitSpeed2 = 0.;
    G4double fFuelBedHydrogenExitCosTheta = 0.;
    G4int fNFuelBedHydrogenEntries = 0;
    G4int fNFuelBedHydrogenExits = 0;
    EscapeSpeedBinArray fOuterFuelShellEscapeSpeedBins{};
    std::map<G4String, G4int> fNFissionProducts;
};

}

#endif
