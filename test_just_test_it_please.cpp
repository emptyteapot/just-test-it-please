#include "just_test_it_please.h"

//------ jt tests ----------------------------------------
JT_TEST_ENTRY("jt-test", "Hello world") {
    int one = 1;
    JT_CHECK(one == 1);
    JT_CHECK_NEQ(one, 2);
    JT_CHECK(one != 3, "...with literal '3' and one='{}'", one);
}

JT_TEST_ENTRY("jt-test", "Use a local test runner") {

    JT_GIVEN("a local test runner");
    JT_WHEN("some tests in the test runner fail");
    JT_THEN("the failures do not propagate beyond the local scope "
        "and the correct failure count is observed");

    JtTestRunner tr;

    JT_CHECK(true);
    JT_CHECK(false);
    JT_CHECK(false);

    // Close the test runner before executing our checks,
    // because we want our tests to propagate OUTSIDE the testrunner.
    tr.Close();

    JT_CHECK_EQ(tr.FailCount(), 2);
}

JT_TEST_ENTRY("jt-test", "word wrapping") {
    JT_CHECK_EQ(Jt::getWrappedText("abc"), "abc");
    JT_CHECK_EQ(Jt::getWrappedText("a  b c"), "a  b c");
    JT_CHECK_EQ(Jt::getWrappedText("a\n b\n  c d e f g h", 4), "a\n b\n  c\nd e\nf g\nh");
}

JT_TEST_ENTRY("jt-test", "Jt::iterate stops iterating after specified failure count") {
    std::vector<int> vals{ 1,2,3,4,5,6,7,8 };
    JT_GIVEN("a vector of values [1,2,3,4,5,6,7,8]");
    JT_THEN("the values iterated with Jt::iterate fail with 4 and 6");
    JT_WHEN("the last checked value will be 4, 6, or 8");

    for (size_t maxFailures = 0; maxFailures <= 10; ++maxFailures) {
        int lastCheckedVal = 0;
        {
            JtScope catchFailureScope({}, true);
            for (auto val : Jt::iterate(vals, maxFailures)) {
                lastCheckedVal = val;
                JT_CHECK(val != 4);
                JT_CHECK(val != 6);
            }
        }
        switch (maxFailures) {
        case 0:  // fallthrough
        case 1:  JT_CHECK(lastCheckedVal == 4); break;
        case 2:  JT_CHECK(lastCheckedVal == 6); break;
        default: JT_CHECK(lastCheckedVal == 8); break;
        }
    }
}


//------ enum tests ----------------------------------------
namespace {
    namespace TestJt {
        enum class Enum {
            kZero,
            kOne,
            kTwo,
        };
        struct TestStruct {
            enum TestEnum : uint64_t {
                kRed = 2,
                kBlue = 1,
            };
        };
    }

    enum class TestJtEnum {
        kThree = 3,
        kFour = 4,
        kFive = 5,
    };
}

JT_DEFINE_ENUM(TestJt::Enum, kZero,
    kOne,
    kTwo);

JT_DEFINE_ENUM(TestJt::TestStruct::TestEnum, kRed, kBlue);

JT_DEFINE_ENUM(TestJtEnum, kThree,
    kFour,
    kFive);

namespace {
    JT_TEST_ENTRY("jt-test", "test enum formatting") {
        JT_CHECK_EQ(std::format("{}", TestJt::Enum::kOne), "kOne");
        JT_CHECK_EQ(std::format("{}", TestJt::Enum::kTwo), "kTwo");
        JT_CHECK_EQ(std::format("{}", TestJtEnum::kThree), "kThree");
        JT_CHECK_EQ(std::format("{}", TestJtEnum::kFour), "kFour");
        JT_CHECK_EQ(std::format("{}", TestJtEnum::kFive), "kFive");
        JT_CHECK_EQ(std::format("{}", (TestJtEnum)1), "TestJtEnum::enum(1)");
        JT_CHECK_EQ(std::format("{}", (TestJtEnum)-1), "TestJtEnum::enum(-1)");
        JT_CHECK_EQ(std::format("{}", TestJt::TestStruct::TestEnum::kRed), "kRed");
        JT_CHECK_EQ(std::format("{}", TestJt::TestStruct::TestEnum::kBlue), "kBlue");
    }
}
