#ifndef B1SteppingAction_h
#define B1SteppingAction_h 

#include "G4UserSteppingAction.hh"
#include "globals.hh"

#include <set>

class G4LogicalVolume;

namespace B1
{

class EventAction;

class SteppingAction : public G4UserSteppingAction
{
  public:
    SteppingAction(EventAction* eventAction);
    ~SteppingAction() override;

    void UserSteppingAction(const G4Step*) override;

  private:
    EventAction* fEventAction = nullptr;
    G4LogicalVolume* fScoringVolume = nullptr;

    // 当前事件号，用于在新事件开始时清空集合
    G4int fCurrentEventID = -1;

    // 去重：同一个 track 只统计一次
    std::set<G4int> fCountedOutwardFuelShellEscapeTracks;
    std::set<G4int> fCountedShape3Tracks;
    std::set<G4int> fCountedShape4Tracks;
};

}

#endif
