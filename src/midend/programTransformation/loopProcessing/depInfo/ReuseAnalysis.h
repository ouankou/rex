#ifndef REUSE_ANALYSIS_H
#define REUSE_ANALYSIS_H

#include "DepInfo.h"

#include "LoopTransformInterface.h"

#include "PtrSet.h"

// returns the percentage of the array access 'r' being reused at each increase
// of the induction variable due to spatial locality, assuming cache
// line size is 'linesize'; if 'r' is not array access, returns 0
float SelfSpatialReuse(const AstNodePtr &r, const SymbolicVar &induction,
                       unsigned linesize);

// input: ep: a list of DepInfos between references;
//        loopmap: the loop to be placed innermost for each reference;
//        dist: the maximum reuse distance between the innermost loops of two
//        references
//              that the corresponding depInfo in 'ep' must have in order to be
//              included in refSet;
//               if 'dist' == 0 or *'dist' < 0, impose no restraint on reuse
//               distance
// output: refSet: the set of references being reused due to deps in 'ep' within
// distance 'dist'
//         dist: the maximum reuse distance for all references in 'refSet';
// return:  the type of dependences in 'ep' that cause their sink references to
// be reused.
DepType TemporaryReuseRefs(DepInfoConstIterator ep,
                           Map2Object<AstNodePtr, DepDirection, int> &loopmap,
                           CollectObject<AstNodePtr> &refCollect,
                           int *dist = 0);

#endif
