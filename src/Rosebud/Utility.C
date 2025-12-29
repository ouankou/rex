#include <Rosebud/Utility.h>

#include <Sawyer/Clexer.h>
#include <Sawyer/GraphTraversal.h>
#include <Sawyer/StaticBuffer.h>

#include <Rose/StringUtility.h>

#include <algorithm>
#include <cctype>
#include <cstring>
#include <iostream>
#include <regex>

using namespace Sawyer::Message::Common;

namespace Rosebud {

Settings settings;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// String utilities
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

std::string
matching(const std::string &s) {
    if (s.size() == 1) {
        return std::string(1, matching(s[0]));
    }
    ASSERT_not_reachable("not a nesting character \"" + s + "\"");
}

char
matching(char ch) {
    static const char *key   = "({[<>]})";
    static const char *value = ")}]><[{(";
    if (const char *pos = std::strchr(key, ch)) {
        return value[pos - key];
    }
    ASSERT_not_reachable("not a nesting character '" + std::string(1, ch) + "'");
}

bool
startsWith(const std::string &s, const std::string &prefix) {
    return s.size() >= prefix.size() && s.compare(0, prefix.size(), prefix) == 0;
}

bool
endsWith(const std::string &s, const std::string &suffix) {
    return s.size() >= suffix.size() &&
           s.compare(s.size() - suffix.size(), suffix.size(), suffix) == 0;
}

std::vector<std::string>
splitIntoLines(const std::string &s) {
    return Rose::StringUtility::split('\n', s);
}

void
eraseBlankLines(std::vector<std::string> &lines) {
    std::regex blankLineRe("[ \t]*");
    lines.erase(std::remove_if(lines.begin(), lines.end(), [&blankLineRe](const std::string &line) {
        return std::regex_match(line, blankLineRe);
    }), lines.end());
}

void
trimBlankLines(std::vector<std::string> &lines) {
    // Trim white space from the end of every line
    for (std::string &line: lines)
        line = Rose::StringUtility::trim(line, " \t\r\n", false, true);

    // Remove blank lines from the end
    while (!lines.empty() && lines.back().empty())
        lines.pop_back();

    // Remove blank lines from the beginning
    auto firstNonBlank = std::find_if(lines.begin(), lines.end(), [](const std::string &s) {
        return !s.empty();
    });
    lines.erase(lines.begin(), firstNonBlank);

    // Replace two or more consecutive blank lines with a single blank line.
    for (size_t i = lines.size(); i > 1; --i) {
        if (lines[i-1].empty() && lines[i-2].empty())
            lines.erase(lines.begin() + i - 1);
    }
}

std::string
prefixLines(const std::string &s, const std::string &prefix) {
    std::vector<std::string> lines = splitIntoLines(s);
    prefixLines(lines, prefix);
    return Rose::StringUtility::join("\n", lines);
}

void
prefixLines(std::vector<std::string> &lines, const std::string &prefix) {
    for (std::string &line: lines)
        line = prefix + line;
}

struct LevenshteinStack {
    typedef std::pair<char/*key*/, size_t/*value*/> KeyVal;
    typedef std::list<KeyVal> KeyValList;
    KeyValList pairs;

    void unique_push_zero(char key) {
        for (typename KeyValList::iterator pi=pairs.begin(); pi!=pairs.end(); ++pi) {
            if (pi->first==key)
                return;
        }
        pairs.push_front(KeyVal(key, 0));
    }

    size_t& operator[](char key) {
        for (typename KeyValList::iterator pi=pairs.begin(); pi!=pairs.end(); ++pi) {
            if (pi->first==key)
                return pi->second;
        }
        ASSERT_not_reachable("not found");
    }
};

// Returns the Damerau-Levenshtein edit distance.
size_t
editDistance(const std::string &src, const std::string &tgt) {
    // Based on the C# implementation on the wikipedia page
    if (src.empty() || tgt.empty())
        return std::max(src.size(), tgt.size());

    const size_t x = src.size();
    const size_t y = tgt.size();
    std::vector<std::vector<size_t> > score(x+2, std::vector<size_t>(y+2, 0));
    size_t score_ceil = x + y;
    score[0][0] = score_ceil;
    for (size_t i=0; i<=x; ++i) {
        score[i+1][1] = i;
        score[i+1][0] = score_ceil;
    }
    for (size_t j=0; j<=y; ++j) {
        score[1][j+1] = j;
        score[0][j+1] = score_ceil;
    }

    LevenshteinStack dict;
    for (size_t i=0; i<x; ++i)
        dict.unique_push_zero(src[i]);
    for (size_t j=0; j<y; ++j)
        dict.unique_push_zero(tgt[j]);

    for (size_t i=1; i<=x; ++i) {
        size_t db = 0;
        for (size_t j=1; j<=y; ++j) {
            size_t i1 = dict[tgt[j-1]];
            size_t j1 = db;
            if (src[i-1]==tgt[j-1]) {
                score[i+1][j+1] = score[i][j];
                db = j;
            } else {
                score[i+1][j+1] = std::min(score[i][j], std::min(score[i+1][j], score[i][j+1])) + 1;
            }
            // swaps
            score[i+1][j+1] = std::min(score[i+1][j+1], score[i1][j1] + (i-i1-1) + 1 + (j-j1-1));
        }
        dict[src[i-1]] = i;
    }

    return score[x+1][y+1];
}

double
relativeDifference(const std::string &src, const std::string &tgt) {
    const size_t n = std::max(src.size(), tgt.size());
    return n == 0 ? 0.0 : (double)editDistance(src, tgt) / n;
}

std::string
bestMatch(const std::vector<std::string> &candidates, const std::string &sample) {
    if (candidates.empty())
        return "";

    // Compute all scores
    std::vector<std::pair<double, size_t>> scores;
    scores.reserve(candidates.size());
    for (size_t i = 0; i < candidates.size(); ++i)
        scores.push_back(std::make_pair(relativeDifference(sample, candidates[i]), i));

    // Return candidate with lowest score (least difference from sample)
    const size_t bestIdx = std::min_element(scores.begin(), scores.end(),
                                            [](const auto &a, const auto &b) {
                                                return a.first < b.first;
                                            })->second;
    return candidates[bestIdx];
}

std::string
toString(Access access) {
    switch (access) {
        case Access::PRIVATE:
            return "private";
        case Access::PROTECTED:
            return "protected";
        case Access::PUBLIC:
            return "public";
    }
    ASSERT_not_reachable("invalid access");
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Filesystem utilities
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

Rose::FileSystem::Path
findRoseRootDir(const Rose::FileSystem::Path &start) {
    Rose::FileSystem::Path root = Rose::FileSystem::makeAbsolute(start);
    while (!root.empty()) {
        if (Rose::FileSystem::isDirectory(root) && Rose::FileSystem::isFile(root / "src/Rosebud/Ast.h")) {
            break;
        }
        Rose::FileSystem::Path parent = root.parent_path();
        if (parent == root) {
            root.clear();
            break;
        }
        root = parent;
    }
    return root;
}

Rose::FileSystem::Path
relativeToRoseSource(const Rose::FileSystem::Path &fileName) {
    Rose::FileSystem::Path root = findRoseRootDir(fileName);
    if (root.empty()) {
        return {};
    }

    Rose::FileSystem::Path absFile = Rose::FileSystem::makeAbsolute(fileName);
    Rose::FileSystem::Path absRoot = Rose::FileSystem::makeAbsolute(root);

    auto rootIter = absRoot.begin();
    auto fileIter = absFile.begin();
    for (; rootIter != absRoot.end() && fileIter != absFile.end(); ++rootIter, ++fileIter) {
        if (*rootIter != *fileIter)
            return {};
    }
    if (rootIter != absRoot.end())
        return {};

    Rose::FileSystem::Path rel;
    for (; fileIter != absFile.end(); ++fileIter)
        rel /= *fileIter;
    return rel;
}

Rose::FileSystem::Path
toPath(const std::string &symbol, const std::string &extension) {
    Rose::FileSystem::Path path;
    size_t at = 0;
    while (at < symbol.size()) {
        const size_t sep = symbol.find("::", at);
        const std::string component = sep == std::string::npos ?
                                      symbol.substr(at) :
                                      symbol.substr(at, sep - at);
        if (!component.empty())
            path /= component;
        if (sep == std::string::npos)
            break;
        at = sep + 2;
    }
    if (!extension.empty())
        path += extension;
    return path;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Comment utilities
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

std::string
makeBlockComment(const std::string &text, const std::string &open) {
    return Rose::StringUtility::join("\n", makeBlockComment(splitIntoLines(text), open));
}

std::vector<std::string>
makeBlockComment(const std::vector<std::string> &textLines, const std::string &open) {
    std::regex cStyle("([ \t]*)/\\*(.*)");
    std::regex cxxStyle("([ \t]*)//(.*)");
    std::smatch parts;
    std::string prefix, close;
    if (std::regex_match(open, parts, cStyle)) {
        prefix = parts.str(1) + " * ";
        close = " */";
    } else if (std::regex_match(open, parts, cxxStyle)) {
        prefix = parts.str(1) + "// ";
    } else {
        prefix = open;
        close = " */";
    }

    std::vector<std::string> comment;
    comment.reserve(textLines.size());
    for (const std::string &line: textLines)
        comment.push_back(comment.empty() ? open + line : prefix + line);

    if (!comment.empty())
        comment.back() += close + "\n";
    return comment;
}

std::string
makeTitleComment(const std::string &multiLine, const std::string &prefix, char bar, size_t width) {
    return Rose::StringUtility::join("\n",
                                     makeTitleComment(splitIntoLines(Rose::StringUtility::trim(multiLine)),
                                                      prefix, bar, width)) + "\n";
}

std::vector<std::string>
makeTitleComment(const std::vector<std::string> &lines, const std::string &prefix, char bar, size_t width) {
    std::vector<std::string> retval;
    const std::string commentLeft = '#' == bar ? "#" : "//";
    const std::string topBottom = prefix + commentLeft + std::string(width - std::min(commentLeft.size(), width), bar);

    retval.reserve(2 + lines.size());
    retval.push_back(topBottom);
    for (const std::string &line: lines)
        retval.push_back(prefix + commentLeft + " " + line);
    retval.push_back(topBottom);
    return retval;
}

std::string
appendToDoxygen(const std::string &comment, const std::string &newText) {
    std::vector<std::string> commentLines = splitIntoLines(comment);
    const std::vector<std::string> newTextLines = splitIntoLines(newText);

    // Remove the last line of the comment if it contains only the C-style closing.  Otherwise just remove the C-style closing from
    // the lat line.
    const std::regex closingOnlyRe("[^a-zA-Z0-9]*\\*/[ \t]*");
    const std::regex closingRe("(.*)\\*/[ \t]*");
    if (!commentLines.empty()) {
        std::smatch parts;
        if (std::regex_match(commentLines.back(), closingOnlyRe)) {
            commentLines.pop_back();
        } else if (std::regex_match(commentLines.back(), parts, closingRe)) {
            commentLines.back() = parts.str(1);
        }
    }

    // What is the prefix for the previous lines of the comment that contain text.
    const std::string prefix = [&commentLines]() -> std::string {
        if (commentLines.empty()) {
            return "/** ";
        } else {
            const std::regex nonEmptyPrefixRe("^([^a-zA-Z0-9@\\\\]*)[a-zA-Z0-9@\\\\]");
            const std::regex startOfCommentRe("^([ \\t]*)/\\*");
            std::smatch parts;
            for (size_t i = commentLines.size(); i > 0; --i) {
                const std::string &line = commentLines[i-1];
                if (1 == i && std::regex_search(line, parts, startOfCommentRe)) {
                    return parts.str(1) + " * ";
                } else if (std::regex_search(line, parts, nonEmptyPrefixRe)) {
                    return parts.str(1);
                }
            }
            return " * ";
        }
    }();

    // Build the new comment and add it to the end of the existing comment.
    const std::vector<std::string> newCommentLines = makeBlockComment(newTextLines, prefix);
    commentLines.insert(commentLines.end(), newCommentLines.begin(), newCommentLines.end());
    return Rose::StringUtility::join("\n", commentLines);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Diagnostic messages
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

size_t nErrors = 0;

bool usingColor() {
    switch (settings.usingColor) {
        case When::NEVER:
            return false;
        case When::ALWAYS:
            return true;
        case When::AUTO:
            return isatty(2);

    }
    ASSERT_not_reachable("invalid when");
}

std::string
messageString(Sawyer::Message::Importance importance, const std::string &mesg) {

    const auto ansi = [importance]() -> std::pair<std::string, std::string> {
        const Sawyer::Message::ColorSpec cs = (usingColor() ?
                                               Sawyer::Message::ColorSet::fullColor() :
                                               Sawyer::Message::ColorSet::blackAndWhite())[importance];
        if (!cs.isDefault()) {
            using namespace Sawyer::Message;
            std::vector<std::string> parts;
            if (cs.foreground != COLOR_DEFAULT)
                parts.push_back(std::to_string(30 + cs.foreground));
            if (cs.background != COLOR_DEFAULT)
                parts.push_back(std::to_string(40 + cs.background));
            if (cs.bold)
                parts.push_back("1");
            return {"\033[" + Rose::StringUtility::join(";", parts) + "m", std::string("\033[m")};
        } else {
            return {"", ""};
        }
    }();

    switch (importance) {
        case DEBUG:            return ansi.first + "debug: "   + ansi.second + mesg;
        case TRACE:            return ansi.first + "trace: "   + ansi.second + mesg;
        case WHERE:            return ansi.first + "where: "   + ansi.second + mesg;
        case MARCH:            return ansi.first + "march: "   + ansi.second + mesg;
        case INFO:             return ansi.first + "info: "    + ansi.second + mesg;
        case WARN:             return ansi.first + "warning: " + ansi.second + mesg;
        case ERROR: ++nErrors; return ansi.first + "error: "   + ansi.second + mesg;
        case FATAL: ++nErrors; return ansi.first + "error: "   + ansi.second + mesg;
        default: ASSERT_not_reachable("invalid importance");
    }
}

// Message for no input file
void
message(Sawyer::Message::Importance importance, const std::string &mesg) {
    for (const std::string &line: splitIntoLines(mesg))
        std::cerr <<Sawyer::thisExecutableName() <<": " <<messageString(importance, line) <<"\n";
}

// Message for input file and single token
void
message(Sawyer::Message::Importance importance, const Ast::File::Ptr &file, const Token &token,
        const std::string &mesg) {
    message(importance, file, token, token, token, mesg);
}

// Message for input file and range of tokens
void
message(Sawyer::Message::Importance importance, const Ast::File::Ptr &file, const std::vector<Token> &tokens,
        const std::string &mesg) {
    ASSERT_forbid(tokens.empty());
    Token whole(tokens.front().type(), tokens.front().prior(), tokens.front().begin(), tokens.back().end());
    message(importance, file, whole, mesg);
}

void
message(Sawyer::Message::Importance importance, const Ast::File::Ptr &file, const Token &begin,
        const Token &focus, const Token &end, const std::string &mesg) {
    ASSERT_not_null(file);
    if (settings.debugging || importance != DEBUG) {
        const auto loc = file->tokenStream().location(begin);
        const std::vector<std::string> lines = splitIntoLines(mesg);

        for (const std::string &line: lines) {
            std::cerr <<file->tokenStream().fileName() <<":" <<(loc.first + 1) <<":" <<(loc.second + 1) <<": "
                      <<messageString(importance, line) <<"\n";
        }

        file->emitContext(std::cerr, begin, focus, end);

        if (ERROR == importance || FATAL == importance)
            ++nErrors;
    }
}

void
message(Sawyer::Message::Importance importance, const Ast::File::Ptr &file, const std::string &mesg) {
    ASSERT_not_null(file);

    if (settings.debugging || importance != DEBUG) {
        for (const std::string &line: splitIntoLines(mesg)) {
            std::cerr <<file->tokenStream().fileName() <<": " <<messageString(importance, line) <<"\n";
        }
        if (ERROR == importance || FATAL == importance)
            ++nErrors;
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Class hierarchy utilities
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

Hierarchy
classHierarchy(const Classes &classes) {
    Hierarchy h;

    // Vertices for definitions
    for (const auto &c: classes) {
        ASSERT_not_null(c);
        auto found = h.findVertexKey(c);
        if (found == h.vertices().end()) {
            h.insertVertex(c);
        } else {
            const auto &curFile = c->findAncestor<Ast::File>();
            message(ERROR, curFile, c->nameToken, "class \"" + c->name + "\" is already defined");
            const auto &prevFile = found->value()->findAncestor<Ast::File>();
            message(INFO, prevFile, found->value()->nameToken, "previous definition");
        }
    }

    // Edges
    for (const auto &c: classes) {
        auto cv = h.findVertexKey(c);
        ASSERT_forbid(cv == h.vertices().end());
        for (const auto &super: c->inheritance) {
            auto sv = h.findVertexKey(super.second);
            if (sv != h.vertices().end())
                h.insertEdge(sv, cv);
        }
    }

    return h;
}

void
checkClassHierarchy(Hierarchy &h) {
    // Any cycles in the graph?
    using namespace Sawyer::Container::Algorithm;
    std::vector<bool> visited(h.nVertices(), false);
    for (size_t rootId = 0; rootId < h.nVertices(); ++rootId) {
        if (visited[rootId])
            continue;
        std::vector<size_t> path;
        path.reserve(h.nVertices());
        path.push_back(rootId);
        std::vector<bool> onPath(h.nVertices(), false);
        onPath[rootId] = true;
        using Traversal = DepthFirstForwardGraphTraversal<Hierarchy>;
        for (Traversal t(h, h.findVertex(rootId), EDGE_EVENTS); t; ++t) {
            const auto target = t.edge()->target();
            if (t.event() == ENTER_EDGE) {
                if (onPath[target->id()]) {
                    auto curFile = target->value()->findAncestor<Ast::File>();
                    message(ERROR, curFile, target->value()->nameToken, "cycle in class hierarchy");
                    for (auto it = path.rbegin(); it != path.rend(); ++it) {
                        auto next = h.findVertex(*it);
                        ASSERT_require(next != h.vertices().end());
                        auto nextFile = next->value()->findAncestor<Ast::File>();
                        message(INFO, nextFile, next->value()->nameToken, "inherits from here");
                    }
                }
                onPath[target->id()] = true;
                path.push_back(target->id());
                if (visited[target->id()]) {
                    t.skipChildren();
                } else {
                    visited[target->id()] = true;
                }
            } else {
                ASSERT_require(t.event() == LEAVE_EDGE);
                ASSERT_require(onPath[target->id()]);
                ASSERT_forbid(path.empty());
                onPath[target->id()] = false;
                path.pop_back();
            }
        }
    }
}

Classes
topDown(Hierarchy &h) {
    using namespace Sawyer::Container::Algorithm;
    Classes retval;
    retval.reserve(h.nVertices());

    std::vector<bool> seen(h.nVertices(), false);
    for (size_t rootId = 0; rootId < h.nVertices(); ++rootId) {
        if (!seen[rootId]) {
            using Traversal = DepthFirstReverseGraphTraversal<Hierarchy>;
            for (Traversal t(h, h.findVertex(rootId), LEAVE_VERTEX); t; ++t) {
                if (!seen[t.vertex()->id()]) {
                    seen[t.vertex()->id()] = true;
                    retval.push_back(t.vertex()->value());
                }
            }
        }
    }
    return retval;
}

Classes
bottomUp(Hierarchy &h) {
    Classes classes = topDown(h);
    return Classes(classes.rbegin(), classes.rend());
}

Classes
derivedClasses(const Ast::Class::Ptr &c, const Hierarchy &h) {
    ASSERT_not_null(c);
    Classes retval;

    auto vertex = h.findVertexKey(c);
    if (vertex != h.vertices().end()) {
        retval.reserve(vertex->nOutEdges());
        for (const auto &edge: vertex->outEdges())
            retval.push_back(edge.target()->value());
    }
    return retval;
}

bool
isBaseClass(const Ast::Class::Ptr &c, const Hierarchy &h) {
    ASSERT_not_null(c);
    auto vertex = h.findVertexKey(c);
    if (vertex == h.vertices().end())
        return false;
    return vertex->nOutEdges() > 0;
}

std::vector<Ast::Property::Ptr> allConstructorArguments(const Ast::Class::Ptr &c, const Hierarchy &h_) {
    ASSERT_not_null(c);
    using namespace Sawyer::Container::Algorithm;
    std::vector<Ast::Property::Ptr> retval;
    auto h = const_cast<Hierarchy&>(h_);

    auto root = h.findVertexKey(c);
    ASSERT_require2(root != h.vertices().end(), "class " + c->name);
    using Traversal = DepthFirstReverseGraphTraversal<Hierarchy>;
    for (Traversal t(h, root, LEAVE_VERTEX); t; ++t) {
        Ast::Class::Ptr baseClass = t.vertex()->value();
        for (const auto &p: *baseClass->properties()) {
            if (p->findAttribute("Rosebud::ctor_arg"))
                retval.push_back(p());
        }
    }
    return retval;
}

std::string
firstPublicBaseClass(const Ast::Class::Ptr &c) {
    ASSERT_not_null(c);

    for (const auto &base: c->inheritance) {
        if ("public" == base.first)
            return base.second;
    }
    return "";
}


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Type utilities
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

std::string
constRef(const std::string &type) {
    return type + " const&";
}

// Remove volatile and mutable from a type
std::string
removeVolatileMutable(const std::string &type) {
    std::string retval = type;
    std::regex re("^(volatile|mutable)[ \t]*");
    std::smatch found;
    while (std::regex_search(retval, found, re))
        retval = retval.substr(found.length());
    return retval;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// C preprocessor utilities
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

std::string
locationDirective(size_t line, const std::string &fileName) {
    if (settings.showingLocations) {
        const std::string directory = [&fileName]() {
            if (fileName.find('/') == std::string::npos) {
                return "src/Rosebud/";
            }
            return std::string();
        }();

        return "#line " + std::to_string(line) + " \"" + directory + fileName + "\"\n";
    }
    return "";
}

std::string
locationDirective(const Ast::Node::Ptr &node, const Token &token) {
    if (token) {
        if (auto file = node->findAncestor<Ast::File>()) {
            return locationDirective(file->tokenStream().location(token).first + 1, file->name());
        }
    }
    return "";
}

std::string
toCppSymbol(const std::string &name) {
    std::string symbol;
    symbol.reserve(name.size());

    for (const char ch: name) {
        if (std::isalnum(static_cast<unsigned char>(ch)) || ch == '_') {
            symbol.push_back(ch);
        } else {
            symbol.push_back('_');
        }
    }

    return symbol;
}

std::vector<std::string>
extractCpp(std::string &s, const std::regex &re, size_t capture) {
    std::vector<std::string> retval;

    if (!s.empty()) {
        auto buffer = Sawyer::Container::StaticBuffer<size_t, char>::instance(s.c_str(), s.size());
        Sawyer::Language::Clexer::TokenStream tokens("-", buffer);
        tokens.skipPreprocessorTokens(false);

        std::string filtered;
        while (true) {
            auto token = tokens[0];
            const std::string prior = tokens.content().contentAsString(token.prior(), token.begin());
            if (token) {
                const std::string lexeme = tokens.lexeme(token);
                std::smatch found;
                if (token.type() == Sawyer::Language::Clexer::TOK_CPP && std::regex_match(lexeme, found, re)) {
                    filtered += prior;
                    retval.push_back(found.str(capture));
                } else {
                    filtered += prior + lexeme;
                }
                tokens.consume();
            } else {
                filtered += prior;
                break;
            }
        }
        s = filtered;
    }
    return retval;
}

} // namespace
