#include "EventAction.hh"
#include "RunAction.hh"

#include "G4Event.hh"
#include "G4RunManager.hh"
#include "G4ios.hh"

#include <cstdlib>
#include <sstream>
#include <string>

namespace B1
{

namespace
{
G4int gPrintedNFissionDetails = 0;
constexpr G4int kMaxPrintedNFissionDetails = 10;

G4bool ReadPrintEventSummaries()
{
  const char* value = std::getenv("FFRE_PRINT_EVENT_SUMMARY");
  if (!value) {
    return true;
  }

  const std::string text(value);
  return !(text == "0" || text == "false" || text == "FALSE" ||
           text == "off" || text == "OFF" ||
           text == "no" || text == "NO");
}
}

EventAction::EventAction(RunAction* runAction)
: fRunAction(runAction),
  fPrintEventSummaries(ReadPrintEventSummaries())
{}

EventAction::~EventAction()
{}

void EventAction::BeginOfEventAction(const G4Event*)
{
  fEdep = 0.;

  // 每个事件开始时清零
  fNNFission = 0;
  fNFissionFragments = 0;
  fNOuterFuelShellEscapes = 0;
  fNEnterShape3 = 0;
  fNEnterShape4 = 0;
  fDirectFragmentInitialEnergy = 0.;
  fOuterFuelShellEscapeEnergy = 0.;
  fOuterFuelShellEscapeSpeed = 0.;
  fOuterFuelShellEscapeSpeed2 = 0.;
  fOuterFuelShellEscapeSpeedBins.fill(0);
  fNFissionProducts.clear();
}

void EventAction::EndOfEventAction(const G4Event* event)
{
  // accumulate statistics in run action
  fRunAction->AddEdep(fEdep);
  fRunAction->AddEventFissionStats(fNNFission, fNFissionFragments,
                                   fNEnterShape3, fNEnterShape4,
                                   fNOuterFuelShellEscapes,
                                   fOuterFuelShellEscapeEnergy,
                                   fOuterFuelShellEscapeSpeed,
                                   fOuterFuelShellEscapeSpeed2,
                                   fOuterFuelShellEscapeSpeedBins,
                                   fDirectFragmentInitialEnergy);

  // 只在有结果时输出一行摘要，避免刷屏
  if (fPrintEventSummaries &&
      (fNNFission > 0 || fNFissionFragments > 0 ||
       fNOuterFuelShellEscapes > 0 ||
       fNEnterShape3 > 0 || fNEnterShape4 > 0)) {
    G4cout
      << "Event " << event->GetEventID()
      << " : nFission = " << fNNFission
      << " : fission fragments = " << fNFissionFragments
      << ", outer coating escapes = " << fNOuterFuelShellEscapes
      << ", entered Shape3 = " << fNEnterShape3
      << ", entered Shape4 = " << fNEnterShape4
      << G4endl;

    if (fNNFission > 0 && !fNFissionProducts.empty() &&
        gPrintedNFissionDetails < kMaxPrintedNFissionDetails) {
      std::ostringstream products;
      G4bool first = true;
      for (const auto& [name, count] : fNFissionProducts) {
        if (!first) products << ", ";
        products << name << " x" << count;
        first = false;
      }

      G4cout
        << "  nFission direct products: " << products.str()
        << G4endl;
      ++gPrintedNFissionDetails;
    }
  }
}

}
