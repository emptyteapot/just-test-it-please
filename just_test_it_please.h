//======================================================================================
// Just test it please!  Thank you for testing your code :-)
// 
// Copyright 2024, Jacob Langford
// Released under the MIT license.
// 
// Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files(the “Software”), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and /or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions :
// The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
// THE SOFTWARE IS PROVIDED “AS IS”, WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
// 
// Overview: 
// 
//      Another minimal (header-only) test framework for C++20
// 
//           #include "just_test_it_please.h"
//           int main()
//           {
//               JtTestRunner tr;
//               tr.ReportToStdout = true;
//               tr.ReportSuccess = true;
//               tr.RunAllTests();
// 
//               JT_SCOPE("TEST: hello {}", "world");
//               JT_CHECK(1 == 1);
//               JT_CHECK_EQ(tr.FailCount(), 0);
//               JT_CHECK("abc"[1] == 'b', "...actual {}", "abc"[1]);
// 
//               return 0;
//           }
// 
//           JT_TEST_ENTRY("a given-when-then style test") {
//               JT_GIVEN( "two identical integers: {}", 11 );
//               JT_WHEN( "the values are compared for equality" );
//               JT_THEN( "the result is true" ); 
//               JT_CHECK(11 == 11);
//           }
//               
//      - see just_test_it_please.cpp for examples.
//
// History:
// - Version 0.1: 9/25/2024
//    - initial release
//======================================================================================
#pragma once

#include <string>
#include <memory>
#include <vector>
#include <map>
#include <stack>
#include <functional>
#include <format>
#include <cassert>
#include <sstream>
#include <iterator>


struct JtScope {
    struct ScopeNode;
    struct EventArgs;
    typedef std::shared_ptr<ScopeNode> NodePtr;
    typedef std::function<void(const EventArgs&)> EventListener;

    struct NamedData {
        template<typename T>
        inline NamedData(const std::string_view &name, T* data)
            : Name(name), TypeId(GetTypeId<T>()), Data(data) {}

        inline NamedData()
            : TypeId{}, Data{} {}

        template<typename T>
        static inline const void* GetTypeId() {
            static int something_static;
            return &something_static;
        }

        template<typename T>
        T* AsPtr() {
            return Data != nullptr && TypeId == GetTypeId<T>()
                ? (T*)Data : nullptr;
        }
        std::string         Name;
        const void*         TypeId;
        void*               Data;
    };

    struct ConstructorArgs {
        std::string                 File;
        int                         Line;
        std::string                 Text;
        NamedData                   Data;
    };

    struct EventInfo {
        size_t Count;      // All times fired from node or descendent
        size_t FireCount;  // Times fired from this node
    };

    struct EventArgs {
        std::string             Name;
        NodePtr                 Scope;
        NodePtr                 ListenerScope;
    };

    struct ScopeNode {
        inline ScopeNode(const ConstructorArgs arg, NodePtr parent)
            : File(arg.File), Line(arg.Line), 
            Text(arg.Text), Data(arg.Data), Parent(parent) {
        }

        inline bool IsOpen() {
            assert(Event["open"].FireCount == 1);
            assert(Event["close"].Count < Event["open"].Count);
            return Event["close"].FireCount == 0;
        }

        inline size_t FailCount() {
            return Event["fail"].Count;
        }

        std::string                         File;
        int                                 Line;
        std::string                         Text;
        NamedData                             Data;
        NodePtr                             Parent;
        std::vector<EventListener>          Listeners;
        std::map<std::string, EventInfo>    Event;
    };


    inline JtScope(const ConstructorArgs args, bool isRoot = false)
        : Node(std::make_shared<ScopeNode>(args, 
            isRoot || GetStack().size() == 0 ? nullptr : GetStack().top())),
        File(Node->File), Line(Node->Line), Text(Node->Text), Data(Node->Data),
        Listeners(Node->Listeners), Event(Node->Event) {
        assert(Node->Parent == nullptr || Node->Parent->IsOpen());
        GetStack().push(Node);
        FireEvent("open");
    }

    inline ~JtScope() {
        if (Event["close"].FireCount == 0) {
            Close();
        }
    }
    
    inline void Close() {
        assert(GetStack().top() == Node);
        FireEvent("close");
        GetStack().pop();
        Data.Data = nullptr;
    }

    inline void FireEvent(const std::string& name) {
        ++Event[name].FireCount;
        for (auto n = Node; n != nullptr; n = n->Parent) {
            ++n->Event[name].Count;
            for (auto func : n->Listeners) {
                func(EventArgs(name, Node, n));
            }
        }
    }
    static inline std::stack<NodePtr>& GetStack() {
        static thread_local std::stack<NodePtr> st;
        return st;
    }

    inline void AddListener(const EventListener& l) {
        Listeners.push_back(l);
    }
    
    inline bool IsOpen() {
        return Node->IsOpen();
    }

    inline size_t FailCount() {
        return Node->FailCount();
    }

    NodePtr                             Node;
    std::string&                        File;
    int&                                Line;
    std::string&                        Text;
    NamedData&                          Data;
    std::vector<EventListener>&         Listeners;
    std::map<std::string, EventInfo>&   Event;
};

typedef JtScope::NodePtr JtScopePtr;

template <typename TContainer>
class JtIteration {
public:
    JtIteration(const TContainer& baseContainer, size_t maxFail = 1)
        : m_containerCopy(baseContainer), m_maxFail(std::max(size_t(1), maxFail)), m_scope({}) {
    }

    class Iterator {
    public:
        using iterator_category = std::forward_iterator_tag;
        using value_type = TContainer::const_iterator::value_type;
        using difference_type = std::ptrdiff_t;
        using pointer = const value_type*;
        using reference = const value_type&;

        Iterator(TContainer::const_iterator it, TContainer::const_iterator itEnd, JtScopePtr testNode, size_t maxFail)
            : m_it(it), m_itEnd(itEnd), m_testNode(testNode), m_maxFail(maxFail) {
        }

        reference operator*() const { return *m_it; }
        pointer operator->() const { return &*m_it; }

        Iterator& operator++() {
            if (m_testNode != nullptr && m_testNode->Event["fail"].Count >= m_maxFail) {
                m_it = m_itEnd;
            }
            else {
                ++m_it;
            }
            return *this;
        }

        Iterator operator++(int) {
            Iterator temp = *this;
            ++(*this);
            return temp;
        }

        bool operator==(const Iterator& other) const { return m_it == other.m_it; }
        bool operator!=(const Iterator& other) const { 
            return m_it != other.m_it; 
        }

    private:
        TContainer::const_iterator      m_it;
        TContainer::const_iterator      m_itEnd;
        JtScopePtr                      m_testNode;
        size_t                          m_maxFail;
    };

    Iterator begin() { return Iterator(m_containerCopy.begin(), m_containerCopy.end(), m_scope.Node, m_maxFail); }
    Iterator end() { return Iterator(m_containerCopy.end(), m_containerCopy.end(), nullptr, 0); }

private:
    const TContainer    m_containerCopy;
    size_t              m_maxFail;
    JtScope             m_scope;
};

namespace Jt {
    template <typename T>
    JtIteration<T> iterate(const T& container, size_t maxFailures = 0) {
        return JtIteration(container, maxFailures);
    }

    template<typename... Args>
    std::string format(const std::string_view fmt, Args... formatItems)
    {
        return std::vformat(fmt, std::make_format_args(std::forward<Args>(formatItems)...));
    }

    constexpr std::string_view format(const std::string_view message = "")
    {
        return message;
    }
 
    struct SpaceAndWord {
        std::string space;
        std::string word;
        inline size_t size() {
            return space.size() + word.size();
        }
    };
    inline std::ostream& operator<< (std::ostream& os, SpaceAndWord& toks) {
        os << toks.space << toks.word;
        return os;
    }

    inline std::istream& operator>> (std::istream& is, SpaceAndWord& toks)
    {
        toks = {};
        size_t leading_spaces = 0;
        while (is.peek() == ' ') {
            ++leading_spaces;
            is.get();
        }
        if (is >> toks.word && leading_spaces) {
            toks.space = std::string(leading_spaces, ' ');
        }
        return is;
    }

    inline std::vector<std::string> getWrappedTextLines(const std::string &text, size_t line_length = 72)
    {
        std::vector<std::string> result;
        std::istringstream lines(text);
        std::string line;
        while (std::getline(lines, line)) {
            std::ostringstream oline;
            std::istringstream words(line);
            SpaceAndWord spaceAndWord;
            if (words >> spaceAndWord) {
                oline << spaceAndWord;
                size_t space_left = line_length - spaceAndWord.size();
                while (words >> spaceAndWord) {
                    if (space_left < spaceAndWord.size()) {
                        result.push_back(oline.str());
                        oline = {};
                        oline << spaceAndWord.word;
                        space_left = line_length - spaceAndWord.word.size();
                    }
                    else {
                        oline << spaceAndWord;
                        space_left -= spaceAndWord.size();
                    }
                }
                result.push_back(oline.str());
            }
        }
        return result;
    }

    inline std::string getWrappedText(const std::string& text, size_t line_length = 72) {
        std::string result = "";
        auto sep = "";
        for (auto line : getWrappedTextLines(text, line_length)) {
            result.append(sep);
            result.append(line);
            sep = "\n";
        }
        return result;
    }

    inline std::string getStackTrace(JtScopePtr node, bool allLevels = true, size_t atColumn = 85) {
        std::stack<std::string> prefixes;
        std::stack<std::string> texts;
        std::stack<std::string> ats;
        size_t longestPrefix = 0;
        for (auto n = node; n != nullptr; n = n->Parent) {
            if (n == node) {
                prefixes.push(node->Event["pass"].Count == 0 ? "FAIL: " : "PASS: ");
                texts.push(n->Text);
            }
            else {
                std::istringstream istr(n->Text);
                SpaceAndWord sw;
                if (!(istr >> sw)) {
                    // empty or whitespace, skip this node
                    continue;
                }
                size_t prefixLen = 0;

                if (sw.word.back() == ':') {
                    // First word ending with a colon is part of the prefix
                    prefixLen += sw.word.size();
                    if (istr >> sw) {
                        // So is the space to the next word
                        prefixLen += sw.space.size();
                    }
                }
                prefixes.push(n->Text.substr(0, prefixLen));
                texts.push(n->Text.substr(prefixLen));
            }
            longestPrefix = std::max(longestPrefix, prefixes.top().size());
            std::string atText = "";
            if (n->File != "") {
                auto file = n->File;
                auto jFile = std::max(std::max(size_t(0), file.rfind('/') + 1), file.rfind('\\') + 1);
                atText = std::format("   at {}({})", &file[jFile], n->Line);
            }
            ats.push(atText);
            if (!allLevels)
                break;
        }
        std::string result;
        if (allLevels) {
            result = std::string(80, '-');
            result.append("\n");
        }
        auto prefixColWidth = std::min(size_t(16), longestPrefix);
        while (texts.size() > 0) {
            auto prefix = prefixes.top(); prefixes.pop();
            auto at = ats.top(); ats.pop();
            auto text = texts.top(); texts.pop();
            for (auto textLine : getWrappedTextLines(text, atColumn - prefixColWidth)) {
                result.append(std::format("{:>{}}{:{}}{}\n",
                    prefix, prefixColWidth,
                    textLine, atColumn - prefixColWidth,
                    at));
                prefix = at = ""; // first line only
            }
        }
        return result;
    }
}

struct JtTestEntry {
    JtTestEntry(std::string file, int line, std::function<void()> entry, std::initializer_list<std::string_view> names) :
        File(file), Line(line), Func(entry), Names{ names } {
        Instances().push_back(*this);
    }
    std::string NamesStr() {
        std::string res = "";
        auto sep = "";
        for (auto name : Names) {
            res.append(sep);
            res.append(name);
            sep = ", ";
        }
        return res;
    }
    bool HasName(const std::string_view& name) {
        for (auto n : Names)
            if (n == name)
                return true;
        return false;
    }
    std::string                         File;
    int                                 Line;
    std::function<void()>               Func;
    std::vector<std::string_view>       Names;
    static inline std::vector<JtTestEntry>& Instances() {
        static std::vector<JtTestEntry> instances;
        return instances;
    }
};


struct JtTestRunner : JtScope {
    inline JtTestRunner() : JtScope({}, true),
        ReportToStdout(false),
        ReportSuccess(false) {
        AddListener([this](const JtScope::EventArgs& e) { OnEvent(e); });
    }

    virtual void OnEvent(const JtScope::EventArgs& e) {
        if (ReportToStdout) {
            if (e.Name == "fail" || (e.Name == "pass" && ReportSuccess)) {
                bool lastLevelOnly = PrevReported && PrevReported->Parent == e.Scope->Parent;
                printf("%s", Jt::getStackTrace(e.Scope, !lastLevelOnly).c_str());
                PrevReported = e.Scope;
            }
            else if (e.Name == "close" && e.Scope == Node) {
                printf("===========================\nTEST RESULTS:\n");
                for (auto ev : { "pass", "fail" }) {
                    printf("   %s: %d\n", ev, (int)Event[ev].Count);
                }
            }
        }
    }

    inline void RunAllTests(const std::vector<std::string>& names = {"-skip"}) {
        for (auto& t : JtTestEntry::Instances()) {
            bool requested = false;
            bool hasRequest = false;
            bool denied = false;
            for (auto name : names) {
                if (name.size() > 0 && name[0] == '-') {
                    if (t.HasName(name.substr(1))) {
                        denied = true;
                        break;
                    }
                }
                else {
                    hasRequest = true;
                    if (t.HasName(name)) {
                        requested = true;
                    }
                }
            }
            if (!denied && (requested || !hasRequest)) {
                JtScope scope({ t.File, t.Line, std::string("TEST: ") + t.NamesStr() });
                t.Func();
            }
        }
    }

    bool ReportToStdout;
    bool ReportSuccess;
    JtScopePtr PrevReported;
};

#define JT_FORMAT(...) std::string(Jt::format(__VA_ARGS__))

// Macro helpers
#define JT_PP_CAT_INNER(a, b) a##b
#define JT_PP_CAT(a, b) JT_PP_CAT_INNER(a,b)
#define JT_PP_LOCAL(NAME) JT_PP_CAT(jt_ ## NAME, __LINE__ )

#define JT_TEST_ENTRY(...) \
    void JT_PP_LOCAL(testFunc) (); \
    JtTestEntry JT_PP_LOCAL(te)(__FILE__, __LINE__, JT_PP_LOCAL(testFunc), {__VA_ARGS__}); \
    void JT_PP_LOCAL(testFunc) ()

#define JT_SCOPE(...) \
    JtScope JT_PP_LOCAL(scope) ({ __FILE__, __LINE__, JT_FORMAT(__VA_ARGS__) })

#define JT_DATA(NAME, PDATA, ...) \
    JtScope JT_PP_LOCAL(scope) ({ __FILE__, __LINE__, JT_FORMAT(__VA_ARGS__), JtScope::NamedData(NAME, PDATA)})

#define JT_PP_SCOPE_TAG(TAG, FILE, LINE, ...) \
    JtScope JT_PP_LOCAL(scope) ({ FILE, LINE, std::string(TAG " ") + JT_FORMAT(__VA_ARGS__) })

#define JT_GIVEN(...) JT_PP_SCOPE_TAG("GIVEN:", __FILE__, __LINE__, __VA_ARGS__)
#define JT_WHEN(...)  JT_PP_SCOPE_TAG("WHEN:", __FILE__, __LINE__, __VA_ARGS__)
#define JT_THEN(...)  JT_PP_SCOPE_TAG("THEN:", __FILE__, __LINE__, __VA_ARGS__)
#define JT_WITH(...)  JT_PP_SCOPE_TAG("with:", __FILE__, __LINE__, __VA_ARGS__)

#define JT_CHECK(CONDITION, ...) \
    [&]() {\
        auto detail = JT_FORMAT(__VA_ARGS__); \
        JtScope scope({ __FILE__, __LINE__, detail != ""  \
            ? std::format("JT_CHECK( {} )\ndetail: {}", #CONDITION, detail) \
            : std::string("JT_CHECK( " #CONDITION " )") }); \
        bool result = CONDITION; \
        scope.FireEvent(result ? "pass" : "fail"); \
        return result; \
    }()

#define JT_CHECK_BINOP(OPERATOR, LHS, RHS) \
    [&]() { auto lhs = LHS; auto rhs = RHS; bool result = lhs OPERATOR rhs; \
        JtScope scope ({ __FILE__, __LINE__, \
            std::format("JT_CHECK( {} " #OPERATOR " {} )\n   lhs: {}\n   rhs: {}", \
                #LHS, #RHS, lhs, rhs)}); \
        scope.FireEvent(result ? "pass" : "fail"); \
        return result; \
    }()

#define JT_CHECK_EQ(LHS, RHS) JT_CHECK_BINOP(==, LHS, RHS)
#define JT_CHECK_NEQ(LHS, RHS) JT_CHECK_BINOP(!=, LHS, RHS)

#define JT_DEFINE_ENUM(T_TYPE,...) \
    template <> \
    struct std::formatter<T_TYPE> : std::formatter<std::string> { \
        using enum T_TYPE; \
        auto format(const T_TYPE& arg, std::format_context& ctx) const { \
            static std::vector<T_TYPE> vals{ __VA_ARGS__ }; \
            static std::vector<std::string> strs = []() { \
                std::vector<std::string> result; \
                std::istringstream ival(#__VA_ARGS__); \
                std::string tok; \
                while (std::getline(ival, tok, ',')) { \
                    std::istringstream(tok) >> tok; /*trim leading*/\
                    result.push_back(tok); \
                } \
                return result; \
            }(); \
            for (auto j = 0; j < vals.size(); ++j) { \
                if (vals[j] == arg) { \
                    return std::formatter<std::string>::format(strs[j], ctx); \
                } \
            } \
            return std::formatter<std::string>::format(std::format(#T_TYPE "::enum({})", (int64_t)arg), ctx); \
        } \
    }
