#define DELEGATE_ARGS_SIZE 24
#define DELEGATE_ARGS_ALIGN 8
#include "delegate.h"

#ifdef WIN32
#define DO_NOT_USE_WMAIN
#define CATCH_CONFIG_WINDOWS_CRTDBG
#endif
#define CATCH_CONFIG_RUNNER
#define CATCH_CONFIG_FAST_COMPILE
#include "catch.hpp"

namespace StaticFixture
{
    static bool ran = false;
    static int in = 0;

    static void init()
    {
        ran = false;
        in = 0;
    }

    static void func_void() 
    {
        ran = true;
    }
    static int func_int() 
    {
        ran = true;
        return 17;
    }
    static void func_void_int(int i)
    {
        ran = true;
        in = i;
    }
    static int func_int_int(int i)
    {
        ran = true;
        in = i;
        return 101 + i;
    }
};

class ClassFixture
{
public:
    bool ran;
    int in;

    static int construct_count;
    static int destruct_count;

    ClassFixture() :
        ran(false),
        in(0)
    {
        construct_count++;
    }

    ClassFixture(const ClassFixture &other)
    {
        construct_count++;
        if (this == &other)
        {
            return;
        }
        this->ran = other.ran;
        this->in = other.in;
    }

    ~ClassFixture()
    {
        destruct_count++;
    }

    static void reset_counts()
    {
        construct_count = 0;
        destruct_count = 0;
    }

    void func_void()
    {
        ran = true;
    }
    int func_int()
    {
        ran = true;
        return 17;
    }
    void func_void_int(int i)
    {
        ran = true;
        in = i;
    }
    int func_int_int(int i)
    {
        ran = true;
        in = i;
        return 101 + i;
    }
};
int ClassFixture::construct_count = 0;
int ClassFixture::destruct_count = 0;

/**
 * Test nove-only classes using smart pointers.
 */
TEST_CASE("Smart pointers", "[smart_pointer]")
{
    ClassFixture::reset_counts();
    SECTION("Assign")
    {
        {
            std::unique_ptr<ClassFixture> cf(new ClassFixture);
            delegate::Func<delegate::NonCopyableType, int, int> test([cf = std::move(cf)](int i)
            {
                return cf->func_int_int(i);
            });

            REQUIRE(ClassFixture::construct_count == 1);
            REQUIRE(test(1234) == 1335);
            REQUIRE(ClassFixture::destruct_count == 0);
        }
        REQUIRE(ClassFixture::construct_count == 1);
        REQUIRE(ClassFixture::destruct_count == 1);
    }
    SECTION("Functor Move")
    {
        {
            delegate::Func<delegate::NonCopyableType, int, int> test;
            {
                std::unique_ptr<ClassFixture> cf(new ClassFixture);
                auto f = [cf = std::move(cf)](int i)
                {
                    return cf->func_int_int(i);
                };
                test = (std::move(f));
            }

            REQUIRE(ClassFixture::construct_count == 1);
            REQUIRE(test(1234) == 1335);
            REQUIRE(ClassFixture::destruct_count == 0);
        }
        REQUIRE(ClassFixture::construct_count == 1);
        REQUIRE(ClassFixture::destruct_count == 1);
    }
    SECTION("Delegate Move")
    {
        {
            delegate::Func<delegate::NonCopyableType, int, int> test;
            {
                std::unique_ptr<ClassFixture> cf(new ClassFixture);
                auto f = [cf = std::move(cf)](int i)
                {
                    return cf->func_int_int(i);
                };
                test = (std::move(f));
                delegate::Func<delegate::NonCopyableType, int, int> temp(std::move(test));

                REQUIRE(temp(1234) == 1335);

                REQUIRE(ClassFixture::destruct_count == 0);
            }

            REQUIRE(ClassFixture::construct_count == 1);
            REQUIRE(test(1234) == 0);
            REQUIRE(ClassFixture::destruct_count == 1);
        }
        REQUIRE(ClassFixture::construct_count == 1);
        REQUIRE(ClassFixture::destruct_count == 1);
    }
}

/**
 * Show that delegates can indeed store up to their maximum size.
 */
TEMPLATE_TEST_CASE("Storage Test", "[storage_test]", delegate::NonMovableType)
{
    uint32_t param0 = 111;
    uint32_t param1 = 222;
    uint32_t param2 = 333;
    uint32_t param3 = 444;
    uint32_t param4 = 555;
    uint32_t param5 = 666;

    static uint32_t sparam0 = 0;
    static uint32_t sparam1 = 0;
    static uint32_t sparam2 = 0;
    static uint32_t sparam3 = 0;
    static uint32_t sparam4 = 0;
    static uint32_t sparam5 = 0;

    auto lambda1 = [](){};
    auto lambda4 = [param0](){sparam0 = param0;};
    auto lambda8 = [param0, param1](){sparam0 = param0; sparam1 = param1;};
    auto lambda12 = [param0, param1, param2](){sparam0 = param0; sparam1 = param1; sparam2 = param2;};
    auto lambda16 = [param0, param1, param2, param3]()
    {
        sparam0 = param0; sparam1 = param1; sparam2 = param2; sparam3 = param3;
    };
    auto lambda20 = [param0, param1, param2, param3, param4]()
    {
        sparam0 = param0; sparam1 = param1; sparam2 = param2; sparam3 = param3; sparam4 = param4;
    };
    auto lambda24 = [param0, param1, param2, param3, param4, param5]()
    {
        sparam0 = param0; sparam1 = param1; sparam2 = param2; sparam3 = param3; sparam4 = param4; sparam5 = param5;
    };

    static_assert(sizeof(lambda1) == 1, "A capture-less lambda is assumed to be one byte");
    static_assert(sizeof(lambda4) == 4, "A 4-byte capture should result in a 4-byte lambda");
    static_assert(sizeof(lambda8) == 8, "A 8-byte capture should result in a 8-byte lambda");
    static_assert(sizeof(lambda12) == 12, "A 12-byte capture should result in a 12-byte lambda");
    static_assert(sizeof(lambda16) == 16, "A 16-byte capture should result in a 16-byte lambda");
    static_assert(sizeof(lambda20) == 20, "A 20-byte capture should result in a 20-byte lambda");
    static_assert(sizeof(lambda24) == 24, "A 24-byte capture should result in a 24-byte lambda");
    static_assert(sizeof(delegate::FunctorArgs) == 24, "This test assumes 24-bytes of storage");

    delegate::Func<TestType, void> delegate1 = lambda1;
    delegate1();
    REQUIRE(sparam0 == 0);
    REQUIRE(sparam1 == 0);
    REQUIRE(sparam2 == 0);
    REQUIRE(sparam3 == 0);
    REQUIRE(sparam4 == 0);
    REQUIRE(sparam5 == 0);

    delegate::Func<TestType, void> delegate4 = lambda4;
    delegate4();
    REQUIRE(sparam0 == 111);
    REQUIRE(sparam1 == 0);
    REQUIRE(sparam2 == 0);
    REQUIRE(sparam3 == 0);
    REQUIRE(sparam4 == 0);
    REQUIRE(sparam5 == 0);
    sparam0 = 0;

    delegate::Func<TestType, void> delegate8 = lambda8;
    delegate8();
    REQUIRE(sparam0 == 111);
    REQUIRE(sparam1 == 222);
    REQUIRE(sparam2 == 0);
    REQUIRE(sparam3 == 0);
    REQUIRE(sparam4 == 0);
    REQUIRE(sparam5 == 0);
    sparam0 = 0;
    sparam1 = 0;

    delegate::Func<TestType, void> delegate12 = lambda12;
    delegate12();
    REQUIRE(sparam0 == 111);
    REQUIRE(sparam1 == 222);
    REQUIRE(sparam2 == 333);
    REQUIRE(sparam3 == 0);
    REQUIRE(sparam4 == 0);
    REQUIRE(sparam5 == 0);
    sparam0 = 0;
    sparam1 = 0;
    sparam2 = 0;

    delegate::Func<TestType, void> delegate16 = lambda16;
    delegate16();
    REQUIRE(sparam0 == 111);
    REQUIRE(sparam1 == 222);
    REQUIRE(sparam2 == 333);
    REQUIRE(sparam3 == 444);
    REQUIRE(sparam4 == 0);
    REQUIRE(sparam5 == 0);
    sparam0 = 0;
    sparam1 = 0;
    sparam2 = 0;
    sparam3 = 0;

    delegate::Func<TestType, void> delegate20 = lambda20;
    delegate20();
    REQUIRE(sparam0 == 111);
    REQUIRE(sparam1 == 222);
    REQUIRE(sparam2 == 333);
    REQUIRE(sparam3 == 444);
    REQUIRE(sparam4 == 555);
    REQUIRE(sparam5 == 0);
    sparam0 = 0;
    sparam1 = 0;
    sparam2 = 0;
    sparam3 = 0;
    sparam4 = 0;

    delegate::Func<TestType, void> delegate24 = lambda24;
    delegate24();
    REQUIRE(sparam0 == 111);
    REQUIRE(sparam1 == 222);
    REQUIRE(sparam2 == 333);
    REQUIRE(sparam3 == 444);
    REQUIRE(sparam4 == 555);
    REQUIRE(sparam5 == 666);
}

/**
 * Test that constructors and destructors.  Also test that delegate copies work correctly.
 */
TEMPLATE_TEST_CASE("Construct / Destruct", "[construct_destruct]", delegate::NonMovableType)
{
    SECTION("Construct / Destruct / Same Copy")
    {
        ClassFixture::reset_counts();
        {
            static bool state_ran = false;
            static int state_in = 0;
            
            /*
             * This fixture will be created and destroyed, the temporary lambda assigned to f will also be created and
             * destroyed, taking its copy of ClassFixture through a cycle, and the placement new in the constructor for
             * the delegate will also create a lambda, but there are only destructor calls for full type, and copies
             * only call constructors for full types.
             */
            ClassFixture fixture;
            delegate::Func<TestType, int, bool, int> f = [fixture](const bool dump_state, const int i) mutable
            {
                if(dump_state)
                {
                    state_ran = fixture.ran;
                    state_in = fixture.in;

                    return 0;
                }
                else
                {
                    return fixture.func_int_int(i);
                }
            };

            REQUIRE(ClassFixture::construct_count == 3);
            REQUIRE(ClassFixture::destruct_count == 1);

            REQUIRE(f(false, 123) == 224);
            REQUIRE(state_ran == false);
            REQUIRE(state_in == 0);
            REQUIRE(f(true, 123) == 0);
            REQUIRE(state_ran == true);
            REQUIRE(state_in == 123);

            /*
             * Make a new delegate and copy over the one above.  No fixture constructors or destructors are called.
             */
            delegate::Func<TestType, int, bool, int> f_copy = f;
            REQUIRE(f_copy(true, 987) == 0);
            REQUIRE(state_ran == true);
            REQUIRE(state_in == 123);

            delegate::Func<TestType, int, bool, int> fff;

            /*
             * The new delegate keeps chugging along.
             */
            REQUIRE(f_copy(false, 555) == 656);

            /*
             * The original delegate is unaffected.
             */
            REQUIRE(f(true, 123) == 0);
            REQUIRE(state_ran == true);
            REQUIRE(state_in == 123);

            REQUIRE(ClassFixture::construct_count == 4);
            REQUIRE(ClassFixture::destruct_count == 1);

            /*
             * Perform a copy assignemnt.
             */
            delegate::Func<TestType, int, bool, int> f_copy_assign;
            f_copy_assign = f;
        }

        /*
         * Show that the trivial version does less work while the full version does the full monty.
         */
        REQUIRE(ClassFixture::construct_count == 5);
        REQUIRE(ClassFixture::destruct_count == 5);
    }
}

/**
 * Test trivial functions (no captures).
 */
TEMPLATE_TEST_CASE("Trivial Function", "[trivial_function]", delegate::NonMovableType)
{
    SECTION("void")
    {
        StaticFixture::init();
        delegate::Func<TestType, void> f(&StaticFixture::func_void);
        f();
        REQUIRE(StaticFixture::ran == true);
        REQUIRE(StaticFixture::in == 0);
    }
    SECTION("int")
    {
        StaticFixture::init();
        delegate::Func<TestType, int> f(&StaticFixture::func_int);
        REQUIRE(f() == 17);
        REQUIRE(StaticFixture::ran == true);
        REQUIRE(StaticFixture::in == 0);
    }
    SECTION("void int")
    {
        StaticFixture::init();
        delegate::Func<TestType, void, int> f(&StaticFixture::func_void_int);
        f(21);
        REQUIRE(StaticFixture::ran == true);
        REQUIRE(StaticFixture::in == 21);
    }
    SECTION("int int")
    {
        StaticFixture::init();
        delegate::Func<TestType, int, int> f(&StaticFixture::func_int_int);
        REQUIRE(f(33) == 134);
        REQUIRE(StaticFixture::ran == true);
        REQUIRE(StaticFixture::in == 33);
    }
}

/**
 * Test class functions (implicit this captures and explicit captures).
 */
TEMPLATE_TEST_CASE("Class Function", "[class_function]", delegate::NonMovableType)
{
    SECTION("void")
    {
        ClassFixture fixture;
        delegate::Func<TestType, void> f(std::bind(&ClassFixture::func_void, &fixture));
        f();
        REQUIRE(fixture.ran == true);
        REQUIRE(fixture.in == 0);
    }
    SECTION("int")
    {
        ClassFixture fixture;
        delegate::Func<TestType, int> f(std::bind(&ClassFixture::func_int, &fixture));
        REQUIRE(f() == 17);
        REQUIRE(fixture.ran == true);
        REQUIRE(fixture.in == 0);
    }
    SECTION("void int")
    {
        ClassFixture fixture;
        delegate::Func<TestType, void, int> f(std::bind(&ClassFixture::func_void_int,
                                                        &fixture,
                                                        std::placeholders::_1));
        f(21);
        REQUIRE(fixture.ran == true);
        REQUIRE(fixture.in == 21);
    }
    SECTION("int int")
    {
        ClassFixture fixture;
        delegate::Func<TestType, int, int> f(std::bind(&ClassFixture::func_int_int,
                                                       &fixture,
                                                       std::placeholders::_1));
        REQUIRE(f(33) == 134);
        REQUIRE(fixture.ran == true);
        REQUIRE(fixture.in == 33);
    }
}

/**
 * Test lambda functions.
 */
TEMPLATE_TEST_CASE("Lambda", "[lambda]", delegate::NonMovableType)
{
    SECTION("void")
    {
        ClassFixture fixture;
        delegate::Func<TestType, void> f([&fixture](){fixture.func_void();});
        f();
        REQUIRE(fixture.ran == true);
        REQUIRE(fixture.in == 0);
    }
    SECTION("int")
    {
        ClassFixture fixture;
        delegate::Func<TestType, int> f([&fixture](){return fixture.func_int();});
        REQUIRE(f() == 17);
        REQUIRE(fixture.ran == true);
        REQUIRE(fixture.in == 0);
    }
    SECTION("void int")
    {
        ClassFixture fixture;
        delegate::Func<TestType, void, int> f([&fixture](int i){fixture.func_void_int(i);});
        f(21);
        REQUIRE(fixture.ran == true);
        REQUIRE(fixture.in == 21);
    }
    SECTION("int int")
    {
        ClassFixture fixture;
        delegate::Func<TestType, int, int> f([&fixture](int i){return fixture.func_int_int(i);});
        REQUIRE(f(33) == 134);
        REQUIRE(fixture.ran == true);
        REQUIRE(fixture.in == 33);
    }
}

/**
 * Test uninitialized delegates.
 */
TEMPLATE_TEST_CASE("Trivial Function Default", "[trivial_function_default]", delegate::NonMovableType)
{
    SECTION("default void")
    {
        delegate::Func<TestType, void> f;
        f();
    }
    SECTION("trivial default int")
    {
        delegate::Func<TestType, int> f;
        REQUIRE(f() == 0);
    }
    SECTION("default void int")
    {
        delegate::Func<TestType, void, int> f;
        f(1);
    }
    SECTION("default int int")
    {
        delegate::Func<TestType, int, int> f;
        REQUIRE(f(1) == 0);
    }
}

/**
 * Test type forwarding delegates.
 */
TEMPLATE_TEST_CASE("Semi-Perfect Forwarding", "[semi_perfect_forwarding]", delegate::NonMovableType)
{
    SECTION("non-reference type to non-reference argument")
    {
        ClassFixture::reset_counts();
        {
            delegate::Func<TestType, void, ClassFixture> f([](ClassFixture fixture){fixture.func_void();});
            ClassFixture fixture;
            REQUIRE(ClassFixture::construct_count == 1);
            REQUIRE(ClassFixture::destruct_count == 0);
            f(fixture);
            REQUIRE(ClassFixture::construct_count == 3);
            REQUIRE(ClassFixture::destruct_count == 2);
        }
    }
    SECTION("non-reference type to rvalue reference argument")
    {
        ClassFixture::reset_counts();
        {
            delegate::Func<TestType, void, ClassFixture> f([](ClassFixture&& fixture){fixture.func_void();});
            ClassFixture fixture;
            REQUIRE(ClassFixture::construct_count == 1);
            REQUIRE(ClassFixture::destruct_count == 0);
            f(fixture);
            REQUIRE(ClassFixture::construct_count == 2);
            REQUIRE(ClassFixture::destruct_count == 1);
        }
    }
    SECTION("rvalue reference type to non-reference argument")
    {
        ClassFixture::reset_counts();
        {
            delegate::Func<TestType, void, ClassFixture&&> f([](ClassFixture fixture){fixture.func_void();});
            ClassFixture fixture;
            REQUIRE(ClassFixture::construct_count == 1);
            REQUIRE(ClassFixture::destruct_count == 0);
            f(std::move(fixture));
            REQUIRE(ClassFixture::construct_count == 2);
            REQUIRE(ClassFixture::destruct_count == 1);
        }
    }
    SECTION("rvalue reference type to rvalue reference argument")
    {
        ClassFixture::reset_counts();
        {
            delegate::Func<TestType, void, ClassFixture&&> f([](ClassFixture&& fixture){fixture.func_void();});
            ClassFixture fixture;
            REQUIRE(ClassFixture::construct_count == 1);
            REQUIRE(ClassFixture::destruct_count == 0);
            f(std::move(fixture));
            REQUIRE(ClassFixture::construct_count == 1);
            REQUIRE(ClassFixture::destruct_count == 0);
        }
    }
}

int main(int, char*[])
{
    Catch::Session session;

    Catch::ConfigData config_data;
    session.useConfigData(config_data);

    session.run();
}
