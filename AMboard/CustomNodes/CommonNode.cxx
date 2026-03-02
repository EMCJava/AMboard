//
// Created by LYS on 2/26/2026.
//

#include <AMboard/Macro/DataPin.hxx>
#include <AMboard/Macro/ExecuteNode.hxx>

#include <iostream>

#include <spdlog/spdlog.h>

// 1. Helper structs for type lists
template <typename T>
struct TypeTag {
    using type = T;
};
template <typename... Ts>
struct TypeList { };

// 2. Supported basic C++ numeric types
using SupportedNumericTypes = TypeList<
    char, signed char, unsigned char,
    short, unsigned short,
    int, unsigned int,
    long, unsigned long,
    long long, unsigned long long,
    float, double, long double>;

class CEntranceNode : public CExecuteNode {
public:
    CEntranceNode()
    {
        EmplacePin<CFlowPin>(false);
    }
};

class CPrintingNode : public CExecuteNode {
public:
    CPrintingNode()
    {
        AddInputOutputFlowPin();
        EmplacePin<CDataPin>(true);
    }

    std::string_view GetCategory() noexcept override
    {
        return "Logging";
    }

    template <typename... Ts>
    std::string ToString_Impl(const std::type_index Ty, const void* ptr, TypeList<Ts...>)
    {
        std::string result;

        auto handleType = [&](auto tag) {
            using T = typename decltype(tag)::type;

            // Cast the void pointer back to the actual type
            const T value = *static_cast<const T*>(ptr);

            // std::to_string automatically promotes char/short to int,
            // so it safely converts all numeric types to their string representation.
            result = std::to_string(value);
        };

        // Fold expression: iterate through all types to find the matching type_index
        ((Ty == typeid(Ts) ? (handleType(TypeTag<Ts> { }), true) : false) || ...);

        return result;
    }

protected:
    void Execute() override
    {
        Evaluate();

        const auto& Value = reinterpret_cast<const CDataPin&>(*GetInputPins()[1]);
        spdlog::info("{}", ToString_Impl(Value, Value.GetData(), SupportedNumericTypes { }));
    }
};

class CBranchingNode : public CExecuteNode {
public:
    CBranchingNode()
    {
        AddInputOutputFlowPin();
        EmplacePin<CDataPin>(true);
        EmplacePin<CFlowPin>(false);
    }

    std::string_view GetCategory() noexcept override
    {
        return "Flow";
    }

protected:
    void Execute() override
    {
        if (GetFlowOutputPins().size() > 1) [[likely]] {
            if (auto DataPin = GetInputPinsWith<EPinType::Data>();
                !DataPin.empty() && !static_cast<CDataPin*>(DataPin.front().get())->TryGet<bool>())

                m_DesiredOutputPin = 1;
        }
    }
};

class CSequenceNode : public CExecuteNode {
public:
    CSequenceNode()
    {
        AddInputOutputFlowPin();
    }

    std::string_view GetCategory() noexcept override
    {
        return "Flow";
    }

    void ExecuteNode() override
    {
        for (const auto* Pin : GetFlowOutputPins())
            if (*Pin)
                Pin->GetTheOnlyConnected()->GetOwner()->As<CExecuteNode>()->ExecuteNode();
    }
};

class CMathCommonNode : public CBaseNode {
public:
    std::string_view GetCategory() noexcept override
    {
        return "Numeric";
    }

    CMathCommonNode()
    {
        EmplacePin<CDataPin>(true);
        EmplacePin<CDataPin>(false);
    }

protected:
    // 3. The core generic engine that handles ANY operation
    template <typename Op, typename... Ts>
    std::type_index Compute_Impl(const std::type_index ATy, const std::type_index BTy,
        const void* A, const void* B, void* Result, TypeList<Ts...>)
    {
        std::type_index resultType = typeid(void);
        bool success = false;
        Op op; // Instantiate the operation functor (e.g., std::plus<>)

        auto handleA = [&](auto tagA) {
            using TypeA = typename decltype(tagA)::type;

            auto handleB = [&](auto tagB) {
                using TypeB = typename decltype(tagB)::type;

                // Deduce the result type based on the operation and C++ promotion rules
                using ResType = decltype(op(std::declval<TypeA>(), std::declval<TypeB>()));
                static_assert(!std::is_same_v<ResType, void>);

                const TypeA valA = *static_cast<const TypeA*>(A);
                const TypeB valB = *static_cast<const TypeB*>(B);

                // Safety check: Prevent Integer Division by Zero
                if constexpr (std::is_same_v<Op, std::divides<>>) {
                    if constexpr (std::is_integral_v<TypeB>) {
                        if (valB == 0) {
                            throw std::runtime_error("Integer division by zero.");
                        }
                    }
                }

                // Store result if a valid pointer was provided
                if (Result) {
                    *static_cast<ResType*>(Result) = op(valA, valB);
                }

                resultType = typeid(ResType);
                success = true;
            };

            ((BTy == typeid(Ts) ? (handleB(TypeTag<Ts> { }), true) : false) || ...);
        };

        ((ATy == typeid(Ts) ? (handleA(TypeTag<Ts> { }), true) : false) || ...);

        if (!success) {
            return typeid(void);
        }

        return resultType;
    }
};

class CAddNode : public CMathCommonNode {
public:
    CAddNode()
    {
        EmplacePin<CDataPin>(true);

        reinterpret_cast<CDataPin&>(*GetInputPins()[0]).Set(123);
        reinterpret_cast<CDataPin&>(*GetInputPins()[1]).Set(321.f);
    }

    bool Evaluate() noexcept override
    {
        CMathCommonNode::Evaluate();

        const auto& ValueA = reinterpret_cast<const CDataPin&>(*GetInputPins()[0]);
        const auto& ValueB = reinterpret_cast<const CDataPin&>(*GetInputPins()[1]);
        auto& Result = reinterpret_cast<CDataPin&>(*GetOutputPins()[0]);

        return Result.SetValueType(Compute_Impl<std::plus<>>(ValueA, ValueB, ValueA.GetData(), ValueB.GetData(), Result.GetData(), SupportedNumericTypes { })) != typeid(void);
    }
};

REGISTER_MACROS(CAddNode, CEntranceNode, CPrintingNode, CBranchingNode, CSequenceNode)