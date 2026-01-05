#ifndef ROSE_MIDEND_ASTPROCESSING_GRAPHPROCESSING_H
#define ROSE_MIDEND_ASTPROCESSING_GRAPHPROCESSING_H

#define LP 1
#define PERFDEBUG 0
#ifdef _OPENMP
#include <omp.h>
#endif
#include <algorithm>
#include <cassert>
#include <fstream>
#include <map>
#include <mlog.h>
#include <set>
#include <sstream>
#include <string>
#include <tuple>
#include <unordered_set>
#include <utility>
#include <vector>

#include <staticCFG.h>

/**
*@file graphProcessing.h

*Brief Overview of Algorithm:

***********************
*Current Implementation
***********************

*This implementation uses the graph structure to analyze the paths of the graph

*The path analyzer sends the user paths to be evaluated by the "analyzePath"
function that is user defined

**************************
*Further Improvements: TODO
**************************

@todo utilize visitors to take advantage of graph structure abilities

***************
*Contact Info
***************

*Finally, blame can be assigned to and questions can be forwarded to the author,
though response is not guaranteed

*if I'm still at Lawrence
*hoffman34 AT llnl DOT gov
*@author Michael Hoffman
*/
#include <sys/resource.h>
#include <sys/time.h>

template <class CFG> inline auto vertices(const CFG &g) { return g.vertices(); }

template <class CFG> inline auto edges(const CFG &g) { return g.edges(); }

template <class CFG>
inline auto out_edges(typename CFG::vertex_descriptor v, const CFG &g) {
  return g.out_edges(v);
}

template <class CFG>
inline auto in_edges(typename CFG::vertex_descriptor v, const CFG &g) {
  return g.in_edges(v);
}

template <class CFG>
inline auto source(typename CFG::edge_descriptor e, const CFG &g) {
  return g.source(e);
}

template <class CFG>
inline auto target(typename CFG::edge_descriptor e, const CFG &g) {
  return g.target(e);
}

template <class CFG> inline auto add_vertex(CFG &g) { return g.add_vertex(); }

template <class CFG>
inline auto add_edge(typename CFG::vertex_descriptor u,
                     typename CFG::vertex_descriptor v, CFG &g) {
  return g.add_edge(u, v);
}

template <class CFG> class SgGraphTraversal {
public:
  using Vertex = typename CFG::vertex_descriptor;
  using Edge = typename CFG::edge_descriptor;

  void constructPathAnalyzer(CFG *g, bool unbounded = false, Vertex end = 0,
                             Vertex begin = 0, bool ns = true);
  virtual void analyzePath(std::vector<Vertex> &pth) = 0;
  std::vector<int> getInEdges(int &node, CFG *&g);
  std::vector<int> getOutEdges(int &node, CFG *&g);
  int getTarget(int &n, CFG *&g);
  int getSource(int &n, CFG *&g);
  std::map<Vertex, int> vertintmap;
  std::map<Edge, int> edgeintmap;
  std::map<int, Vertex> intvertmap;
  std::map<int, Edge> intedgemap;
  SgGraphTraversal();
  virtual ~SgGraphTraversal();
  SgGraphTraversal(SgGraphTraversal &);
  SgGraphTraversal &operator=(SgGraphTraversal &);
  int pathnum;

  void firstPrepGraph(CFG *&g);

private:
  int normals;
  int abnormals;
  bool needssafety;
  int recursed;
  int checkedfound;
  void prepareGraph(CFG *&g);
  void findClosuresAndMarkersAndEnumerate(CFG *&g);
  int stoppedpaths;
  std::set<std::vector<int>> traversePath(int begin, int end, CFG *&g,
                                          bool loop = false);
  std::set<std::vector<int>>
  uTraversePath(int begin, int end, CFG *&g, bool loop,
                std::map<int, std::vector<std::vector<int>>> &localLoops);
  std::vector<std::vector<int>> bfsTraversePath(int begin, int end, CFG *&g,
                                                bool loop = false);
  void expandBfsFrontier(int begin, int end, CFG *&g, bool recursedloop,
                         std::vector<std::vector<int>> &paths,
                         std::vector<int> &localLoops,
                         std::map<int, std::vector<std::vector<int>>> &ptp,
                         const std::unordered_set<int> &completed_loops_set,
                         const std::unordered_set<int> &recurses_lookup);
  std::vector<std::vector<int>>
  mergePathSegments(const std::vector<std::vector<int>> &paths,
                    const std::map<int, std::vector<std::vector<int>>> &ptp,
                    int begin);
  void collectGlobalLoopPaths(
      const std::vector<int> &localLoops, CFG *&g,
      std::map<int, std::vector<std::vector<int>>> &globalLoopPaths,
      std::vector<int> &completedLoops,
      std::unordered_set<int> &completed_loops_set);
  void collectLocalLoopsForPath(
      const std::vector<int> &path,
      const std::map<int, std::vector<std::vector<int>>> &globalLoopPaths,
      std::map<int, std::vector<std::vector<int>>> &localLoops,
      std::vector<int> &perms, std::vector<unsigned int> &qs, int &permnums);
  std::vector<std::vector<int>> buildLoopPermutations(
      const std::vector<int> &path,
      std::map<int, std::vector<std::vector<int>>> &localLoops,
      const std::vector<unsigned int> &qs, const std::vector<int> &perms,
      int permnums);
  std::vector<int> unzipPath(std::vector<int> &path, CFG *&g, int start,
                             int end);
  std::vector<int> zipPath(std::vector<int> &path, CFG *&g, int start, int end);
  std::vector<int> zipPath2(std::vector<int> &path, CFG *&g);
  void printCFGNode(int &cf, std::ofstream &o);
  void printCFGNodeGeneric(int &cf, std::string prop, std::ofstream &o);
  void printCFGEdge(int &cf, CFG *&cfg, std::ofstream &o);
  void printHotness(CFG *&g);
  void printPathDot(CFG *&g);
  void computeOrder(CFG *&g, const int &begin);
  void computeSubGraphs(const int &begin, const int &end, CFG *&g,
                        int depthDifferential);
  std::vector<int> sources;
  std::vector<int> sinks;
  std::vector<int> recursiveLoops;
  std::vector<int> recurses;
  std::map<int, int> ptsNum;
  bool borrowed;
  std::set<int> badloop;
  std::map<int, std::vector<std::vector<int>>> totalLoops;
  std::map<int, std::string> nodeStrings;
  int sourcenum;
  unsigned long long evaledpaths;
  int badpaths;
  int workingthreadnum;
  bool workingthread;
  std::map<int, std::set<std::vector<int>>> loopStore;
  std::vector<std::vector<int>> pathStore;
  std::map<int, std::vector<int>> subpathglobal;
  std::map<std::vector<int>, int> subpathglobalinv;
  int nextsubpath;
  std::vector<int> orderOfNodes;
  std::vector<std::map<Vertex, Vertex>> SubGraphGraphMap;
  std::vector<std::map<Vertex, Vertex>> GraphSubGraphMap;
  std::vector<CFG *> subGraphVector;
  void getVertexPath(std::vector<int> path, CFG *&g,
                     std::vector<Vertex> &vertexPath);
  void storeCompact(std::vector<int> path);
  int nextNode;
  int nextEdge;
  std::vector<int> markers;
  std::vector<int> closures;
  std::map<int, int> markerIndex;
  std::map<int, std::vector<int>> pathsAtMarkers;
  using vertex_iterator = typename CFG::vertex_iterator;
  using out_edge_iterator = typename CFG::out_edge_iterator;
  using in_edge_iterator = typename CFG::in_edge_iterator;
  using edge_iterator = typename CFG::edge_iterator;
  bool bound;
};

template <class CFG> SgGraphTraversal<CFG>::SgGraphTraversal() {}

template <class CFG>
SgGraphTraversal<CFG> &
SgGraphTraversal<CFG>::operator=(SgGraphTraversal &other) {
  return *this;
}

#ifndef SWIG

template <class CFG> SgGraphTraversal<CFG>::~SgGraphTraversal() {}

#endif

/**
    Gets the source of an edge
    SgGraphTraversal::getSource
    Input:
    @param[edge] int& integer representation of edge in question
    @param[g] CFG*& the CFG used
*/
template <class CFG>
inline int SgGraphTraversal<CFG>::getSource(int &edge, CFG *&g) {
  Edge e = intedgemap[edge];
  Vertex v = source(e, *g);
  return (vertintmap[v]);
}

/**
    Gets the target of an edge
    SgGraphTraversal::getTarget
    Input:
    @param[edge] int& integer representation of edge in quesution
    @param[g] the CFG*& CFG used
*/

template <class CFG>
inline int SgGraphTraversal<CFG>::getTarget(int &edge, CFG *&g) {
  Edge e = intedgemap[edge];
  Vertex v = target(e, *g);
  return (vertintmap[v]);
}

/**
Gets out edges with integer inputs, internal use only
SgGraphTraversal::getInEdges
Input:
@param[node] int, integer representation of the node to get the in edges from
@param[g] CFG* g, CFG
*/

template <class CFG>
std::vector<int> SgGraphTraversal<CFG>::getInEdges(int &node, CFG *&g) {
  Vertex getIns = intvertmap[node];
  std::vector<int> inedges;
  // DQ (4/11/2017): Fix Klockworks issue of uninitialized variables.
#if 1
  in_edge_iterator i, j;
#else
  // This does not compile.
  in_edge_iterator i = inedges.begin();
  in_edge_iterator j = i;
#endif
  for (std::tie(i, j) = in_edges(getIns, *g); i != j; ++i) {
    inedges.push_back(edgeintmap[*i]);
  }
  return inedges;
}

/**
Gets out edges with integer inputs, internal use only
SgGraphTraversal::getOutEdges
Input:
@param[node] int, integer representation of the node to get the out edges from
@param[g] CFG* g, CFG
*/

template <class CFG>
std::vector<int> SgGraphTraversal<CFG>::getOutEdges(int &node, CFG *&g) {
  Vertex getOuts = intvertmap[node];
  std::vector<int> outedges;
  // DQ (4/11/2017): Fix Klockworks issue of uninitialized variables.
#if 1
  out_edge_iterator i, j;
#else
  // This does not compile.
  out_edge_iterator i = outedges.begin();
  out_edge_iterator j = i;
#endif
  for (std::tie(i, j) = out_edges(getOuts, *g); i != j; ++i) {
    outedges.push_back(edgeintmap[*i]);
  }
  return outedges;
}

/**
Condenses paths, currently deprecated...
Input:
@param[pth] std::vector<int> the original path
@param[g] CFG*, the ambient graph
Output:
zipped path
*/

template <class CFG>
inline std::vector<int> SgGraphTraversal<CFG>::zipPath2(std::vector<int> &pth,
                                                        CFG *&g) {
  std::vector<int> npth;
  npth.push_back(pth[0]);
  for (int i = 1; i < pth.size() - 1; i++) {
    if (find(closures.begin(), closures.end(), pth[i]) != closures.end()) {
      npth.push_back(pth[i]);
    }
  }
  npth.push_back(pth.back());
  return npth;
}

/**
Condenses paths to simply the first and last node and the ordered set of edges
taken at nodes with more than 1 outedge
Input:
@param[pth] std::vector<int>, the original path
@param[g] CFG*, the ambient graph
@param[start] integer representation of the first node
@param[end] integer representation of the last node
*/

template <class CFG>
std::vector<int> SgGraphTraversal<CFG>::zipPath(std::vector<int> &pth, CFG *&g,
                                                int start, int end) {
  std::vector<int> subpath;
  std::vector<int> movepath;
  movepath.push_back(pth.front());
  movepath.push_back(pth.back());
  for (unsigned int qw = 0; qw < pth.size() - 1; qw++) {
    if (find(markers.begin(), markers.end(), pth[qw]) != markers.end()) {
      std::vector<int> oeds = getOutEdges(pth[qw], g);
      for (unsigned int i = 0; i < oeds.size(); i++) {
        if (getTarget(oeds[i], g) == pth[qw + 1]) {
          movepath.push_back(oeds[i]);
        }
      }
    }
  }
  return movepath;
}

/**
unzips the paths zipped by zipPath
Input:
@param[pzipped] the zipped path
@param[CFG] the ambient graph
@param[start] the integer representation of the first node (used to check that
zipPath is working correctly)
@param[end] the integer representation of the end node
*/

template <class CFG>
std::vector<int> SgGraphTraversal<CFG>::unzipPath(std::vector<int> &pzipped,
                                                  CFG *&g, int start, int end) {
  ROSE_ASSERT(pzipped[0] == start && (pzipped[1] == end || end == -1));
  std::vector<int> zipped;
  for (unsigned int i = 2; i < pzipped.size(); i++) {
    zipped.push_back(pzipped[i]);
  }
  std::vector<int> unzipped;
  unzipped.push_back(start);
  std::vector<int> oeds = getOutEdges(start, g);
  if (oeds.size() == 0) {
    return unzipped;
  }
  for (unsigned int i = 0; i < zipped.size(); i++) {
    oeds = getOutEdges(unzipped.back(), g);
    while (oeds.size() == 1) {
      if (getTarget(oeds[0], g) == end && unzipped.size() != 1) {
        unzipped.push_back(end);
        return unzipped;
      }
      unzipped.push_back(getTarget(oeds[0], g));
      oeds = getOutEdges(unzipped.back(), g);
    }
    if (oeds.size() == 0) {
      return unzipped;
    }
    if (oeds.size() > 1 && (unzipped.back() != end ||
                            (unzipped.size() == 1 && unzipped.back() == end))) {
      ROSE_ASSERT(getSource(zipped[i], g) == unzipped.back());
      unzipped.push_back(getTarget(zipped[i], g));
    }
  }
  std::vector<int> oeds2 = getOutEdges(unzipped.back(), g);
  if (unzipped.back() != end && oeds2.size() != 0) {
    while (oeds2.size() == 1 && unzipped.back() != end) {
      unzipped.push_back(getTarget(oeds2[0], g));
      oeds2 = getOutEdges(unzipped.back(), g);
    }
  }
  return unzipped;
}

template <class CFG>
void SgGraphTraversal<CFG>::expandBfsFrontier(
    int begin, int end, CFG *&g, bool recursedloop,
    std::vector<std::vector<int>> &paths, std::vector<int> &localLoops,
    std::map<int, std::vector<std::vector<int>>> &ptp,
    const std::unordered_set<int> &completed_loops_set,
    const std::unordered_set<int> &recurses_lookup) {
  std::set<int> nodes;
  std::vector<std::vector<int>> pathContainer;
  std::vector<int> bgpath;
  bgpath.push_back(begin);
  pathContainer.push_back(bgpath);
  std::vector<std::vector<int>> newPathContainer;
  while (!pathContainer.empty()) {
    // iterating through the currently discovered subpaths to build them up
    for (unsigned int i = 0; i < pathContainer.size(); i++) {
      std::vector<int> npth = pathContainer[i];
      std::vector<int> oeds = getOutEdges(npth.back(), g);
      std::vector<int> ieds = getInEdges(npth.back(), g);

      npth = pathContainer[i];
      oeds = getOutEdges(npth.back(), g);
      std::unordered_set<int> path_nodes(npth.begin(), npth.end());

      if ((!recursedloop &&
           ((bound && npth.back() == end && npth.size() != 1) ||
            (!bound && oeds.size() == 0))) ||
          (recursedloop && npth.back() == end && npth.size() != 1)) {
        std::vector<int> newpth;
        newpth = (pathContainer[i]);
        std::vector<int> movepath = newpth;
        if (recursedloop && newpth.back() == end && newpth.size() != 1) {
          paths.push_back(movepath);
        } else if (!recursedloop) {
          if (bound && newpth.size() != 1 && newpth.back() == end) {
            paths.push_back(movepath);
          } else if (!bound) {
            paths.push_back(movepath);
          }
        }

      } else {
        std::vector<int> oeds = getOutEdges(pathContainer[i].back(), g);

        for (unsigned int j = 0; j < oeds.size(); j++) {

          int tg = getTarget(oeds[j], g);

          std::vector<int> newpath = (pathContainer[i]);
          // we split up paths into pieces so that they don't take up a lot of
          // memory, basically this is when we run into a path more than once,
          // so we attach all paths that go to that path to that particular node
          // via ptp
          if (nodes.find(tg) != nodes.end() &&
              path_nodes.find(tg) == path_nodes.end() && tg != end) {
            if (ptp.find(tg) == ptp.end()) {
              std::vector<int> nv;
              nv.push_back(tg);
              newPathContainer.push_back(nv);
              ptp[tg].push_back(newpath);
            } else {
              ptp[tg].push_back(newpath);
            }
          } else if (path_nodes.find(getTarget(oeds[j], g)) ==
                         path_nodes.end() ||
                     getTarget(oeds[j], g) == end) {
            newpath.push_back(tg);
            std::vector<int> ieds = getInEdges(tg, g);
            if (ieds.size() > 1) { // find(closures.begin(), closures.end(), tg)
              nodes.insert(tg);
            }
            newPathContainer.push_back(newpath);
          } else if (tg == end && recursedloop) {
            newpath.push_back(tg);
            newPathContainer.push_back(newpath);
          } else {
            std::vector<int> ieds = getInEdges(tg, g);
            if (ieds.size() > 1 &&
                completed_loops_set.find(tg) == completed_loops_set.end() &&
                recurses_lookup.find(tg) == recurses_lookup.end()) {
              localLoops.push_back(tg);
              nodes.insert(tg);
            }
          }
        }
      }
    }
    pathContainer = newPathContainer;
    newPathContainer.clear();
  }
}

template <class CFG>
std::vector<std::vector<int>> SgGraphTraversal<CFG>::mergePathSegments(
    const std::vector<std::vector<int>> &paths,
    const std::map<int, std::vector<std::vector<int>>> &ptp, int begin) {
  std::vector<std::vector<int>> working_paths = paths;
  std::vector<std::vector<int>> finnpts;
  std::vector<std::vector<int>> npts;
  while (true) {
    if (working_paths.size() > 1000000) {
      MLOG_ERROR_C("graphProcessing", "Too many paths; consider a subgraph.\n");
      ROSE_ABORT();
    }
    for (unsigned int qq = 0; qq < working_paths.size(); qq++) {
      std::vector<int> pq = working_paths[qq];
      std::unordered_set<int> pq_lookup(pq.begin(), pq.end());
      std::vector<int> qp;
      int ppf = working_paths[qq].front();
      if (ptp.find(ppf) != ptp.end()) {
        for (unsigned int kk = 0; kk < ptp.find(ppf)->second.size(); kk++) {
          std::vector<int> newpath = ptp.find(ppf)->second[kk];
          bool good = true;
          if (newpath.back() == newpath.front() && newpath.front() != begin &&
              newpath.size() > 1) {
            good = false;
          } else {

            for (unsigned int kk1 = 0; kk1 < newpath.size(); kk1++) {
              if (pq_lookup.find(newpath[kk1]) != pq_lookup.end() &&
                  newpath[kk1] != begin) {
                good = false;
                break;
              }
            }
          }
          if (good) {
            newpath.insert(newpath.end(), pq.begin(), pq.end());
#pragma omp critical
            {
              npts.push_back(newpath);
            }
          }
        }
      } else {
        std::vector<int> ppq = pq;
#pragma omp critical
        {
          finnpts.push_back(ppq);
        }
      }
    }
    if (npts.size() == 0) {
      break;
    } else {
      working_paths = npts;
      npts.clear();
    }
  }
  return finnpts;
}

template <class CFG>
void SgGraphTraversal<CFG>::collectGlobalLoopPaths(
    const std::vector<int> &localLoops, CFG *&g,
    std::map<int, std::vector<std::vector<int>>> &globalLoopPaths,
    std::vector<int> &completedLoops,
    std::unordered_set<int> &completed_loops_set) {
  for (unsigned int k = 0; k < localLoops.size(); k++) {
    int lk = localLoops[k];
    std::vector<std::vector<int>> loopp;
    if (loopStore.find(localLoops[k]) != loopStore.end()) {
      loopp.insert(loopp.end(), loopStore[localLoops[k]].begin(),
                   loopStore[localLoops[k]].end());
    } else {
      std::map<int, std::vector<std::vector<int>>> localLoopPaths;
      completedLoops.push_back(lk);
      completed_loops_set.insert(lk);
      recurses.push_back(lk);
      loopp = bfsTraversePath(lk, lk, g, true);
      recurses.pop_back();
    }
    for (unsigned int ik = 0; ik < loopp.size(); ik++) {

      if (find(globalLoopPaths[lk].begin(), globalLoopPaths[lk].end(),
               loopp[ik]) == globalLoopPaths[lk].end()) {
        globalLoopPaths[localLoops[k]].push_back(loopp[ik]);
      }
    }
  }
}

/**
The function responsible for collecting all paths without loops, and all paths
within lops that do not include other loops then sending those to uTraverse to
assemble them into all paths with any combination of loops Input:
@param[begin] integer representation of the first node
@param[end] integer representation of the last node (or -1 if its not bounded)
@param[g] CFG*, the ambient CFG
@param[loop] boolean expressing whether or not we are calculating paths
contained within a loop
*/

template <class CFG>
std::vector<std::vector<int>>
SgGraphTraversal<CFG>::bfsTraversePath(int begin, int end, CFG *&g, bool loop) {
  bool recursedloop = loop;
  std::map<int, std::vector<std::vector<int>>> PtP;
  std::vector<std::vector<int>> paths;
  std::vector<int> localLoops;
  std::map<int, std::vector<std::vector<int>>> globalLoopPaths;
  std::vector<int> completedLoops;
  std::unordered_set<int> completed_loops_set;
  std::unordered_set<int> recurses_lookup(recurses.begin(), recurses.end());
  expandBfsFrontier(begin, end, g, recursedloop, paths, localLoops, PtP,
                    completed_loops_set, recurses_lookup);
  paths = mergePathSegments(paths, PtP, begin);
  collectGlobalLoopPaths(localLoops, g, globalLoopPaths, completedLoops,
                         completed_loops_set);
  borrowed = true;
  std::vector<std::vector<int>> lps2;

  pathStore = paths;
  paths.clear();
  if (!recursedloop) {
    uTraversePath(begin, end, g, false, globalLoopPaths);
  } else {
    recursed++;

    std::set<std::vector<int>> lps =
        uTraversePath(begin, end, g, true, globalLoopPaths);
    recursed--;
    for (std::set<std::vector<int>>::iterator ij = lps.begin(); ij != lps.end();
         ij++) {
      std::vector<int> ijk = (*ij);

      lps2.push_back(*ij);
    }
  }
  return lps2;
}

template <class CFG>
void SgGraphTraversal<CFG>::collectLocalLoopsForPath(
    const std::vector<int> &path,
    const std::map<int, std::vector<std::vector<int>>> &globalLoopPaths,
    std::map<int, std::vector<std::vector<int>>> &localLoops,
    std::vector<int> &perms, std::vector<unsigned int> &qs, int &permnums) {
  std::vector<int> takenLoops;
  takenLoops.push_back(path[0]);
  int lost = 0;
  for (unsigned int q = 1; q < path.size() - 1; q++) {
    auto it = globalLoopPaths.find(path[q]);
    if (it != globalLoopPaths.end() && !it->second.empty()) {
      std::unordered_set<int> taken_lookup(takenLoops.begin(),
                                           takenLoops.end());
      for (unsigned int qp1 = 0; qp1 < it->second.size(); qp1++) {
        const std::vector<int> &gp = it->second[qp1];
        bool taken = false;
        for (int node : gp) {
          if (taken_lookup.count(node) != 0U) {
            taken = true;
            break;
          }
        }

        if (!taken) {
          localLoops[path[q]].push_back(gp);
        } else {
          lost++;
          taken = false;
        }
      }
      if (localLoops[path[q]].size() != 0) {
        takenLoops.push_back(path[q]);
        permnums *= (localLoops[path[q]].size() + 1);
        perms.push_back(permnums);
        qs.push_back(path[q]);
      }
    }
  }
}

template <class CFG>
std::vector<std::vector<int>> SgGraphTraversal<CFG>::buildLoopPermutations(
    const std::vector<int> &path,
    std::map<int, std::vector<std::vector<int>>> &localLoops,
    const std::vector<unsigned int> &qs, const std::vector<int> &perms,
    int permnums) {
  std::set<std::vector<int>> movepathscheck;
  std::vector<int> nvec;
  std::vector<std::vector<int>> boxpaths(permnums, nvec);
  for (int i = 1; i <= permnums; i++) {
    std::vector<int> loopsTaken;
    unsigned int j = 0;
    std::vector<int> npath;
    while (true) {
      if (j == perms.size() || perms[j] > i) {
        break;
      } else {
        j++;
      }
    }
    int pn = i;
    std::vector<int> pL;
    for (unsigned int j1 = 0; j1 <= j; j1++) {
      pL.push_back(-1);
    }
    for (unsigned int k = j; k > 0; k--) {
      int l = 1;
      while (perms[k - 1] * l < pn) {
        l++;
      }
      pL[k] = l - 2;
      pn -= (perms[k - 1] * (l - 1));
    }
    pL[0] = pn - 2;

    unsigned int q2 = 0;
    for (unsigned int q1 = 0; q1 < path.size(); q1++) {
      if (q2 < qs.size()) {
        if (qs.size() != 0 && (unsigned)path[q1] == qs[q2] &&
            (size_t)q2 != pL.size()) {
          if (pL[q2] == -1) {
            npath.push_back(path[q1]);
          } else {
            npath.insert(npath.end(), localLoops[path[q1]][pL[q2]].begin(),
                         localLoops[path[q1]][pL[q2]].end());
          }
          q2++;
        } else {
          npath.push_back(path[q1]);
        }
      } else {
        npath.push_back(path[q1]);
      }
    }
    boxpaths[i - 1] = npath;
  }
  return boxpaths;
}

/**
This function calculates all the permutations of loops on paths
it also throws away duplicate paths
Input:
@param[begin] integer representation of first node
@param[end] integer representation of the final node
@param[g] ambient CFG
@param[globalLoopPaths] connects an integer representation of a node to all
possible loops starting at that node
*/

template <class CFG>
std::set<std::vector<int>> SgGraphTraversal<CFG>::uTraversePath(
    int begin, int end, CFG *&g, bool loop,
    std::map<int, std::vector<std::vector<int>>> &globalLoopPaths) {
  int newmil = 1;
  std::set<std::vector<int>> newpaths;
  std::set<std::vector<int>> npaths;
  pathnum = 0;
  std::vector<int> path;
  std::vector<std::vector<int>> paths;
  std::vector<std::vector<int>> checkpaths;
  std::vector<std::vector<int>> npathchecker;
  std::map<int, int> currents;
  std::set<std::vector<int>> loopPaths;
  bool done = false;
  std::set<std::vector<int>> fts;
  while (true) {
    if (paths.size() > 1000000) {
      MLOG_WARN_C("graphProcessing",
                  "Nearly 1 million paths with no loops; stopping early.\n");
      return loopPaths;
    }
    if (done || borrowed) {

      if (borrowed) {
        paths = pathStore;
        pathStore.clear();
      }
      if (paths.size() != 0) {
      } else {
        return loopPaths;
      }

#pragma omp parallel
      {
        std::set<std::vector<int>> local_loop_paths;
#pragma omp for schedule(guided)
        for (unsigned int qqq = 0; qqq < paths.size(); qqq++) {
          std::set<std::vector<int>> movepaths;
          std::vector<int> path; // = paths[qqq];
          path = paths[qqq];     // unzipPath(paths[qqq], g, begin, end);
          int permnums = 1;
          std::vector<int> perms;
          std::vector<unsigned int> qs;
          std::map<int, std::vector<std::vector<int>>> localLoops;
          collectLocalLoopsForPath(path, globalLoopPaths, localLoops, perms, qs,
                                   permnums);
          std::vector<std::vector<int>> boxpaths =
              buildLoopPermutations(path, localLoops, qs, perms, permnums);

          unsigned long long eval_increment =
              static_cast<unsigned long long>(boxpaths.size());
#pragma omp atomic
          evaledpaths += eval_increment;
#pragma omp critical
          {
            if (evaledpaths > newmil * 100000ull) {
              newmil++;
            }
          }
          if (!loop) {
            for (std::vector<std::vector<int>>::iterator box = boxpaths.begin();
                 box != boxpaths.end(); box++) {
              std::vector<Vertex> verts;
              getVertexPath((*box), g, verts);
              if (needssafety) {
                // Some analyzePath implementations are not thread-safe;
                // serialize in this mode to avoid data races.
#pragma omp critical
                {
                  analyzePath(verts);
                }
              } else {
                analyzePath(verts);
              }
            }
          } else {
            local_loop_paths.insert(boxpaths.begin(), boxpaths.end());
          }
        }
        if (loop && !local_loop_paths.empty()) {
#pragma omp critical
          {
            loopPaths.insert(local_loop_paths.begin(), local_loop_paths.end());
          }
        }
      }
    }
#ifdef LP
    if (loop) {
      loopStore[begin] = loopPaths;
    }
#endif
    return loopPaths;
  }
}

/**
This is the function that is used by the user directly to start the algorithm.
It is immediately available to the user

SgGraphTraversal::constructPathAnalyzer
Input:
@param[begin] Vertex, starting node
@param[end] Vertex, endnode
@param[g] CFG* g, CFG calculated previously
*/

template <class CFG>
void SgGraphTraversal<CFG>::constructPathAnalyzer(CFG *g, bool unbounded,
                                                  Vertex begin, Vertex end,
                                                  bool ns) {
  abnormals = 0;
  normals = 0;
  if (ns) {
    needssafety = true;
  } else {
    needssafety = false;
  }
  checkedfound = 0;
  recursed = 0;
  nextsubpath = 0;
  borrowed = true;
  stoppedpaths = 0;
  evaledpaths = 0;
  badpaths = 0;
  sourcenum = 0;
  prepareGraph(g);
  workingthread = false;
  workingthreadnum = -1;
  bool subgraph = false;
  if (!subgraph) {
    if (!unbounded) {
      bound = true;
      recursiveLoops.clear();
      recurses.clear();
      std::vector<std::vector<int>> spaths =
          bfsTraversePath(vertintmap[begin], vertintmap[end], g);
    } else {
      std::set<int> usedsources;
      bound = false;
      std::vector<int> localLps;
      for (unsigned int j = 0; j < sources.size(); j++) {
        sourcenum = sources[j];
        recursiveLoops.clear();
        recurses.clear();
        std::vector<std::vector<int>> spaths =
            bfsTraversePath(sources[j], -1, g);
      }
    }
  }
  printHotness(g);
}

/** DEPRECATED
This is a function to construct subgraphs for parallelization
SgGraphTraversal::computeSubGraphs
Input:
@param[begin] const int, starting point
@param[end] const int ending point
@param[g] const CFG*, control flow graph to compute
@param[depthDifferential] int, used to specify how large the subgraph should be
 */

template <class CFG>
void SgGraphTraversal<CFG>::computeSubGraphs(const int &begin, const int &end,
                                             CFG *&g, int depthDifferential) {
  int minDepth = 0;
  int maxDepth = minDepth + depthDifferential;
  int currSubGraph = 0;
  CFG *subGraph;
  std::set<int> foundNodes;
  while (true) {
    Vertex begin = add_vertex(*subGraphVector[currSubGraph]);
    GraphSubGraphMap[currSubGraph][intvertmap[orderOfNodes[minDepth]]] =
        intvertmap[begin];
    SubGraphGraphMap[currSubGraph][intvertmap[begin]] =
        intvertmap[orderOfNodes[minDepth]];
    for (int i = minDepth; i <= maxDepth; i++) {
      Vertex v = GraphSubGraphMap[currSubGraph][intvertmap[orderOfNodes[i]]];
      std::vector<int> outEdges = getOutEdges(orderOfNodes[i], g);
      for (unsigned int j = 0; j < outEdges.size(); j++) {
        Vertex u;
        if (foundNodes.find(getTarget(outEdges[j], g)) == foundNodes.end()) {
          u = GraphSubGraphMap[currSubGraph]
                              [intvertmap[getTarget(outEdges[j], g)]];
        } else {
          u = add_vertex(*subGraphVector[currSubGraph]);
          foundNodes.insert(getTarget(outEdges[j], g));
          SubGraphGraphMap[currSubGraph][u] =
              intvertmap[getTarget(outEdges[j], g)];
          GraphSubGraphMap[currSubGraph]
                          [intvertmap[getTarget(outEdges[j], g)]] = u;
        }
        Edge edge;
        bool ok;
        std::tie(edge, ok) = add_edge(v, u, *subGraphVector[currSubGraph]);
      }
    }
    minDepth = maxDepth;
    if ((unsigned int)minDepth == orderOfNodes.size() - 1) {
      break;
    }
    maxDepth += depthDifferential;
    if ((unsigned int)maxDepth > orderOfNodes.size() - 1) {
      maxDepth = orderOfNodes.size() - 1;
    }
    CFG *newSubGraph;
    subGraphVector.push_back(newSubGraph);
    currSubGraph++;
  }
  return;
}

/*
These should NOT be used by the user. They are simply for writing interesting
information on the DOT graphs of the CFG
*/

template <class CFG>
void SgGraphTraversal<CFG>::printCFGNodeGeneric(int &cf, std::string prop,
                                                std::ofstream &o) {
  std::string nodeColor = "black";
  o << cf << " [label=\"" << " num:" << cf << " prop: " << prop
    << "\", color=\"" << nodeColor << "\", style=\"" << "solid" << "\"];\n";
}

template <class CFG>
void SgGraphTraversal<CFG>::printCFGNode(int &cf, std::ofstream &o) {
#ifdef FULLDEBUG
  int pts = ptsNum[cf];
  std::string nodeColor = "black";
  o << cf << " [label=\"" << " pts: " << pts << "\", color=\"" << nodeColor
    << "\", style=\"" << "solid" << "\"];\n";
#endif
#ifndef FULLDEBUG
  std::string nodeColor = "black";
  o << cf << " [label=\"" << " num:" << cf << "\", color=\"" << nodeColor
    << "\", style=\"" << "solid" << "\"];\n";
#endif
}

template <class CFG>
void SgGraphTraversal<CFG>::printCFGEdge(int &cf, CFG *&cfg, std::ofstream &o) {
  int src = getSource(cf, cfg);
  int tar = getTarget(cf, cfg);
  o << src << " -> " << tar << " [label=\"" << src << " " << tar
    << "\", style=\"" << "solid" << "\"];\n";
}

template <class CFG> void SgGraphTraversal<CFG>::printHotness(CFG *&g) {
  const CFG *gc = g;
  int currhot = 0;
  std::ofstream mf;
  std::stringstream filenam;
  filenam << "hotness" << currhot << ".dot";
  currhot++;
  std::string fn = filenam.str();
  mf.open(fn.c_str());

  mf << "digraph defaultName { \n";
  // DQ (4/11/2017): Fix Klockworks issue of uninitialized variables.
#if 1
  vertex_iterator v, vend;
  edge_iterator e, eend;
#else
  // This does not compile.
  vertex_iterator v = vertices(*gc).begin();
  vertex_iterator vend = v;
  edge_iterator e = edges(*gc).begin();
  edge_iterator eend = e;
#endif
  for (std::tie(v, vend) = vertices(*gc); v != vend; ++v) {
    printCFGNode(vertintmap[*v], mf);
  }
  for (std::tie(e, eend) = edges(*gc); e != eend; ++e) {
    printCFGEdge(edgeintmap[*e], g, mf);
  }
  mf.close();
}
template <class CFG> void SgGraphTraversal<CFG>::printPathDot(CFG *&g) {
  const CFG *gc = g;
  std::ofstream mf;
  std::stringstream filenam;
  filenam << "pathnums.dot";
  std::string fn = filenam.str();
  mf.open(fn.c_str());

  mf << "digraph defaultName { \n";
  vertex_iterator v, vend;
  edge_iterator e, eend;
  for (std::tie(v, vend) = vertices(*gc); v != vend; ++v) {
    if (nodeStrings.find(vertintmap[*v]) != nodeStrings.end()) {
      int nn = vertintmap[*v];
      printCFGNodeGeneric(vertintmap[*v], nodeStrings[nn], mf);
    } else {
      printCFGNodeGeneric(vertintmap[*v], "noprop", mf);
    }
  }
  for (std::tie(e, eend) = edges(*gc); e != eend; ++e) {
    printCFGEdge(edgeintmap[*e], g, mf);
  }

  mf.close();
}

/**
This is the function that preps the graph for traversal

SgGraphTraversal::prepareGraph
Input:
@param[g] CFG*& g, CFG calculated previously
*/

template <class CFG> void SgGraphTraversal<CFG>::prepareGraph(CFG *&g) {
  nextNode = 1;
  nextEdge = 1;
  findClosuresAndMarkersAndEnumerate(g);
}

/**
DEPRECATED
This is the function that preps the graph for traversal, currently this one
isn't used but for many traversals on one visitor may necessitate

SgGraphTraversal::firstPrepGraph
Input:
@param[g] CFG*& g, CFG calculated previously
*/

template <class CFG> void SgGraphTraversal<CFG>::firstPrepGraph(CFG *&g) {
  nextNode = 1;
  nextEdge = 1;
  findClosuresAndMarkersAndEnumerate(g);
}

/**
This calculates nodes with more than one in edge or more than one out edge

SgGraphTraversal::findClosuresAndMarkers
Input:
@param[g] CFG*& g, CFG calculated previously
*/

template <class CFG>
void SgGraphTraversal<CFG>::findClosuresAndMarkersAndEnumerate(CFG *&g) {
  // DQ (4/11/2017): Fix Klockworks issue of uninitialized variables.
#if 1
  edge_iterator e, eend;
#else
  edge_iterator e = edges(*g).begin();
  edge_iterator eend = e;
#endif
  for (std::tie(e, eend) = edges(*g); e != eend; ++e) {
    intedgemap[nextEdge] = *e;
    edgeintmap[*e] = nextEdge;
    nextEdge++;
  }
  // DQ (4/11/2017): Fix Klockworks issue of uninitialized variables.
#if 1
  vertex_iterator v1, vend1;
#else
  vertex_iterator v1 = vertices(*g).begin();
  vertex_iterator vend1 = v1;
#endif
  for (std::tie(v1, vend1) = vertices(*g); v1 != vend1; ++v1) {
    vertintmap[*v1] = nextNode;
    intvertmap[nextNode] = *v1;
    nextNode++;
  }
  // DQ (4/11/2017): Fix Klockworks issue of uninitialized variables.
#if 1
  vertex_iterator v, vend;
#else
  vertex_iterator v = vertices(*g).begin();
  vertex_iterator vend = v;
#endif
  for (std::tie(v, vend) = vertices(*g); v != vend; ++v) {
    std::vector<int> outs = getOutEdges(vertintmap[*v], g);
    std::vector<int> ins = getInEdges(vertintmap[*v], g);
    if (outs.size() > 1) {
      markers.push_back(vertintmap[*v]);

      markerIndex[vertintmap[*v]] = markers.size() - 1;
      for (unsigned int i = 0; i < outs.size(); i++) {
        pathsAtMarkers[vertintmap[*v]].push_back(getTarget(outs[i], g));
      }
    }
    if (ins.size() > 1) {
      closures.push_back(vertintmap[*v]);
    }
    if (outs.size() == 0) {
      sinks.push_back(vertintmap[*v]);
    }
    if (ins.size() == 0) {
      sources.push_back(vertintmap[*v]);
    }
  }
  return;
}

/** DEPRECATED
Currently unused but will be necessary for parallelization in progress
SgGraphTraversal::computeOrder
@param[g] CFG* cfg in question
@parm[begin] const int, integer representation of source node
*/
template <class CFG>
void SgGraphTraversal<CFG>::computeOrder(CFG *&g, const int &begin) {
  std::vector<int> currentNodes;
  std::vector<int> newCurrentNodes;
  currentNodes.push_back(begin);
  std::map<int, int> reverseCurrents;
  orderOfNodes.push_back(begin);
  std::set<int> heldBackNodes;
  while (currentNodes.size() != 0) {
    for (unsigned int j = 0; j < currentNodes.size(); j++) {

      std::vector<int> inEdges = getInEdges(currentNodes[j], g);
      if (inEdges.size() > 1) {
        if (reverseCurrents.find(currentNodes[j]) == reverseCurrents.end()) {
          reverseCurrents[currentNodes[j]] = 0;
        }
        if ((unsigned int)reverseCurrents[currentNodes[j]] ==
            inEdges.size() - 1) {
          heldBackNodes.erase(currentNodes[j]);
          reverseCurrents[currentNodes[j]]++;
          std::vector<int> outEdges = getOutEdges(currentNodes[j], g);
          for (unsigned int k = 0; k < outEdges.size(); k++) {
            newCurrentNodes.push_back(getTarget(outEdges[k], g));
            orderOfNodes.push_back(getTarget(outEdges[k], g));
          }
        } else if (reverseCurrents[currentNodes[j]] < reverseCurrents.size()) {
          reverseCurrents[currentNodes[j]]++;
          if (heldBackNodes.find(currentNodes[j]) == heldBackNodes.end()) {
            heldBackNodes.insert(currentNodes[j]);
          }
        }
      } else {
        std::vector<int> outEdges = getOutEdges(currentNodes[j], g);
        for (unsigned int k = 0; k < outEdges.size(); k++) {
          newCurrentNodes.push_back(getTarget(outEdges[k], g));
          orderOfNodes.push_back(getTarget(outEdges[k], g));
        }
      }
    }
    if (newCurrentNodes.size() == 0 && heldBackNodes.size() != 0) {
      for (std::set<int>::iterator q = heldBackNodes.begin();
           q != heldBackNodes.end(); q++) {
        int qint = *q;
        std::vector<int> heldBackOutEdges = getOutEdges(qint, g);
        for (unsigned int p = 0; p < heldBackOutEdges.size(); p++) {
          newCurrentNodes.push_back(getTarget(heldBackOutEdges[p], g));
        }
      }
      heldBackNodes.clear();
    }
    currentNodes = newCurrentNodes;
    newCurrentNodes.clear();
  }
  return;
}

/**
Converts the path calculated by this algorithm to Vertices so users can
access data
SgGraphTraversal::getVertexPath
@param[path] integer representation of path
@param[g] CFG*, cfg in question
@param[vertexPath] for some reason this can't be a return value so it is changed
via pass by reference
*/

template <class CFG>
void SgGraphTraversal<CFG>::getVertexPath(std::vector<int> path, CFG *&g,
                                          std::vector<Vertex> &vertexPath) {
  for (unsigned int i = 0; i < path.size(); i++) {
    vertexPath.push_back(intvertmap[path[i]]);
  }
}

/**
DEPRECATED
Currently unused, may eventually be modified for optimal storage purposes
SgGraphTraversal::storeCompact
@param[compactPath] path to be compactified
*/
template <class CFG>
void SgGraphTraversal<CFG>::storeCompact(std::vector<int> compactPath) {
  return;
}

#endif // ROSE_MIDEND_ASTPROCESSING_GRAPHPROCESSING_H
