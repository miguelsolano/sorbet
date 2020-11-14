#ifndef SORBET_TYPES_H
#define SORBET_TYPES_H

#include "common/Counters.h"
#include "core/Context.h"
#include "core/Error.h"
#include "core/SymbolRef.h"
#include "core/TypeConstraint.h"
#include <memory>
#include <optional>
#include <string>
namespace sorbet::core {
/** Dmitry: unlike in Dotty, those types are always dealiased. For now */
class Type;
class AppliedType;
class IntrinsicMethod;
class TypeConstraint;
struct DispatchArgs;
struct DispatchComponent;
struct DispatchResult;
struct CallLocs;
class Symbol;
class TypeVar;
class SendAndBlockLink;
class TypeAndOrigins;

class ParamInfo {
public:
    struct ArgFlags {
        bool isKeyword = false;
        bool isRepeated = false;
        bool isDefault = false;
        bool isShadow = false;
        bool isBlock = false;

    private:
        friend class Symbol;
        friend class serialize::SerializerImpl;
        void setFromU1(u1 flags);
        u1 toU1() const;
    };
    ArgFlags flags;
    NameRef name;
    SymbolRef rebind;
    Loc loc;
    TypePtr type;

    /*
     * Return type as observed by the method
     * body that defined that argument
     */
    TypePtr argumentTypeAsSeenByImplementation(Context ctx, core::TypeConstraint &constr) const;

    bool isSyntheticBlockArgument() const;
    std::string toString(const GlobalState &gs) const;
    std::string show(const GlobalState &gs) const;
    std::string argumentName(const GlobalState &gs) const;
    ParamInfo(const ParamInfo &) = delete;
    ParamInfo() = default;
    ParamInfo(ParamInfo &&) noexcept = default;
    ParamInfo &operator=(ParamInfo &&) noexcept = default;
    ParamInfo deepCopy() const;
};
CheckSize(ParamInfo, 48, 8);

template <class T, class... Args> TypePtr make_type(Args &&... args) {
    return TypePtr(TypePtr::TypeToTag<T>::value, new T(std::forward<Args>(args)...));
}

enum class UntypedMode {
    AlwaysCompatible = 1,
    AlwaysIncompatible = 2,
};

class Types final {
public:
    /** Greater lower bound: the widest type that is subtype of both t1 and t2 */
    static TypePtr all(const GlobalState &gs, const TypePtr &t1, const TypePtr &t2);

    /** Lower upper bound: the narrowest type that is supper type of both t1 and t2 */
    static TypePtr any(const GlobalState &gs, const TypePtr &t1, const TypePtr &t2);

    /**
     * is every instance of  t1 an  instance of t2?
     *
     * The parameter `mode` controls whether or not `T.untyped` is
     * considered to be a super type or subtype of all other types */
    static bool isSubTypeUnderConstraint(const GlobalState &gs, TypeConstraint &constr, const TypePtr &t1,
                                         const TypePtr &t2, UntypedMode mode);

    /** is every instance of  t1 an  instance of t2 when not allowed to modify constraint */
    static bool isSubType(const GlobalState &gs, const TypePtr &t1, const TypePtr &t2);
    static bool equiv(const GlobalState &gs, const TypePtr &t1, const TypePtr &t2);

    /** check that t1 <: t2, but do not consider `T.untyped` as super type or a subtype of all other types */
    static bool isAsSpecificAs(const GlobalState &gs, const TypePtr &t1, const TypePtr &t2);
    static bool equivNoUntyped(const GlobalState &gs, const TypePtr &t1, const TypePtr &t2);

    static TypePtr top();
    static TypePtr bottom();
    static TypePtr nilClass();
    static TypePtr untyped(const core::GlobalState &gs, core::SymbolRef blame);
    static TypePtr untypedUntracked();
    static TypePtr void_();
    static TypePtr trueClass();
    static TypePtr falseClass();
    static TypePtr Integer();
    static TypePtr String();
    static TypePtr Symbol();
    static TypePtr Float();
    static TypePtr Boolean();
    static TypePtr Object();
    static TypePtr arrayOfUntyped();
    static TypePtr rangeOfUntyped();
    static TypePtr hashOfUntyped();
    static TypePtr procClass();
    static TypePtr classClass();
    static TypePtr declBuilderForProcsSingletonClass();
    static TypePtr falsyTypes();

    static TypePtr dropSubtypesOf(const GlobalState &gs, const TypePtr &from, SymbolRef klass);
    static TypePtr approximateSubtract(const GlobalState &gs, const TypePtr &from, const TypePtr &what);
    static bool canBeTruthy(const GlobalState &gs, const TypePtr &what);
    static bool canBeFalsy(const GlobalState &gs, const TypePtr &what);
    enum class Combinator { OR, AND };

    static TypePtr resultTypeAsSeenFrom(const GlobalState &gs, const TypePtr &what, SymbolRef fromWhat,
                                        SymbolRef inWhat, const std::vector<TypePtr> &targs);

    static InlinedVector<SymbolRef, 4> alignBaseTypeArgs(const GlobalState &gs, SymbolRef what,
                                                         const std::vector<TypePtr> &targs, SymbolRef asIf);
    // Extract the return value type from a proc.
    static TypePtr getProcReturnType(const GlobalState &gs, const TypePtr &procType);
    static TypePtr instantiate(const GlobalState &gs, const TypePtr &what, const InlinedVector<SymbolRef, 4> &params,
                               const std::vector<TypePtr> &targs);
    /** Replace all type variables in `what` with their instantiations.
     * Requires that `tc` has already been solved.
     */
    static TypePtr instantiate(const GlobalState &gs, const TypePtr &what, const TypeConstraint &tc);

    static TypePtr replaceSelfType(const GlobalState &gs, const TypePtr &what, const TypePtr &receiver);
    /** Get rid of type variables in `what` and return a type that we deem close enough to continue
     * typechecking. We should be careful to only used this type when we are trying to guess a type.
     * We should do proper instatiation and subtype test after we have guessed type variables with
     * tc.solve(). If the constraint has already been solved, use `instantiate` instead.
     */
    static TypePtr approximate(const GlobalState &gs, const TypePtr &what, const TypeConstraint &tc);
    static TypePtr dispatchCallWithoutBlock(const GlobalState &gs, const TypePtr &recv, const DispatchArgs &args);
    static TypePtr dropLiteral(const TypePtr &tp);

    /** Internal implementation. You should probably use all(). */
    static TypePtr glb(const GlobalState &gs, const TypePtr &t1, const TypePtr &t2);
    /** Internal implementation. You should probably use any(). */
    static TypePtr lub(const GlobalState &gs, const TypePtr &t1, const TypePtr &t2);

    static TypePtr lubAll(const GlobalState &gs, std::vector<TypePtr> &elements);
    static TypePtr arrayOf(const GlobalState &gs, const TypePtr &elem);
    static TypePtr rangeOf(const GlobalState &gs, const TypePtr &elem);
    static TypePtr hashOf(const GlobalState &gs, const TypePtr &elem);
    static TypePtr dropNil(const GlobalState &gs, const TypePtr &from);

    /** Recursively replaces proxies with their underlying types */
    static TypePtr widen(const GlobalState &gs, const TypePtr &type);
    static std::optional<int> getProcArity(const AppliedType &type);

    /** Unwrap SelfTypeParam instances that belong to the given owner, to a bare LambdaParam */
    static TypePtr unwrapSelfTypeParam(Context ctx, const TypePtr &ty);

    // Given a type, return a SymbolRef for the Ruby class that has that type, or no symbol if no such class exists.
    // This is an internal method for implementing intrinsics. In the future we should make all updateKnowledge methods
    // be intrinsics so that this can become an anonymous helper function in calls.cc.
    static core::SymbolRef getRepresentedClass(const GlobalState &gs, const core::TypePtr &ty);
};

struct Intrinsic {
    enum class Kind : u1 {
        Instance = 1,
        Singleton = 2,
    };
    const SymbolRef symbol;
    const Kind singleton;
    const NameRef method;
    const IntrinsicMethod *impl;
};
extern const std::vector<Intrinsic> intrinsicMethods;

template <class To> bool isa_type(const TypePtr &what) {
    return what != nullptr && what.tag() == TypePtr::TypeToTag<To>::value;
}

// Specializations to handle the class hierarchy.
class ClassType;
template <> inline bool isa_type<ClassType>(const TypePtr &what) {
    if (what == nullptr) {
        return false;
    }
    switch (what.tag()) {
        case TypePtr::Tag::ClassType:
        case TypePtr::Tag::BlamedUntyped:
        case TypePtr::Tag::UnresolvedClassType:
        case TypePtr::Tag::UnresolvedAppliedType:
            return true;
        default:
            return false;
    }
}

inline bool is_ground_type(const TypePtr &what) {
    if (what == nullptr) {
        return false;
    }
    switch (what.tag()) {
        case TypePtr::Tag::ClassType:
        case TypePtr::Tag::BlamedUntyped:
        case TypePtr::Tag::UnresolvedClassType:
        case TypePtr::Tag::UnresolvedAppliedType:
        case TypePtr::Tag::OrType:
        case TypePtr::Tag::AndType:
            return true;
        case TypePtr::Tag::LiteralType:
        case TypePtr::Tag::ShapeType:
        case TypePtr::Tag::TupleType:
        case TypePtr::Tag::MetaType:
        case TypePtr::Tag::LambdaParam:
        case TypePtr::Tag::SelfTypeParam:
        case TypePtr::Tag::SelfType:
        case TypePtr::Tag::AliasType:
        case TypePtr::Tag::AppliedType:
        case TypePtr::Tag::TypeVar:
            return false;
    }
}

inline bool is_proxy_type(const TypePtr &what) {
    if (what == nullptr) {
        return false;
    }
    switch (what.tag()) {
        case TypePtr::Tag::LiteralType:
        case TypePtr::Tag::ShapeType:
        case TypePtr::Tag::TupleType:
        case TypePtr::Tag::MetaType:
            return true;
        case TypePtr::Tag::ClassType:
        case TypePtr::Tag::BlamedUntyped:
        case TypePtr::Tag::UnresolvedClassType:
        case TypePtr::Tag::UnresolvedAppliedType:
        case TypePtr::Tag::OrType:
        case TypePtr::Tag::AndType:
        case TypePtr::Tag::LambdaParam:
        case TypePtr::Tag::SelfTypeParam:
        case TypePtr::Tag::SelfType:
        case TypePtr::Tag::AliasType:
        case TypePtr::Tag::AppliedType:
        case TypePtr::Tag::TypeVar:
            return false;
    }
}

template <class To> To const *cast_type(const TypePtr &what) {
    if (isa_type<To>(what)) {
        return reinterpret_cast<To const *>(what.get());
    } else {
        return nullptr;
    }
}

template <class To> To *cast_type(TypePtr &what) {
    // const To* -> To* to avoid reimplementing the same logic twice.
    return const_cast<To *>(cast_type<To>(static_cast<const TypePtr &>(what)));
}

template <class To> To const &cast_type_nonnull(const TypePtr &what) {
    ENFORCE_NO_TIMER(isa_type<To>(what));
    return *reinterpret_cast<const To *>(what.get());
}

// Simple forwarders defined on TypePtr which make `typecase` work.
template <class To> inline bool TypePtr::isa(const TypePtr &what) {
    return isa_type<To>(what);
}

template <class To> inline To const &TypePtr::cast(const TypePtr &what) {
    return cast_type_nonnull<To>(what);
}

template <> inline bool TypePtr::isa<TypePtr>(const TypePtr &what) {
    return true;
}

template <> inline TypePtr const &TypePtr::cast<TypePtr>(const TypePtr &what) {
    return what;
}

#define TYPE(name)                                                                                             \
    class name;                                                                                                \
    template <> struct TypePtr::TypeToTag<name> { static constexpr TypePtr::Tag value = TypePtr::Tag::name; }; \
    class __attribute__((aligned(8))) name

TYPE(ClassType) {
public:
    SymbolRef symbol;
    ClassType(SymbolRef symbol);

    std::string toStringWithTabs(const GlobalState &gs, int tabs = 0) const;
    std::string show(const GlobalState &gs) const;
    DispatchResult dispatchCall(const GlobalState &gs, const DispatchArgs &args) const;

    TypePtr getCallArguments(const GlobalState &gs, NameRef name) const;
    bool derivesFrom(const GlobalState &gs, SymbolRef klass) const;
    void _sanityCheck(const GlobalState &gs) const;
};
CheckSize(ClassType, 8, 8);

/*
 * This is the type used to represent a use of a type_member or type_template in
 * a signature.
 */
TYPE(LambdaParam) final {
public:
    SymbolRef definition;

    // The type bounds provided in the definition of the type_member or
    // type_template.
    TypePtr lowerBound;
    TypePtr upperBound;

    LambdaParam(SymbolRef definition, TypePtr lower, TypePtr upper);
    std::string toStringWithTabs(const GlobalState &gs, int tabs = 0) const;
    std::string show(const GlobalState &gs) const;

    bool derivesFrom(const GlobalState &gs, SymbolRef klass) const;

    void _sanityCheck(const GlobalState &gs) const;

    TypePtr _instantiate(const GlobalState &gs, const InlinedVector<SymbolRef, 4> &params,
                         const std::vector<TypePtr> &targs) const;
};
CheckSize(LambdaParam, 40, 8);

TYPE(SelfTypeParam) final {
public:
    SymbolRef definition;

    SelfTypeParam(const SymbolRef definition);
    std::string toStringWithTabs(const GlobalState &gs, int tabs = 0) const;
    std::string show(const GlobalState &gs) const;

    bool derivesFrom(const GlobalState &gs, SymbolRef klass) const;

    DispatchResult dispatchCall(const GlobalState &gs, const DispatchArgs &args) const;
    void _sanityCheck(const GlobalState &gs) const;
};
CheckSize(SelfTypeParam, 8, 8);

TYPE(AliasType) final {
public:
    AliasType(SymbolRef other);
    std::string toStringWithTabs(const GlobalState &gs, int tabs = 0) const;
    std::string show(const GlobalState &gs) const;
    bool derivesFrom(const GlobalState &gs, SymbolRef klass) const;

    SymbolRef symbol;
    void _sanityCheck(const GlobalState &gs) const;
};
CheckSize(AliasType, 8, 8);

/** This is a specific kind of self-type that should only be used in method return position.
 * It indicates that the method may(or will) return `self` or type that behaves equivalently
 * to self(e.g. in case of `.clone`).
 */
TYPE(SelfType) final {
public:
    SelfType();
    std::string toStringWithTabs(const GlobalState &gs, int tabs = 0) const;
    std::string show(const GlobalState &gs) const;
    virtual std::string showValue(const GlobalState &gs) const final;

    TypePtr _replaceSelfType(const GlobalState &gs, const TypePtr &receiver) const;

    void _sanityCheck(const GlobalState &gs) const;
    bool derivesFrom(const GlobalState &gs, SymbolRef klass) const;
};
CheckSize(SelfType, 8, 8);

TYPE(LiteralType) final {
public:
    union {
        const int64_t value;
        const double floatval;
    };

    enum class LiteralTypeKind : u1 { Integer, String, Symbol, Float };
    const LiteralTypeKind literalKind;
    LiteralType(int64_t val);
    LiteralType(double val);
    LiteralType(SymbolRef klass, NameRef val);
    TypePtr underlying() const;
    bool derivesFrom(const GlobalState &gs, core::SymbolRef klass) const;
    DispatchResult dispatchCall(const GlobalState &gs, DispatchArgs args) const;

    std::string toStringWithTabs(const GlobalState &gs, int tabs = 0) const;
    std::string show(const GlobalState &gs) const;
    virtual std::string showValue(const GlobalState &gs) const final;

    bool equals(const LiteralType &rhs) const;
    void _sanityCheck(const GlobalState &gs) const;
};
CheckSize(LiteralType, 24, 8);

/*
 * TypeVars are the used for the type parameters of generic methods.
 */
TYPE(TypeVar) final {
public:
    SymbolRef sym;
    TypeVar(SymbolRef sym);
    std::string toStringWithTabs(const GlobalState &gs, int tabs = 0) const;
    std::string show(const GlobalState &gs) const;
    void _sanityCheck(const GlobalState &gs) const;

    bool derivesFrom(const GlobalState &gs, SymbolRef klass) const;

    TypePtr _approximate(const GlobalState &gs, const TypeConstraint &tc) const;
    TypePtr _instantiate(const GlobalState &gs, const TypeConstraint &tc) const;
};
CheckSize(TypeVar, 8, 8);

TYPE(OrType) final {
public:
    TypePtr left;
    TypePtr right;

    std::string toStringWithTabs(const GlobalState &gs, int tabs = 0) const;
    std::string show(const GlobalState &gs) const;
    DispatchResult dispatchCall(const GlobalState &gs, const DispatchArgs &args) const;
    TypePtr getCallArguments(const GlobalState &gs, NameRef name) const;
    bool derivesFrom(const GlobalState &gs, SymbolRef klass) const;
    void _sanityCheck(const GlobalState &gs) const;
    TypePtr _instantiate(const GlobalState &gs, const InlinedVector<SymbolRef, 4> &params,
                         const std::vector<TypePtr> &targs) const;
    TypePtr _approximate(const GlobalState &gs, const TypeConstraint &tc) const;
    TypePtr _instantiate(const GlobalState &gs, const TypeConstraint &tc) const;
    TypePtr _replaceSelfType(const GlobalState &gs, const TypePtr &receiver) const;

private:
    /*
     * You probably want Types::any() instead.
     */
    OrType(const TypePtr &left, const TypePtr &right);

    /*
     * These implementation methods are allowed to directly instantiate
     * `OrType`. Because `make_shared<OrType>` is not compatible with private
     * constructors + friend declarations (it's `shared_ptr` calling the
     * constructor, so the friend declaration doesn't work), we provide the
     * `make_shared` helper here.
     */
    friend TypePtr Types::falsyTypes();
    friend TypePtr Types::Boolean();
    friend class GlobalSubstitution;
    friend class serialize::SerializerImpl;
    friend bool Types::isSubTypeUnderConstraint(const GlobalState &gs, TypeConstraint &constr, const TypePtr &t1,
                                                const TypePtr &t2, UntypedMode mode);
    friend TypePtr lubDistributeOr(const GlobalState &gs, const TypePtr &t1, const TypePtr &t2);
    friend TypePtr lubGround(const GlobalState &gs, const TypePtr &t1, const TypePtr &t2);
    friend TypePtr Types::lub(const GlobalState &gs, const TypePtr &t1, const TypePtr &t2);
    friend TypePtr Types::glb(const GlobalState &gs, const TypePtr &t1, const TypePtr &t2);
    friend TypePtr filterOrComponents(const TypePtr &originalType, const InlinedVector<TypePtr, 4> &typeFilter);
    friend TypePtr Types::dropSubtypesOf(const GlobalState &gs, const TypePtr &from, SymbolRef klass);
    friend TypePtr Types::unwrapSelfTypeParam(Context ctx, const TypePtr &t1);
    friend class Symbol; // the actual method is `recordSealedSubclass(Mutableconst GlobalState &gs, SymbolRef
                         // subclass)`, but refering to it introduces a cycle

    static TypePtr make_shared(const TypePtr &left, const TypePtr &right);
};
CheckSize(OrType, 32, 8);

TYPE(AndType) final {
public:
    TypePtr left;
    TypePtr right;

    std::string toStringWithTabs(const GlobalState &gs, int tabs = 0) const;
    std::string show(const GlobalState &gs) const;
    DispatchResult dispatchCall(const GlobalState &gs, const DispatchArgs &args) const;

    TypePtr getCallArguments(const GlobalState &gs, NameRef name) const;
    bool derivesFrom(const GlobalState &gs, SymbolRef klass) const;
    void _sanityCheck(const GlobalState &gs) const;
    TypePtr _instantiate(const GlobalState &gs, const InlinedVector<SymbolRef, 4> &params,
                         const std::vector<TypePtr> &targs) const;
    TypePtr _replaceSelfType(const GlobalState &gs, const TypePtr &receiver) const;
    TypePtr _approximate(const GlobalState &gs, const TypeConstraint &tc) const;
    TypePtr _instantiate(const GlobalState &gs, const TypeConstraint &tc) const;

private:
    /*
     * You probably want Types::all() instead.
     */
    AndType(const TypePtr &left, const TypePtr &right);

    friend class GlobalSubstitution;
    friend class serialize::SerializerImpl;
    friend class TypeConstraint;

    friend bool Types::isSubTypeUnderConstraint(const GlobalState &gs, TypeConstraint &constr, const TypePtr &t1,
                                                const TypePtr &t2, UntypedMode mode);
    friend TypePtr lubGround(const GlobalState &gs, const TypePtr &t1, const TypePtr &t2);
    friend TypePtr glbDistributeAnd(const GlobalState &gs, const TypePtr &t1, const TypePtr &t2);
    friend TypePtr glbGround(const GlobalState &gs, const TypePtr &t1, const TypePtr &t2);
    friend TypePtr Types::lub(const GlobalState &gs, const TypePtr &t1, const TypePtr &t2);
    friend TypePtr Types::glb(const GlobalState &gs, const TypePtr &t1, const TypePtr &t2);
    friend TypePtr Types::unwrapSelfTypeParam(Context ctx, const TypePtr &t1);

    static TypePtr make_shared(const TypePtr &left, const TypePtr &right);
};
CheckSize(AndType, 32, 8);

TYPE(ShapeType) final {
public:
    std::vector<TypePtr> keys; // TODO: store sorted by whatever
    std::vector<TypePtr> values;
    const TypePtr underlying_;
    ShapeType();
    ShapeType(TypePtr underlying, std::vector<TypePtr> keys, std::vector<TypePtr> values);

    std::string toStringWithTabs(const GlobalState &gs, int tabs = 0) const;
    std::string show(const GlobalState &gs) const;
    DispatchResult dispatchCall(const GlobalState &gs, const DispatchArgs &args) const;
    void _sanityCheck(const GlobalState &gs) const;
    TypePtr _instantiate(const GlobalState &gs, const InlinedVector<SymbolRef, 4> &params,
                         const std::vector<TypePtr> &targs) const;
    TypePtr _approximate(const GlobalState &gs, const TypeConstraint &tc) const;
    TypePtr _instantiate(const GlobalState &gs, const TypeConstraint &tc) const;
    TypePtr underlying() const;
    bool derivesFrom(const GlobalState &gs, core::SymbolRef klass) const;
};
CheckSize(ShapeType, 64, 8);

TYPE(TupleType) final {
private:
    TupleType() = delete;

public:
    std::vector<TypePtr> elems;
    const TypePtr underlying_;

    TupleType(TypePtr underlying, std::vector<TypePtr> elements);
    static TypePtr build(const GlobalState &gs, std::vector<TypePtr> elements);

    std::string toStringWithTabs(const GlobalState &gs, int tabs = 0) const;
    std::string show(const GlobalState &gs) const;
    std::string showWithMoreInfo(const GlobalState &gs) const;
    void _sanityCheck(const GlobalState &gs) const;
    TypePtr _instantiate(const GlobalState &gs, const InlinedVector<SymbolRef, 4> &params,
                         const std::vector<TypePtr> &targs) const;
    DispatchResult dispatchCall(const GlobalState &gs, const DispatchArgs &args) const;
    TypePtr _approximate(const GlobalState &gs, const TypeConstraint &tc) const;
    TypePtr _instantiate(const GlobalState &gs, const TypeConstraint &tc) const;

    // Return the type of the underlying array that this tuple decays into
    TypePtr elementType() const;
    TypePtr underlying() const;
    bool derivesFrom(const GlobalState &gs, core::SymbolRef klass) const;
};
CheckSize(TupleType, 40, 8);

TYPE(AppliedType) final {
public:
    SymbolRef klass;
    std::vector<TypePtr> targs;
    AppliedType(SymbolRef klass, std::vector<TypePtr> targs);

    std::string toStringWithTabs(const GlobalState &gs, int tabs = 0) const;
    std::string show(const GlobalState &gs) const;
    DispatchResult dispatchCall(const GlobalState &gs, const DispatchArgs &args) const;
    void _sanityCheck(const GlobalState &gs) const;
    TypePtr _instantiate(const GlobalState &gs, const InlinedVector<SymbolRef, 4> &params,
                         const std::vector<TypePtr> &targs) const;

    TypePtr getCallArguments(const GlobalState &gs, NameRef name) const;

    bool derivesFrom(const GlobalState &gs, SymbolRef klass) const;
    TypePtr _approximate(const GlobalState &gs, const TypeConstraint &tc) const;
    TypePtr _instantiate(const GlobalState &gs, const TypeConstraint &tc) const;
};
CheckSize(AppliedType, 32, 8);

// MetaType is the type of a Type. You can think of it as generalization of
// Ruby's singleton classes to Types; Just as `A.singleton_class` is the *type*
// of the *value* `A`, MetaType[T] is the *type* of a *value* that holds the
// type T during execution. For instance, the type of `T.untyped` is
// `MetaType(Types::untyped())`.
//
// These are used within the inferencer in places where we need to track
// user-written types in the source code.
TYPE(MetaType) final {
public:
    TypePtr wrapped;

    MetaType(const TypePtr &wrapped);

    std::string toStringWithTabs(const GlobalState &gs, int tabs = 0) const;
    std::string show(const GlobalState &gs) const;

    bool derivesFrom(const GlobalState &gs, SymbolRef klass) const;

    DispatchResult dispatchCall(const GlobalState &gs, const DispatchArgs &args) const;
    void _sanityCheck(const GlobalState &gs) const;

    TypePtr _approximate(const GlobalState &gs, const TypeConstraint &tc) const;
    TypePtr underlying() const;
};
CheckSize(MetaType, 16, 8);

class SendAndBlockLink {
    SendAndBlockLink(const SendAndBlockLink &) = default;

public:
    SendAndBlockLink(SendAndBlockLink &&) = default;
    std::vector<ParamInfo::ArgFlags> argFlags;
    core::NameRef fun;
    int rubyBlockId;
    std::shared_ptr<DispatchResult> result;

    SendAndBlockLink(NameRef fun, std::vector<ParamInfo::ArgFlags> &&argFlags, int rubyBlockId);
    std::optional<int> fixedArity() const;
    std::shared_ptr<SendAndBlockLink> duplicate();
};

class TypeAndOrigins final {
public:
    TypePtr type;
    InlinedVector<Loc, 1> origins;
    std::vector<ErrorLine> origins2Explanations(const GlobalState &gs, Loc originForUninitialized) const;
    ~TypeAndOrigins() noexcept;
    TypeAndOrigins() = default;
    TypeAndOrigins(const TypeAndOrigins &) = default;
    TypeAndOrigins(TypeAndOrigins &&) = default;
    TypeAndOrigins &operator=(const TypeAndOrigins &) = default;
    TypeAndOrigins &operator=(TypeAndOrigins &&) = default;
};
CheckSize(TypeAndOrigins, 40, 8);

struct CallLocs final {
    FileRef file;
    LocOffsets call;
    LocOffsets receiver;
    InlinedVector<LocOffsets, 2> &args;
};

struct DispatchArgs {
    /*
     * A note on selfType vs fullType: Because of proxies, AndType, and OrType,
     * dispatching a call can recurse into sub-components of an outer type. At
     * top level, when invoked from inference, they will match, and be the same
     * as `this`.
     *
     * As we recurse down into structured types, `fullType` continues to refer
     * to the outermost, top-level type, while `selfType` refers to the *most
     * specific* known type for `self` in this particular invocation. In
     * particular, "most specific" means that it will still carry the `AndType`s
     * and `ProxyType`s, even as we peel them off and call `dispatchCall` on
     * inner types.
     *
     * `fullType` should be exclusively used for generating error
     * messages. `selfType` is primarily used to implement `T.self_type`.
     */

    NameRef name;
    const CallLocs &locs;
    u2 numPosArgs;
    InlinedVector<const TypeAndOrigins *, 2> &args;
    const TypePtr &selfType;
    const TypePtr &fullType;
    const TypePtr &thisType;
    const std::shared_ptr<const SendAndBlockLink> &block;
    Loc originForUninitialized;
    // Do not produce dispatch-related errors while evaluating the call. This is a performance optimization, as there
    // are cases where we call dispatchCall with no intention of showing the errors to the user. Producing those
    // unreported errors is expensive!
    bool suppressErrors = false;

    DispatchArgs withSelfRef(const TypePtr &newSelfRef) const;
    DispatchArgs withThisRef(const TypePtr &newThisRef) const;
    DispatchArgs withErrorsSuppressed() const;
};

struct DispatchComponent {
    TypePtr receiver;
    SymbolRef method;
    std::vector<std::unique_ptr<Error>> errors;
    TypePtr sendTp;
    TypePtr blockReturnType;
    TypePtr blockPreType;
    ParamInfo blockSpec; // used only by LoadSelf to change type of self inside method.
    std::unique_ptr<TypeConstraint> constr;
};

struct DispatchResult {
    enum class Combinator { OR, AND };
    TypePtr returnType;
    DispatchComponent main;
    std::unique_ptr<DispatchResult> secondary;
    Combinator secondaryKind;

    DispatchResult() = default;
    DispatchResult(TypePtr returnType, TypePtr receiverType, core::SymbolRef method)
        : returnType(returnType),
          main(DispatchComponent{
              std::move(receiverType), method, {}, std::move(returnType), nullptr, nullptr, ParamInfo{}, nullptr}){};
    DispatchResult(TypePtr returnType, DispatchComponent comp)
        : returnType(std::move(returnType)), main(std::move(comp)){};
    DispatchResult(TypePtr returnType, DispatchComponent comp, std::unique_ptr<DispatchResult> secondary,
                   Combinator secondaryKind)
        : returnType(std::move(returnType)), main(std::move(comp)), secondary(std::move(secondary)),
          secondaryKind(secondaryKind){};
};

TYPE(BlamedUntyped) final : public ClassType {
public:
    const core::SymbolRef blame;
    BlamedUntyped(SymbolRef whoToBlame) : ClassType(core::Symbols::untyped()), blame(whoToBlame){};

    TypePtr getCallArguments(const GlobalState &gs, NameRef name) const;
};

TYPE(UnresolvedClassType) final : public ClassType {
public:
    const core::SymbolRef scope;
    const std::vector<core::NameRef> names;
    UnresolvedClassType(SymbolRef scope, std::vector<core::NameRef> names)
        : ClassType(core::Symbols::untyped()), scope(scope), names(names){};
    std::string toStringWithTabs(const GlobalState &gs, int tabs = 0) const;
    std::string show(const GlobalState &gs) const;
};

TYPE(UnresolvedAppliedType) final : public ClassType {
public:
    const core::SymbolRef klass;
    const std::vector<TypePtr> targs;
    UnresolvedAppliedType(SymbolRef klass, std::vector<TypePtr> targs)
        : ClassType(core::Symbols::untyped()), klass(klass), targs(std::move(targs)){};
    std::string toStringWithTabs(const GlobalState &gs, int tabs = 0) const;
    std::string show(const GlobalState &gs) const;
};

} // namespace sorbet::core
#endif // SORBET_TYPES_H
