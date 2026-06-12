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
    EscapeSpeedBinArray fOuterFuelShellEscapeSpeedBins{};
    std::map<G4String, G4int> fNFissionProducts;
};

}

#endif
