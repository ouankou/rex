#ifndef ROSE_MIDEND_ASTPROCESSING_GRAPHPROCESSING_H
#define ROSE_MIDEND_ASTPROCESSING_GRAPHPROCESSING_H

#include "mlog.h"
#include "staticCFG.h"
#include <algorithm>
#include <fstream>
#include <map>
#include <set>
#include <string>
#include <tuple>
#include <unordered_set>
#include <utility>
#include <vector>

/**
 * @file graphProcessing.h
 * Enumerates graph paths and passes each to SgGraphTraversal::analyzePath.
 * @author Michael Hoffman
 */
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

template <class CFG> class SgGraphTraversal {
public:
  using Vertex = typename CFG::vertex_descriptor;
  using Edge = typename CFG::edge_descriptor;

  void constructPathAnalyzer(CFG *g, bool unbounded = false, Vertex begin = 0,
                             Vertex end = 0, bool ns = true);
  virtual void analyzePath(std::vector<Vertex> &pth) = 0;
  std::vector<int> getInEdges(int &node, CFG *&g);
  std::vector<int> getOutEdges(int &node, CFG *&g);
  int getTarget(int &n, CFG *&g);
  int getSource(int &n, CFG *&g);
  std::map<Vertex, int> vertintmap;
  std::map<Edge, int> edgeintmap;
  std::map<int, Vertex> intvertmap;
  std::map<int, Edge> intedgemap;
  SgGraphTraversal() = default;
  virtual ~SgGraphTraversal() = default;
  SgGraphTraversal(const SgGraphTraversal &) = default;
  SgGraphTraversal &operator=(const SgGraphTraversal &) = default;
  int pathnum = 0;

private:
  bool needssafety = true;
  void clearPreparedGraphState();
  void prepareGraph(CFG *&g);
  void findClosuresAndMarkersAndEnumerate(CFG *&g);
  std::set<std::vector<int>>
  uTraversePath(int begin, bool loop,
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
  void printCFGNode(int &cf, std::ofstream &o);
  void printCFGEdge(int &cf, CFG *&cfg, std::ofstream &o);
  void printHotness(CFG *&g);
  std::vector<int> sources;
  std::vector<int> recurses;
  std::map<int, std::set<std::vector<int>>> loopStore;
  std::vector<std::vector<int>> pathStore;
  void getVertexPath(const std::vector<int> &path,
                     std::vector<Vertex> &vertexPath);
  int nextNode = 1;
  int nextEdge = 1;
  using vertex_iterator = typename CFG::vertex_iterator;
  using out_edge_iterator = typename CFG::out_edge_iterator;
  using in_edge_iterator = typename CFG::in_edge_iterator;
  using edge_iterator = typename CFG::edge_iterator;
  bool bound = false;
};

template <class CFG> void SgGraphTraversal<CFG>::clearPreparedGraphState() {
  vertintmap.clear();
  edgeintmap.clear();
  intvertmap.clear();
  intedgemap.clear();
  sources.clear();
  recurses.clear();
  loopStore.clear();
  pathStore.clear();
}

/**
    Gets the source of an edge
    SgGraphTraversal::getSource
    Input:
    @param[edge] int& integer representation of edge in question
    @param[g] CFG*& the CFG used
*/
template <class CFG>
inline int SgGraphTraversal<CFG>::getSource(int &edge, CFG *&g) {
  Edge e = intedgemap.at(edge);
  Vertex v = source(e, *g);
  return vertintmap.at(v);
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
  Edge e = intedgemap.at(edge);
  Vertex v = target(e, *g);
  return vertintmap.at(v);
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
  Vertex getIns = intvertmap.at(node);
  std::vector<int> inedges;
  in_edge_iterator i, j;
  for (std::tie(i, j) = in_edges(getIns, *g); i != j; ++i) {
    inedges.push_back(edgeintmap.at(*i));
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
  Vertex getOuts = intvertmap.at(node);
  std::vector<int> outedges;
  out_edge_iterator i, j;
  for (std::tie(i, j) = out_edges(getOuts, *g); i != j; ++i) {
    outedges.push_back(edgeintmap.at(*i));
  }
  return outedges;
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
      std::unordered_set<int> path_nodes(npth.begin(), npth.end());

      if ((!recursedloop &&
           ((bound && npth.back() == end && npth.size() != 1) ||
            (!bound && oeds.size() == 0))) ||
          (recursedloop && npth.back() == end && npth.size() != 1)) {
        paths.push_back(npth);
      } else {
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
          } else if (path_nodes.find(tg) == path_nodes.end() || tg == end) {
            newpath.push_back(tg);
            std::vector<int> ieds = getInEdges(tg, g);
            if (ieds.size() > 1) {
              nodes.insert(tg);
            }
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
      MLOG_ERROR_C("graphProcessing", "Too many paths.\n");
      ROSE_ABORT();
    }
    for (unsigned int qq = 0; qq < working_paths.size(); qq++) {
      std::vector<int> pq = working_paths[qq];
      std::unordered_set<int> pq_lookup(pq.begin(), pq.end());
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
#ifdef _OPENMP
#pragma omp critical
#endif
            {
              npts.push_back(newpath);
            }
          }
        }
      } else {
#ifdef _OPENMP
#pragma omp critical
#endif
        {
          finnpts.push_back(pq);
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
    std::unordered_set<int> &completed_loops_set) {
  for (unsigned int k = 0; k < localLoops.size(); k++) {
    int lk = localLoops[k];
    std::vector<std::vector<int>> loopp;
    if (loopStore.find(localLoops[k]) != loopStore.end()) {
      loopp.insert(loopp.end(), loopStore[localLoops[k]].begin(),
                   loopStore[localLoops[k]].end());
    } else {
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
  std::unordered_set<int> completed_loops_set;
  std::unordered_set<int> recurses_lookup(recurses.begin(), recurses.end());
  expandBfsFrontier(begin, end, g, recursedloop, paths, localLoops, PtP,
                    completed_loops_set, recurses_lookup);
  paths = mergePathSegments(paths, PtP, begin);
  collectGlobalLoopPaths(localLoops, g, globalLoopPaths, completed_loops_set);
  std::vector<std::vector<int>> lps2;

  pathStore = paths;
  paths.clear();
  if (!recursedloop) {
    uTraversePath(begin, false, globalLoopPaths);
  } else {
    std::set<std::vector<int>> lps =
        uTraversePath(begin, true, globalLoopPaths);
    for (std::set<std::vector<int>>::iterator ij = lps.begin(); ij != lps.end();
         ij++) {
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
  std::vector<std::vector<int>> boxpaths(permnums);
  for (int i = 1; i <= permnums; i++) {
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
@param[globalLoopPaths] connects an integer representation of a node to all
possible loops starting at that node
*/

template <class CFG>
std::set<std::vector<int>> SgGraphTraversal<CFG>::uTraversePath(
    int begin, bool loop,
    std::map<int, std::vector<std::vector<int>>> &globalLoopPaths) {
  pathnum = 0;
  std::vector<std::vector<int>> paths = std::move(pathStore);
  pathStore.clear();
  std::set<std::vector<int>> loopPaths;
  if (paths.empty()) {
    return loopPaths;
  }

#ifdef _OPENMP
#pragma omp parallel
#endif
  {
    std::set<std::vector<int>> local_loop_paths;
#ifdef _OPENMP
#pragma omp for schedule(guided)
#endif
    for (unsigned int qqq = 0; qqq < paths.size(); qqq++) {
      const std::vector<int> &path = paths[qqq];
      int permnums = 1;
      std::vector<int> perms;
      std::vector<unsigned int> qs;
      std::map<int, std::vector<std::vector<int>>> localLoops;
      collectLocalLoopsForPath(path, globalLoopPaths, localLoops, perms, qs,
                               permnums);
      std::vector<std::vector<int>> boxpaths =
          buildLoopPermutations(path, localLoops, qs, perms, permnums);

      if (!loop) {
        for (std::vector<std::vector<int>>::iterator box = boxpaths.begin();
             box != boxpaths.end(); box++) {
          std::vector<Vertex> verts;
          getVertexPath(*box, verts);
          if (needssafety) {
            // Some analyzePath implementations are not thread-safe;
            // serialize in this mode to avoid data races.
#ifdef _OPENMP
#pragma omp critical
#endif
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
#ifdef _OPENMP
#pragma omp critical
#endif
      {
        loopPaths.insert(local_loop_paths.begin(), local_loop_paths.end());
      }
    }
  }
  if (loop) {
    loopStore[begin] = loopPaths;
  }
  return loopPaths;
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
  needssafety = ns;
  prepareGraph(g);
  if (!unbounded) {
    bound = true;
    recurses.clear();
    bfsTraversePath(vertintmap.at(begin), vertintmap.at(end), g);
  } else {
    bound = false;
    for (unsigned int j = 0; j < sources.size(); j++) {
      recurses.clear();
      bfsTraversePath(sources[j], -1, g);
    }
  }
  printHotness(g);
}

template <class CFG>
void SgGraphTraversal<CFG>::printCFGNode(int &cf, std::ofstream &o) {
  std::string nodeColor = "black";
  o << cf << " [label=\"" << " num:" << cf << "\", color=\"" << nodeColor
    << "\", style=\"" << "solid" << "\"];\n";
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
  std::ofstream mf("hotness0.dot");

  mf << "digraph defaultName { \n";
  vertex_iterator v, vend;
  edge_iterator e, eend;
  for (std::tie(v, vend) = vertices(*gc); v != vend; ++v) {
    printCFGNode(vertintmap.at(*v), mf);
  }
  for (std::tie(e, eend) = edges(*gc); e != eend; ++e) {
    printCFGEdge(edgeintmap.at(*e), g, mf);
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
  clearPreparedGraphState();
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
  edge_iterator e, eend;
  for (std::tie(e, eend) = edges(*g); e != eend; ++e) {
    intedgemap[nextEdge] = *e;
    edgeintmap[*e] = nextEdge;
    nextEdge++;
  }
  vertex_iterator v1, vend1;
  for (std::tie(v1, vend1) = vertices(*g); v1 != vend1; ++v1) {
    vertintmap[*v1] = nextNode;
    intvertmap[nextNode] = *v1;
    nextNode++;
  }
  vertex_iterator v, vend;
  for (std::tie(v, vend) = vertices(*g); v != vend; ++v) {
    int node = vertintmap.at(*v);
    if (getInEdges(node, g).empty()) {
      sources.push_back(node);
    }
  }
  return;
}

template <class CFG>
void SgGraphTraversal<CFG>::getVertexPath(const std::vector<int> &path,
                                          std::vector<Vertex> &vertexPath) {
  for (unsigned int i = 0; i < path.size(); i++) {
    vertexPath.push_back(intvertmap.at(path[i]));
  }
}

#endif // ROSE_MIDEND_ASTPROCESSING_GRAPHPROCESSING_H
