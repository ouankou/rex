// -*- c++ -*-

#ifndef ROSE_DOCS_ROSE_COMMIT_MESSAGES_H
#define ROSE_DOCS_ROSE_COMMIT_MESSAGES_H

/** @brief Writing Git commit messages
 *
 * What constitutes a good Git commit message.
 *
 * Each commit should ideally provide a single bug fix or (part of) a feature, and thus the comment should be quite focused.
 * Each commit message consists of a title line and a body separated from one another by a blank line.
 *
 * @section commit_messages_title Titles
 *
 * - The title should start with a subject in parentheses.
 * - Titles should be short, single-line statements. Details are in the body.
 *
 * Subjects are created on the fly. Their purpose is to make it easier for a human to scan a long list of commit titles to
 * find certain things since it's faster to read a one or two word subject than it is to read a whole title. They also make it
 * easier to visually group related commit titles. A few example subjects:
 *
 * | Subject name | What it means |
 * | --- | --- |
 * | Frontend, Analysis | Some broad part of the ROSE library. |
 * | compass2 | A particular project that uses ROSE. |
 * | Attribute, Stack Delta | Some specific feature. |
 *
 * Some examples of good commit titles:
 *
 * - `(Attributes) Attribute is moved`
 * - `(move tool) one more test case for comments handling`
 * - `(C++11) Minor fixes to compile ROSE with C++11`
 * - `(Tutorial) Fixed link error in tutorial`
 *
 * @section commit_messages_body Body
 *
 * The body provides details that are missing from the title and which are not easily evident from the patch. Some examples
 * about what is good for a commit message body:
 *
 * - The **general overview** of what changed. Often the patch is so detailed and/or spread out that it's hard to otherwise
 *   discern what really changed. For instance, if test answers are updated the commit should explain why the answer changed.
 * - Where the commit came from, if not from the committer. For instance, if the commit was a patch submitted by a user it is
 *   polite to **thank the user** or the organization (provided they don't object).
 * - If controversial, the commit should mention its **provenance**. Perhaps it came from developer consensus at a team
 *   meeting. Otherwise someone who wasn't privy to that information might revert the commit later.
 * - **Changes that affect users** should be listed including what the user needs to change in their own code. Some of our
 *   users know how to use Git and will either look through the commit messages before merging or will search the commit
 *   messages when they have a compile error due to changed names. (On a similar note, try not to change the API without
 *   first using ROSE_DEPRECATED for a suitable period of time.)
 * - The body should **reference a JIRA issue** if the commit is related to an issue, especially if that issue has
 *   relevant details, discussion, test cases, etc. Place the issue name as the last line of the commit message, one
 *   issue per line for the sake of automated tools. Most commits should correspond to at least one JIRA issue. Git
 *   has tools for searching for commits that affect a JIRA issue (e.g., `git log --grep '^ROSE-1234'`).
 *
 * The body must be separated from the title by a blank line because some Git tools concatenate the first lines together into
 * a single title line, which creates formatting problems since the title then becomes very long. Also, although ROSE uses a
 * 130-column policy throughout, commit messages should try to keep lines around 80-characters because that works better
 * in GUIs like github and qgit.
 *
 * Make sure your message is spelled correctly since it cannot be changed once it's merged into a release branch--at least not
 * without a very disruptive history rewrite. Commit messages are for posterity, so be careful. "I hate git", commit
 * [a53823f](https://github.com/rose-compiler/rose/commit/a53823f), was unprofessional but is now effectively permanent.
 *
 * @section commit_messages_examples Examples of good commit messages
 *
 * This commit message makes it clear what users must change in their code:
 *
 * ```text
 * (Attributes) Changed names for some classes in the new Attribute API
 *
 * Attribute::Node         -> Attribute::NodeBase
 * Attribute::NodePtr      -> Attribute::Ptr
 * Attribute::LeafPtr      -> Attribute::ScalarPtr
 * Attribute::InternalPtr  -> Attribute::CompositePtr
 *
 * ROSE-123
 * ```
 *
 * This commit makes it clear to the simulator2 project maintainer why a change was made since it was changed by someone that
 * doesn't normally work on that project:
 *
 * ```text
 * (Simulator2) Add Linux conditional around statfs::f_flags
 *
 * statfs::f_flags is only available since Linux 2.6.36.
 * Our internal RHEL5 testing servers are Linux 2.6.18.
 *
 * ROSE-123
 * ```
 *
 * This commit explains why a change was made, which is not evident from the change itself:
 *
 * ```text
 * (Git) Update .gitmodules to point to release repositories
 *
 * This decision was reached during a team meeting with Dan,
 * Markus, Leo, Justin, and Pei-Hung.
 *
 * The solution:
 *
 * 1. Merge the rose/scratch/tools.git branches into the
 *    rose/tools.git repository.
 *
 * 2. Update .gitmodules to point to the release repository.
 *
 * 3. Remove the rose/scratch/tools.git repository once it
 *    has been entirely merged into the release repository.
 *
 * ROSE-123
 * JENKINS-456
 * ```
 *
 * @section commit_messages_jira JIRA Issues
 *
 * To search for which commits implement a certain JIRA issue, use this command:
 *
 * ```bash
 * git log --grep '^ROSE-1234'
 * ```
 *
 * In a shell script, to get the list of all the JIRA issues for a particular commit, use this command:
 *
 * ```bash
 * git log -n1 --format='%B' $commit_sha1 |grep '^[A-Z]\\+-[1-9][0-9]*$'
 * ```
 *
 * See @ref developer_docs.
 */
struct commit_messages {};

#endif
