// Author: Gergo Barany
#include "sage3basic.h"

#include "rosePublicConfig.h"

#ifdef ROSE_HAVE_PTHREAD_H

#include "AstSharedMemoryParallelSimpleProcessing.h"

// general stuff
AstSharedMemoryParallelProcessingSynchronizationInfo::
    AstSharedMemoryParallelProcessingSynchronizationInfo(
        size_t numberOfThreads, size_t synchronizationWindowSize)
    : synchronizationWindowSize(synchronizationWindowSize),
      ownsResources(true) {
  mutex = new pthread_mutex_t;
  pthread_mutex_init(mutex, NULL);

  synchronizationEvent = new pthread_cond_t;
  pthread_cond_init(synchronizationEvent, NULL);

  workingThreads = new size_t;
  *workingThreads = numberOfThreads;

  synchronizationGeneration = new size_t;
  *synchronizationGeneration = 0;
}

AstSharedMemoryParallelProcessingSynchronizationInfo::
    ~AstSharedMemoryParallelProcessingSynchronizationInfo() {
  if (!ownsResources)
    return;

  pthread_mutex_destroy(mutex);
  delete mutex;

  pthread_cond_destroy(synchronizationEvent);
  delete synchronizationEvent;

  delete workingThreads;
  delete synchronizationGeneration;
}

AstSharedMemoryParallelProcessingSynchronizationInfo::
    AstSharedMemoryParallelProcessingSynchronizationInfo(
        const AstSharedMemoryParallelProcessingSynchronizationInfo &X)
    : mutex(X.mutex), synchronizationEvent(X.synchronizationEvent),
      workingThreads(X.workingThreads),
      synchronizationGeneration(X.synchronizationGeneration),
      synchronizationWindowSize(X.synchronizationWindowSize),
      ownsResources(false) {}

AstSharedMemoryParallelProcessingSynchronizationBase::
    AstSharedMemoryParallelProcessingSynchronizationBase(
        const AstSharedMemoryParallelProcessingSynchronizationInfo &syncInfo)
    : syncInfo(syncInfo), numberOfThreads(*syncInfo.workingThreads) {}

void AstSharedMemoryParallelProcessingSynchronizationBase::synchronize() {
  pthread_mutex_lock(syncInfo.mutex);
  const size_t generation = *syncInfo.synchronizationGeneration;
  *syncInfo.workingThreads -= 1;
  if (*syncInfo.workingThreads == 0) {
    *syncInfo.workingThreads = numberOfThreads;
    ++*syncInfo.synchronizationGeneration;
    pthread_cond_broadcast(syncInfo.synchronizationEvent);
  } else {
    while (*syncInfo.synchronizationGeneration == generation)
      pthread_cond_wait(syncInfo.synchronizationEvent, syncInfo.mutex);
  }
  pthread_mutex_unlock(syncInfo.mutex);
}

// parallel SIMPLE implementation

AstSharedMemoryParallelizableSimpleProcessing::
    AstSharedMemoryParallelizableSimpleProcessing(
        const AstSharedMemoryParallelProcessingSynchronizationInfo &syncInfo,
        const AstSharedMemoryParallelizableSimpleProcessing::TraversalPtrList
            &t)
    : AstCombinedSimpleProcessing(t),
      AstSharedMemoryParallelProcessingSynchronizationBase(syncInfo),
      visitedNodes(0), runningParallelTraversal(false),
      synchronizationWindowSize(syncInfo.synchronizationWindowSize) {}

void AstSharedMemoryParallelizableSimpleProcessing::
    set_runningParallelTraversal(bool val) {
  runningParallelTraversal = val;
}

void AstSharedMemoryParallelizableSimpleProcessing::visit(SgNode *astNode) {
  if (runningParallelTraversal && ++visitedNodes > synchronizationWindowSize) {
    synchronize();
    visitedNodes = 0;
  }

  // let the superclass handle the actual visits
  Superclass::visit(astNode);
}

void AstSharedMemoryParallelizableSimpleProcessing::atTraversalEnd() {
  // delegate call to superclass
  Superclass::atTraversalEnd();

  if (runningParallelTraversal) {
    // clear the flag so subsequent traversals can be non-parallel
    runningParallelTraversal = false;
  }
}

// This class holds the arguments to a traversal thread: The parallelizable
// traversal class that contains a number of traversals, the node to start the
// traversal from, and the traversal order.
struct AstSharedMemoryParallelSimpleThreadArgs {
  AstSharedMemoryParallelizableSimpleProcessing *traversal;
  SgNode *basenode;
  t_traverseOrder treeTraverseOrder;

  AstSharedMemoryParallelSimpleThreadArgs(
      AstSharedMemoryParallelizableSimpleProcessing *traversal,
      SgNode *basenode, t_traverseOrder treeTraverseOrder)
      : traversal(traversal), basenode(basenode),
        treeTraverseOrder(treeTraverseOrder) {}
};

// This is the function that is executed in each thread. It basically unpacks
// the args object and starts the traversal specified in that. Synchronization
// is built into the parallelizable processing classes.
void *parallelSimpleProcessingThread(void *p) {
  AstSharedMemoryParallelSimpleThreadArgs *threadArgs =
      (AstSharedMemoryParallelSimpleThreadArgs *)p;

  AstSharedMemoryParallelizableSimpleProcessing *traversal =
      threadArgs->traversal;
  SgNode *basenode = threadArgs->basenode;
  t_traverseOrder treeTraverseOrder = threadArgs->treeTraverseOrder;
  delete threadArgs;

  // Set the flag that indicates that this is indeed a parallel traversal;
  // it is cleared by the traversal class itself when it is done.
  traversal->set_runningParallelTraversal(true);
  // Start the traversal.
  traversal->traverse(basenode, treeTraverseOrder);

  return NULL;
}

AstSharedMemoryParallelSimpleProcessing::
    AstSharedMemoryParallelSimpleProcessing(int threads)
    : AstCombinedSimpleProcessing(), numberOfThreads(threads),
      synchronizationWindowSize(100000) {
  ROSE_ASSERT(threads > 0);
}

AstSharedMemoryParallelSimpleProcessing::
    AstSharedMemoryParallelSimpleProcessing(
        const AstSharedMemoryParallelSimpleProcessing::TraversalPtrList &t,
        int threads)
    : AstCombinedSimpleProcessing(t), numberOfThreads(threads),
      synchronizationWindowSize(100000) {
  ROSE_ASSERT(threads > 0);
}

void AstSharedMemoryParallelSimpleProcessing::traverseInParallel(
    SgNode *basenode, t_traverseOrder treeTraverseOrder) {
  size_t numberOfTraversals = traversals.size();
  size_t i;

  AstSharedMemoryParallelProcessingSynchronizationInfo syncInfo(
      numberOfThreads, synchronizationWindowSize);

  ParallelizableTraversalPtrList parallelTraversals(numberOfThreads);
  size_t begin = 0, end;
  for (i = 0; i < numberOfThreads; i++) {
    end = begin + numberOfTraversals / numberOfThreads +
          (i < numberOfTraversals % numberOfThreads);

    parallelTraversals[i] = new AstSharedMemoryParallelizableSimpleProcessing(
        syncInfo, std::vector<TraversalPtr>(traversals.begin() + begin,
                                            traversals.begin() + end));
    begin = end;
  }

  pthread_t *threads = new pthread_t[numberOfThreads];
  for (i = 0; i < numberOfThreads; i++) {
    const int status =
        pthread_create(&threads[i], NULL, parallelSimpleProcessingThread,
                       new AstSharedMemoryParallelSimpleThreadArgs(
                           parallelTraversals[i], basenode, treeTraverseOrder));
    ROSE_ASSERT(status == 0);
  }

  for (i = 0; i < numberOfThreads; i++) {
    const int status = pthread_join(threads[i], nullptr);
    ROSE_ASSERT(status == 0);
  }
  delete[] threads;
  for (i = 0; i < numberOfThreads; i++)
    delete parallelTraversals[i];
}

// parallel PRE POST implementation

AstSharedMemoryParallelizablePrePostProcessing::
    AstSharedMemoryParallelizablePrePostProcessing(
        const AstSharedMemoryParallelProcessingSynchronizationInfo &syncInfo,
        const AstSharedMemoryParallelizablePrePostProcessing::TraversalPtrList
            &t)
    : AstCombinedPrePostProcessing(t),
      AstSharedMemoryParallelProcessingSynchronizationBase(syncInfo),
      visitedNodes(0), runningParallelTraversal(false),
      synchronizationWindowSize(syncInfo.synchronizationWindowSize) {}

void AstSharedMemoryParallelizablePrePostProcessing::
    set_runningParallelTraversal(bool val) {
  runningParallelTraversal = val;
}

void AstSharedMemoryParallelizablePrePostProcessing::preOrderVisit(
    SgNode *astNode) {
  if (runningParallelTraversal && ++visitedNodes > synchronizationWindowSize) {
    synchronize();
    visitedNodes = 0;
  }

  // let the superclass handle the actual visits
  Superclass::preOrderVisit(astNode);
}

void AstSharedMemoryParallelizablePrePostProcessing::atTraversalEnd() {
  // delegate call to superclass
  Superclass::atTraversalEnd();

  if (runningParallelTraversal) {
    // clear the flag so subsequent traversals can be non-parallel
    runningParallelTraversal = false;
  }
}

// This class holds the arguments to a traversal thread: The parallelizable
// traversal class that contains a number of traversals and the node to start
// the traversal from.
struct AstSharedMemoryParallelPrePostThreadArgs {
  AstSharedMemoryParallelizablePrePostProcessing *traversal;
  SgNode *basenode;

  AstSharedMemoryParallelPrePostThreadArgs(
      AstSharedMemoryParallelizablePrePostProcessing *traversal,
      SgNode *basenode)
      : traversal(traversal), basenode(basenode) {}
};

// This is the function that is executed in each thread. It basically unpacks
// the args object and starts the traversal specified in that. Synchronization
// is built into the parallelizable processing classes.
void *parallelPrePostProcessingThread(void *p) {
  AstSharedMemoryParallelPrePostThreadArgs *threadArgs =
      (AstSharedMemoryParallelPrePostThreadArgs *)p;

  AstSharedMemoryParallelizablePrePostProcessing *traversal =
      threadArgs->traversal;
  SgNode *basenode = threadArgs->basenode;
  delete threadArgs;

  // Set the flag that indicates that this is indeed a parallel traversal;
  // it is cleared by the traversal class itself when it is done.
  traversal->set_runningParallelTraversal(true);
  // Start the traversal.
  traversal->traverse(basenode);

  return NULL;
}

AstSharedMemoryParallelPrePostProcessing::
    AstSharedMemoryParallelPrePostProcessing(int threads)
    : AstCombinedPrePostProcessing(), numberOfThreads(threads),
      synchronizationWindowSize(100000) {
  ROSE_ASSERT(threads > 0);
}

AstSharedMemoryParallelPrePostProcessing::
    AstSharedMemoryParallelPrePostProcessing(
        const AstSharedMemoryParallelPrePostProcessing::TraversalPtrList &t,
        int threads)
    : AstCombinedPrePostProcessing(t), numberOfThreads(threads),
      synchronizationWindowSize(100000) {
  ROSE_ASSERT(threads > 0);
}

void AstSharedMemoryParallelPrePostProcessing::traverseInParallel(
    SgNode *basenode) {
  size_t numberOfTraversals = traversals.size();
  size_t i;

  AstSharedMemoryParallelProcessingSynchronizationInfo syncInfo(
      numberOfThreads, synchronizationWindowSize);

  ParallelizableTraversalPtrList parallelTraversals(numberOfThreads);
  size_t begin = 0, end;
  for (i = 0; i < numberOfThreads; i++) {
    end = begin + numberOfTraversals / numberOfThreads +
          (i < numberOfTraversals % numberOfThreads);

    parallelTraversals[i] = new AstSharedMemoryParallelizablePrePostProcessing(
        syncInfo, std::vector<TraversalPtr>(traversals.begin() + begin,
                                            traversals.begin() + end));
    begin = end;
  }

  pthread_t *threads = new pthread_t[numberOfThreads];
  for (i = 0; i < numberOfThreads; i++) {
    const int status =
        pthread_create(&threads[i], NULL, parallelPrePostProcessingThread,
                       new AstSharedMemoryParallelPrePostThreadArgs(
                           parallelTraversals[i], basenode));
    ROSE_ASSERT(status == 0);
  }

  for (i = 0; i < numberOfThreads; i++) {
    const int status = pthread_join(threads[i], nullptr);
    ROSE_ASSERT(status == 0);
  }
  delete[] threads;
  for (i = 0; i < numberOfThreads; i++)
    delete parallelTraversals[i];
}

#endif
