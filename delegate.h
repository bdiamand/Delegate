#pragma once
/*
 * Copyright 2019-2022
 * Authored by: Ben Diamand
 *
 * English version - you can use this for whatever you want. Attribution much
 * appreciated but not required.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 * 
 * Anyone is free to copy, modify, publish, use, compile, sell, or
 * distribute this software, either in source code form or as part of a compiled
 * binary, for any purpose, commercial or non-commercial, and by any means,
 * subject to the following conditions(s):
 *
 * ** This comment block must remain in this and derived works.
 */
#include <stdio.h>
#include <array>
#include <type_traits>
#include <utility>
#include <new>
#include <exception>

/**
 *                                                  ^^^ Rationale ^^^
 *
 * There are many examples of std::function replacements, but I was unable to find something that did exactly what
 * these classes do.  For example, this page has a comparison of a plethora of std::function replacements:
 * https://github.com/jamboree/CxxFunctionBenchmark.
 *
 * The design goals (in order) for this implementation are:
 *      Predictable memory and runtime -> fixed size and heapless
 *      Simple to use.
 *      Fast
 *      Small
 *
 * Fixed (size) delegates are a std::function alternative, with more speed / space performance but less functionality.
 * The differences compared to std:function are:
 *      (Pro) Faster than std::function for the same capture sizes
 *      (Pro) Small fixed size, never allocates
 *      (Pro) Documented implementation
 *      (Pro) Uninitialized state except is handled like std::function, with operator bool
 *      (Pro) Similar but simpler syntax to std::function
 *      (Pro / Con) No support for RTTI or exceptions (no knowledge of exceptions at all)
 *      (Con) Unable to store arbitrary sized captures - captures must fit the compile-time delegate size
 *
 * Note: See the accompanying unit tests for some good examples of use.
 */
namespace delegate
{
    /** Allow the delegate size to be specified as a compile-time constant. */
    #ifndef DELEGATE_ARGS_SIZE
     #define DELEGATE_ARGS_SIZE sizeof(int) + sizeof(int *)
     #define DELEGATE_ARGS_SIZE_UNDEF
    #endif
    #ifndef DELEGATE_ARGS_ALIGN
     #define DELEGATE_ARGS_ALIGN 8
     #define DELEGATE_ARGS_ALIGN_UNDEF
    #endif

    /**
     * Templated class representing the aligned storage of a delegate.  The intent is for all delegates to have the
     * same size, and this information is purposefully not part of the delegate signature.
     *
     * @tparam size Number of bytes of storage per delegate.
     * @tparam alignement How to align the data.
     */
    template <size_t size = DELEGATE_ARGS_SIZE, size_t alignment = DELEGATE_ARGS_ALIGN>
    struct TemplateFunctorArgs
    {
    private:
        /** The actual storage. */
        alignas(alignment) std::array<char, size> args;
    };

    #ifdef DELEGATE_ARGS_SIZE_UNDEF
     #undef DELEGATE_ARGS_SIZE
     #undef DELEGATE_ARGS_SIZE_UNDEF
    #endif
    #ifdef DELEGATE_ARGS_ALIGN_UNDEF
     #undef DELEGATE_ARGS_ALIGN
     #undef DELEGATE_ARGS_ALIGN_UNDEF
    #endif

    /** A simplifying name to make the code more readable. */
    using FunctorArgs = TemplateFunctorArgs<>;

    /**
     * Determine whether there is enough space to hold the delegate.
     * 
     * @return Returns true if there is enough space, else false.
     */
    template<typename T>
    constexpr bool can_emplace()
    {
        return (sizeof(T) <= sizeof(FunctorArgs)) &&
               (std::alignment_of<FunctorArgs>::value % std::alignment_of<T>::value) == 0;
    }

    /**
     * Determine the templated class is copyable.
     * 
     * @return Returns true if the class is copyable, else false.
     */
    template<typename T>
    constexpr bool can_copy()
    {
        return std::is_copy_constructible<T>::value;
    }

    /**
     * Reimbues a type-erased piece of memory with its original functor type.
     *
     * @tparam T The functor type.
     * @param args The memory to reimbue.
     *
     * @return Returns a reference to the (now properly typed) memory.
     */
    template<typename T>
    static T &get_typed_functor(const FunctorArgs &args)
    {
        return (T &)(args);
    }

    /**
     * Store a functor's associated captured data into a piece of type-erased memory.
     * 
     * @tparam T The functor type.
     * @param args The memory to store to.
     * @to_store The memory to store from.
     */
    template<typename T>
    static void store_functor(FunctorArgs &args, const T &to_store)
    {
        ::new (&get_typed_functor<T>(args)) T(to_store);
    }

    /**
     * Move a functor's associated captured data into a piece of type-erased memory.
     * 
     * @tparam T The functor type.
     * @param args The memory to store into.
     * @param to_move The type to move.
     */
    template<typename T>
    static void move_functor(FunctorArgs &args, T &&to_move)
    {
        ::new (&get_typed_functor<T>(args)) T(std::move(to_move));
    }

    /**
     * Call the type-erased functor (with the correct type).  This is a pure forwarding function, only passing along
     * arguments to the actual functor, i.e. a trampoline to call the real functor.
     * 
     * @tparam T The functor type.
     * @tparam Result The return type.
     * @tparam Arguments The functor argument types.
     * @param args The functor memory (i.e. function pointer or captures).
     * @param arguments The functor arguments.
     *
     * @return The functor return type.
     */
    template<typename T, typename Result, typename... Arguments>
    static Result typed_call(const FunctorArgs &args, Arguments&&... arguments)
    {
        return get_typed_functor<T>(args)(std::forward<Arguments>(arguments)...);
    }

    /** Another (smaller) name for the type-erased call function. */
    template<typename Result, typename... Arguments>
    using func_call = Result (*)(const FunctorArgs &args, Arguments&&... arguments);

    /**
     * Manual virtual table implementation.  A virtual table is useful because there are multiple functions a full
     * delegate has beyond the call function (copy, move, deletion), and storing pointers for each type erased function
     * would make the delegate larger for no real gain (delegates are for calling - the other operations are incidental).
     */
    struct Vtable
    {
        /**
         * Emits a full function static table pointer, unique to the template parameter.
         *
         * @tparam T The functor type associated with the virtual table.
         */
        template<typename T>
        inline static const Vtable &get_vtable()
        {
            // Fill in the vtable for this type - the same type winds up with the same pointers for each.
            static const Vtable vtable = 
            {
                typed_copy<T>,
                typed_move<T>,
                typed_destroy<T>
            };

            return vtable;
        }

        /** Reference to the copy function. */
        void (& copy)(FunctorArgs &lhs, const FunctorArgs &rhs);

        /** Reference to the move function. */
        void (& move)(FunctorArgs &lhs, FunctorArgs &&rhs);

        /** Reference to the destroy function. */
        void (& destroy)(FunctorArgs &args);

        /**
         * Actual code to perform a copy.
         *
         * @tparam T The functor type to copy.
         * @param lhs The reference to receive the copied data.
         * @param rhs The reference to provide the copied data.
         */
        template<typename T,
                 typename std::enable_if<can_copy<T>(), T>::type* = nullptr>
        static void typed_copy(FunctorArgs &lhs, const FunctorArgs &rhs)
        {
            store_functor<T>(lhs, get_typed_functor<T>(rhs));
        }

        /**
         * Dummy copy.
         *
         * This function exists because there are functors which cannot be copy constructed.  For example, unique_ptr
         * doesn't allow itself to be copied. If a delegate has a unique_ptr value capture, the delegate can no longer
         * be copied to another delegate; that would violate the promise unique_ptr makes that there will exist only one
         * actual pointer value.
         *
         * @tparam T The functor type to copy.
         * @param lhs The reference to receive the copied data.
         * @param rhs The reference to provide the copied data.
         */
        template<typename T,
                 typename std::enable_if<!can_copy<T>(), T>::type* = nullptr>
        static void typed_copy(FunctorArgs &, const FunctorArgs &)
        {
            std::terminate();
        }

        /**
         * Actual code to perform a move.
         *
         * @tparam T The functor type to move.
         * @param lhs The reference to receive the moved data.
         * @param rhs The reference to provide the moved data.
         */
        template<typename T>
        static void typed_move(FunctorArgs &lhs, FunctorArgs &&rhs)
        {
            move_functor<T>(lhs, std::move(get_typed_functor<T>(rhs)));
        }

        /**
         * Actual code to perform a destroy.
         *
         * @tparam T The functor type to copy.
         * @param args The memory of the functor type to destroy.
         */
        template<typename T>
        static void typed_destroy(FunctorArgs &args)
        {
            get_typed_functor<T>(args).~T();
        }
    };

    /**
     * Base delegate - usable for the less common case of delegates with non-copyable captures.
     *
     * @tparam Result The delegate return type.
     * @tparam Arguments The delegate function arguments.
     */
    template<typename Result, typename... Arguments>
    class FuncNonCopyable
    {
    public:
        /** Default constructed delegates, like std::function, are legal but uncallable. */
        inline static auto badcall = [](Arguments...) -> Result {std::terminate();};

        /** Set the call function to be the right one for the type passed. */
        template<typename T>
        void set_call_by_type(const T&)
        {
            call = &typed_call<T, Result, Arguments...>;
        }

        /** Set the call function to default to the badcall lambda. */
        void set_bad_call()
        {
            set_call_by_type(badcall);
        }

        /**
         * Set the vtable to be the right one for the type passed.
         * 
         * @tparam T The lambda type to use for setting the vtable.
         */
        template<typename T>
        void set_vtable_by_type(const T&)
        {
            vtable = &Vtable::get_vtable<T>();
        }

        /**
         * Returns whether the current call function matches the one corresponding
         * to the passed in template parameter.
         * 
         * @tparam T The type of the lambda the check against.
         * @return True if the current call function is the same as the lambda.
         */
        template<typename T>
        bool check_same_call(T&& check) const
        {
            return call == &check;
        }

        /**
         * Like std::function, returns whether it's safe to call this delegate.
         * 
         * @return True if the delegate is safe to call, else false.
         */
        explicit operator bool() const
        {
            return !check_same_call(typed_call<decltype(badcall), Result, Arguments...>);
        }

        /** Default constructor. Creates a valid (but uncallable) object. */
        FuncNonCopyable()
            : call(&typed_call<decltype(badcall), Result, Arguments...>)
            , vtable(&Vtable::get_vtable<decltype(badcall)>())
        {
        }

        /**
         * Converting from functor move constructor.
         *
         * @tparam T The functor type.
         * @param functor The functor to move.
         */
        template<typename T>
        explicit FuncNonCopyable(T &&functor) :
            call(&typed_call<T, Result, Arguments...>),
            vtable(&Vtable::get_vtable<T>())
        {
            static_assert(can_emplace<T>(), "Delegate doesn't fit.");
            static_assert(std::is_same_v<Result, std::invoke_result_t<T, Arguments...>>, "Wrong return type.");
            move_functor(args, std::move(functor));
        }

        /**
         * Move constructor.
         *
         * @param other The delegate to move from.
         */
        FuncNonCopyable(FuncNonCopyable &&other) 
            : call(other.call)
            , vtable(other.vtable)
        {
            other.vtable->move(args, std::move(other.args));

            /** Leave other in some well defined state. */
            other.set_bad_call();
        }

        /**
         * Functor move assignment operator.
         *
         * @param other The delegate to move from.
         */
        template<typename T>
        FuncNonCopyable &operator=(T &&functor)
        {
            static_assert(can_emplace<T>(), "Delegate doesn't fit.");
            static_assert(std::is_same_v<Result, std::invoke_result_t<T, Arguments...>>, "Wrong return type.");

            // Destroy whatever's currently stored before moving the parameter.
            vtable->destroy(args);
            move_functor(args, std::move(functor));

            set_call_by_type<T>(functor);
            set_vtable_by_type<T>(functor);

            return *this;
        }

        /**
         * Move assignment operator.
         * 
         * @param other The delegate to move from.
         * 
         * @return Returns a reference to this.
         */
        FuncNonCopyable &operator=(FuncNonCopyable &&other)
        {
            if (&other == this)
            {
                return *this;
            }

            vtable->destroy(args);

            other.vtable->move(args, std::move(other.args));
            this->call = other.call;
            this->vtable = other.vtable;

            // Leave other in some well defined state.
            other.set_bad_call();

            return *this;
        }

        /**
         * Forwarding function call operator.
         *
         * @param arguments The arguments to pass through to the delegate.
         *
         * @return Returns the Result type.
         */
        Result operator()(Arguments... arguments) const
        {
            return call(args, std::forward<Arguments>(arguments)...);
        }

        /** Destructor. */
        ~FuncNonCopyable()
        {
            vtable->destroy(args);
        }

        /** These must be deleted to allow for non-copyable captures (like unique_ptr). */
        template<typename T>
        FuncNonCopyable(const T &functor) = delete;
        FuncNonCopyable(const FuncNonCopyable &other) = delete;
        FuncNonCopyable &operator=(const FuncNonCopyable &other) = delete;
        template<typename T>
        FuncNonCopyable &operator=(FuncNonCopyable &other) = delete;

    protected:
        /**
         * Construct a new Func object with both the call and vtable set to the
         * passed in ones. Used by the CopyableType's copy constructors. 
         * 
         * @tparam C The call type.
         * @tparam V The vtable type.
         * @param call_type The call function to set.
         * @param vtable_type The vtable to set.
         */
        template <typename C, typename V>
        FuncNonCopyable(C call_type, V vtable_type)
            : call(call_type)
            , vtable(vtable_type)
        {
        }

        /** The delegate arguments (function pointers and / or captures go here). */
        FunctorArgs args;

        /**
         * Trampoline function which reimbues the type-erased delgate with its original type and calls the functor.
         * This is statically constructed by the compiler or copied from another value and cannot be null.
         */
        func_call<Result, Arguments...> call;

        /**
         * Pointer to the manual virtual table.  This is statically constructed by the compiler or copied from another
         * value and cannot be null.
         */
        const Vtable *vtable;
    };

    /**
     * Copyable delegate - usable for the more common case of delegates with copyable captures.
     *
     * @tparam Result The delegate return type.
     * @tparam Arguments The delegate function arguments.
     */
    template<typename Result, typename... Arguments>
    class FuncCopyable : protected FuncNonCopyable<Result, Arguments...>
    {
    public:
        /** Type used to bring forward the useful functions from the base class. */
        using FNC = FuncNonCopyable<Result, Arguments...>;
        using FNC::operator();
        using FNC::operator bool;

        /** Default constructor. Leave the object in an uninitialized state (see operator bool). */
        FuncCopyable() : FNC()
        {
        }
    
        /**
         * Converting from functor move constructor. This could have been pass by value,
         * but the parent's move constructor exists so this saves some code. The cost
         * is roughly the same.
         *
         * @tparam T The functor type.
         * @param functor The functor to move.
         */
        template<typename T>
        FuncCopyable(const T& functor) : FNC(&typed_call<T, Result, Arguments...>, &Vtable::get_vtable<T>())
        {
            static_assert(can_copy<T>(), "Object is non-copyable");
            store_functor(this->args, functor);
        }

        /**
         * Copy constructor.
         *
         * @param other The delegate to move from.
         */
        FuncCopyable(const FuncCopyable &other) : FNC(other.call, other.vtable)
        {
            this->vtable->copy(this->args, other.args);
        }

        /**
         * Copy functor assignment operator.
         * 
         * @param other The delegate to copy from.
         * 
         * @return Returns a reference to this.
         */
        template<typename T>
        FuncCopyable &operator=(const T& functor)
        {
            static_assert(can_emplace<T>(), "Delegate doesn't fit.");
            static_assert(std::is_same_v<Result, std::invoke_result_t<T, Arguments...>>, "Wrong return type.");
            static_assert(can_copy<T>(), "Object is non-copyable");

            store_functor(this->args, functor);
            FNC::template set_call_by_type<T>(functor);
            FNC::template set_vtable_by_type<T>(functor);

            return *this;
        }

        /**
         * Copy assignment operator.
         *
         * @param other The delegate to copy from.
         *
         * @return Returns a reference to this.
         */
        FuncCopyable &operator=(const FuncCopyable &other)
        {
            if (this == &other)
            {
                return *this;
            }

            this->vtable->destroy(this->args);
            this->vtable->copy(this->args, other.args);
            this->call = other.call;
            this->vtable = other.vtable;

            return *this;
        }
    };

    /** The following two are convenient names for the delegates. */
    template<typename Result, typename... Arguments>
    using MoveDelegate = FuncNonCopyable<Result, Arguments...>;

    template<typename Result, typename... Arguments>
    using Delegate = FuncCopyable<Result, Arguments...>;
}

