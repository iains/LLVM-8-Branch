//===- LoopInfo.cpp - Natural Loop Calculator -------------------------------=//
//
// This file defines the LoopInfo class that is used to identify natural loops
// and determine the loop depth of various nodes of the CFG.  Note that the
// loops identified may actually be several natural loops that share the same
// header node... not just a single natural loop.
//
//===----------------------------------------------------------------------===//

#include "llvm/Analysis/LoopInfo.h"
#include "llvm/Analysis/Dominators.h"
#include "llvm/Support/CFG.h"
#include "llvm/Assembly/Writer.h"
#include "Support/DepthFirstIterator.h"
#include <algorithm>

static RegisterAnalysis<LoopInfo>
X("loops", "Natural Loop Construction", true);

//===----------------------------------------------------------------------===//
// Loop implementation
//
bool Loop::contains(const BasicBlock *BB) const {
  return find(Blocks.begin(), Blocks.end(), BB) != Blocks.end();
}

void Loop::print(std::ostream &OS) const {
  OS << std::string(getLoopDepth()*2, ' ') << "Loop Containing: ";

  for (unsigned i = 0; i < getBlocks().size(); ++i) {
    if (i) OS << ",";
    WriteAsOperand(OS, (const Value*)getBlocks()[i]);
  }
  OS << "\n";

  std::copy(getSubLoops().begin(), getSubLoops().end(),
            std::ostream_iterator<const Loop*>(OS, "\n"));
}

//===----------------------------------------------------------------------===//
// LoopInfo implementation
//
void LoopInfo::stub() {}

bool LoopInfo::runOnFunction(Function &) {
  releaseMemory();
  Calculate(getAnalysis<DominatorSet>());    // Update
  return false;
}

void LoopInfo::releaseMemory() {
  for (std::vector<Loop*>::iterator I = TopLevelLoops.begin(),
         E = TopLevelLoops.end(); I != E; ++I)
    delete *I;   // Delete all of the loops...

  BBMap.clear();                             // Reset internal state of analysis
  TopLevelLoops.clear();
}


void LoopInfo::Calculate(const DominatorSet &DS) {
  BasicBlock *RootNode = DS.getRoot();

  for (df_iterator<BasicBlock*> NI = df_begin(RootNode),
	 NE = df_end(RootNode); NI != NE; ++NI)
    if (Loop *L = ConsiderForLoop(*NI, DS))
      TopLevelLoops.push_back(L);

  for (unsigned i = 0; i < TopLevelLoops.size(); ++i)
    TopLevelLoops[i]->setLoopDepth(1);
}

void LoopInfo::getAnalysisUsage(AnalysisUsage &AU) const {
  AU.setPreservesAll();
  AU.addRequired<DominatorSet>();
}

void LoopInfo::print(std::ostream &OS) const {
  std::copy(getTopLevelLoops().begin(), getTopLevelLoops().end(),
            std::ostream_iterator<const Loop*>(OS, "\n"));
}

Loop *LoopInfo::ConsiderForLoop(BasicBlock *BB, const DominatorSet &DS) {
  if (BBMap.find(BB) != BBMap.end()) return 0;   // Havn't processed this node?

  std::vector<BasicBlock *> TodoStack;

  // Scan the predecessors of BB, checking to see if BB dominates any of
  // them.
  for (pred_iterator I = pred_begin(BB), E = pred_end(BB); I != E; ++I)
    if (DS.dominates(BB, *I))   // If BB dominates it's predecessor...
      TodoStack.push_back(*I);

  if (TodoStack.empty()) return 0;  // Doesn't dominate any predecessors...

  // Create a new loop to represent this basic block...
  Loop *L = new Loop(BB);
  BBMap[BB] = L;

  while (!TodoStack.empty()) {  // Process all the nodes in the loop
    BasicBlock *X = TodoStack.back();
    TodoStack.pop_back();

    if (!L->contains(X)) {                  // As of yet unprocessed??
      L->Blocks.push_back(X);

      // Add all of the predecessors of X to the end of the work stack...
      TodoStack.insert(TodoStack.end(), pred_begin(X), pred_end(X));
    }
  }

  // Add the basic blocks that comprise this loop to the BBMap so that this
  // loop can be found for them.  Also check subsidary basic blocks to see if
  // they start subloops of their own.
  //
  for (std::vector<BasicBlock*>::reverse_iterator I = L->Blocks.rbegin(),
	 E = L->Blocks.rend(); I != E; ++I) {

    // Check to see if this block starts a new loop
    if (Loop *NewLoop = ConsiderForLoop(*I, DS)) {
      L->SubLoops.push_back(NewLoop);
      NewLoop->ParentLoop = L;
    }
  
    if (BBMap.find(*I) == BBMap.end())
      BBMap.insert(std::make_pair(*I, L));
  }

  return L;
}
