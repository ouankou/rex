// -*- c++ -*-

#ifndef ROSE_DOCS_MODULES_ROSEHPCT_H
#define ROSE_DOCS_MODULES_ROSEHPCT_H

/** @brief ROSE HPCToolkit Module
 *
 * Implements routines that attach HPCToolkit profile data (stored in XML files) to the ROSE Sage III IR tree.
 *
 * The core external interfaces of this module live in the `RoseHPCT` namespace. The intended end-user interface is documented
 * in the ROSE-HPCT high-level interface and the example program `examples/test_highlevel.cc`.
 *
 * The implementation consists of five primary submodules designed to make attachment of profile data to the Sage III IR
 * independent of the XML format:
 * - ROSEHPCT_LIBXML2: parses XML and stores raw XML in memory (libxml2).
 * - ROSEHPCT_XERCESC: alternative XML parser based on Xerces-C.
 * - ROSEHPCT_PROFIR: stores profile file contents independent of XML representation.
 * - ROSEHPCT_SAGE: defines metric attributes for tagging Sage III IR nodes.
 * - ROSEHPCT_XML2PROFIR / ROSEHPCT_PROFIR2SAGE: convert between XML data and profile IR, and attach metrics to IR nodes.
 *
 * HPCToolkit generates profile data at various levels (statement or scope). ROSE-HPCT attaches this data to the corresponding
 * statement or scope-level nodes. For statement-only data, a traversal propagates metrics to higher-level scopes (see
 * `RoseHPCT::propagateMetrics`).
 *
 * Additional submodules provide generic utilities and demonstrations.
 */
struct ROSEHPCT {};

/** @brief ROSE-HPCT examples
 *
 * The main example is `test_highlevel.cc`, which demonstrates the simplest interface for using ROSE-HPCT. For additional
 * control, see `attach_metrics.cc` and `propagate_metrics.cc`, which expose the steps needed for finer-grained usage.
 *
 * See @ref ROSEHPCT.
 */
struct ROSEHPCT_EXAMPLES {};

#endif
