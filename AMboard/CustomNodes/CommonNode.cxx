//
// Created by LYS on 2/26/2026.
//

#include <AMboard/Macro/DataPin.hxx>
#include <AMboard/Macro/ExecuteNode.hxx>
#include <AMboard/Macro/Ext/ImGuiPopup.hxx>

#include <AMboard/Control/InputService.hxx>

#include <iostream>
#include <variant>

#include <spdlog/spdlog.h>

#include <imgui.h>

#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#endif

// 1. Define the Math Operations
enum class EMathOperation {
    Add,
    Subtract,
    Multiply,
    Divide
};

// A. Add the type to the Variant
using NumericVariant = std::variant<
    int8_t, uint8_t,
    int16_t, uint16_t,
    int32_t, uint32_t,
    int64_t, uint64_t,
    float, double>;

// B. Define the string mapping
template <typename T>
constexpr std::string_view TypeName = "unknown";
template <>
constexpr std::string_view TypeName<int8_t> = "int8_t";
template <>
constexpr std::string_view TypeName<uint8_t> = "uint8_t";
template <>
constexpr std::string_view TypeName<int16_t> = "int16_t";
template <>
constexpr std::string_view TypeName<uint16_t> = "uint16_t";
template <>
constexpr std::string_view TypeName<int32_t> = "int32_t";
template <>
constexpr std::string_view TypeName<uint32_t> = "uint32_t";
template <>
constexpr std::string_view TypeName<int64_t> = "int64_t";
template <>
constexpr std::string_view TypeName<uint64_t> = "uint64_t";
template <>
constexpr std::string_view TypeName<float> = "float";
template <>
constexpr std::string_view TypeName<double> = "double";

// C. Define the Promotion Rank (Higher rank becomes the result type)
template <typename T>
constexpr int TypeRank = 0;
template <>
constexpr int TypeRank<int8_t> = 1;
template <>
constexpr int TypeRank<uint8_t> = 2;
template <>
constexpr int TypeRank<int16_t> = 3;
template <>
constexpr int TypeRank<uint16_t> = 4;
template <>
constexpr int TypeRank<int32_t> = 5;
template <>
constexpr int TypeRank<uint32_t> = 6;
template <>
constexpr int TypeRank<int64_t> = 7;
template <>
constexpr int TypeRank<uint64_t> = 8;
template <>
constexpr int TypeRank<float> = 9;
template <>
constexpr int TypeRank<double> = 10;

// =========================================================================
// CORE LOGIC
// =========================================================================

// Automatic Type Promotion based on Rank
template <typename T1, typename T2>
using PromotedType = std::conditional_t<(TypeRank<T1> > TypeRank<T2>), T1, T2>;

// Parse void* to Variant using C++17 Fold Expressions
template <typename Variant>
Variant ParseData(const std::string_view& ty, const double data)
{
    Variant result;
    bool found = false;

    // C++20 Templated Lambda to iterate over all types in the variant
    [&]<typename... Ts>(std::variant<Ts...>&) {
        found = ((ty == TypeName<Ts> ? (result = reinterpret_cast<const Ts&>(data), true) : false) || ...);
    }(result);

    if (!found)
        throw std::invalid_argument(std::format("Unsupported type: {}", ty));
    return result;
}

class CEntranceNode : public CExecuteNode {
public:
    CEntranceNode()
    {
        ErasePin(GetInputPins()[0].get());
    }
};

class CToStringNode : public CBaseNode {
public:
    CToStringNode()
    {
        EmplacePin<CDataPin>(true)->SetIsUniversalPin();
        EmplacePin<CDataPin>(false)->SetValueType("string");
    }

    std::string_view GetCategory() noexcept override
    {
        return "Util";
    }

protected:
    // Convert void* to string using Fold Expressions and std::format
    template <typename Variant>
    std::string ToStringImpl(const std::string_view& ty, const double data)
    {
        std::string result;
        bool found = false;
        Variant dummy;

        [&]<typename... Ts>(std::variant<Ts...>&) {
            // Unary '+' ensures int8_t/uint8_t format as numbers, not ASCII chars
            found = ((ty == TypeName<Ts> ? (result = std::format("{}", +reinterpret_cast<const Ts&>(data)), true) : false) || ...);
        }(dummy);

        if (!found)
            throw std::invalid_argument(std::format("Unsupported type: {}", ty));
        return result;
    }

    // Public ToString wrapper
    std::string ToString(std::string_view Ty, const double Data)
    {
        return ToStringImpl<NumericVariant>(Ty, Data);
    }

    bool Evaluate() noexcept override
    {
        if (!CBaseNode::Evaluate())
            return false;

        const auto& Value = reinterpret_cast<const CDataPin&>(*GetInputPins()[0]);
        reinterpret_cast<CDataPin&>(*GetOutputPins()[0]).Set("string", std::make_shared<std::string>(ToString(Value.GetValueType(), Value.AsDouble())));

        return true;
    }
};

class CPrintingNode : public CExecuteNode {
public:
    CPrintingNode()
    {
        EmplacePin<CDataPin>(true)->SetValueType("string");
    }

    std::string_view GetCategory() noexcept override
    {
        return "Logging";
    }

protected:
    void Execute() override
    {
        CExecuteNode::Execute();

        const auto& Value = reinterpret_cast<const CDataPin&>(*GetInputPins()[1]);
        spdlog::info("{}", Value.Get<std::string>("string"));
    }
};

class CBranchingNode : public CExecuteNode {
public:
    CBranchingNode()
    {
        EmplacePin<CDataPin>(true)->SetValueType("bool").SetToolTips("bCondition");
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
                !DataPin.empty() && !static_cast<CDataPin*>(DataPin.front().get())->PinGetTrivial(bool))

                m_DesiredOutputPin = 1;
        }
    }
};

class CSequenceNode : public CExecuteNode, public INodeImGuiPupUpExt {
public:
    std::string GetTitle() override
    {
        return "Pin Edit";
    }

    bool Render() override
    {
        uint8_t PinCount = GetFlowOutputPins().size();
        uint8_t Step = 1;
        ImGui::InputScalar("Pin Count", ImGuiDataType_U8, &PinCount, &Step, nullptr, "%d");

        if (PinCount >= 1) {
            while (PinCount != GetFlowOutputPins().size()) {
                if (PinCount > GetFlowOutputPins().size())
                    EmplacePin<CFlowPin>(false);
                else
                    ErasePin(GetFlowOutputPins().back());
            }
        }

        return true;
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

    void WriteExtraContext(std::string& ExtContext) const override
    {
        assert(GetFlowOutputPins().size() < std::numeric_limits<uint8_t>::max());
        if (GetFlowOutputPins().size() > 1)
            ExtContext.push_back(static_cast<uint8_t>(GetFlowOutputPins().size()));
    }

    void ReadExtraContext(const std::string& ExtContext) override
    {
        if (ExtContext.size() == 1) {
            for (int i = static_cast<uint8_t>(ExtContext[0]); i > 1; --i) {
                EmplacePin<CFlowPin>(false);
            }
        }
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
        EmplacePin<CDataPin>(true)->SetIsUniversalPin();
        EmplacePin<CDataPin>(false);
    }

protected:
    std::string_view Operate(EMathOperation op, const std::string_view& Ty1, const std::string_view& Ty2, const double Data1, const double Data2, void* DataOutput)
    {
        auto val1 = ParseData<NumericVariant>(Ty1, Data1);
        auto val2 = ParseData<NumericVariant>(Ty2, Data2);

        return std::visit([op, DataOutput](auto v1, auto v2) -> std::string_view {
            using T1 = decltype(v1);
            using T2 = decltype(v2);
            using ResultType = PromotedType<T1, T2>;

            ResultType c1 = static_cast<ResultType>(v1);
            ResultType c2 = static_cast<ResultType>(v2);
            ResultType result { };

            switch (op) {
            case EMathOperation::Add:
                result = c1 + c2;
                break;
            case EMathOperation::Subtract:
                result = c1 - c2;
                break;
            case EMathOperation::Multiply:
                result = c1 * c2;
                break;
            case EMathOperation::Divide:
                if constexpr (std::is_integral_v<ResultType>) {
                    if (c2 == 0)
                        throw std::runtime_error("Integer division by zero!");
                }
                result = c1 / c2;
                break;
            default:
                std::unreachable(); // C++23
            }

            if (DataOutput != nullptr) {
                *static_cast<ResultType*>(DataOutput) = result;
            }

            return TypeName<ResultType>;
        },
            val1, val2);
    }
};

class CAddNode : public CMathCommonNode {
public:
    CAddNode()
    {
        EmplacePin<CDataPin>(true)->SetIsUniversalPin();

        reinterpret_cast<CDataPin&>(*GetInputPins()[0]).PinSet(float, 321.f);
        reinterpret_cast<CDataPin&>(*GetInputPins()[1]).PinSet(uint32_t, 123);
    }

    bool Evaluate() noexcept override
    {
        CMathCommonNode::Evaluate();

        const auto& ValueA = reinterpret_cast<const CDataPin&>(*GetInputPins()[0]);
        const auto& ValueB = reinterpret_cast<const CDataPin&>(*GetInputPins()[1]);
        auto& Result = reinterpret_cast<CDataPin&>(*GetOutputPins()[0]);

        double TrivialResult = 0;
        Result.Set(Operate(EMathOperation::Add, ValueA.GetValueType(), ValueB.GetValueType(), ValueA.AsDouble(), ValueB.AsDouble(), &TrivialResult), &TrivialResult);
        return Result.GetValueType() != "void";
    }
};

class CActionReplayNode : public CExecuteNode, public INodeImGuiPupUpExt {
public:
    std::string GetTitle() override
    {
        return "Event Record";
    }

    void InputCallback(const InputEvent& input_event)
    {
        if (!m_IsRecording) {
            return;
        }

        if (input_event.type == InputType::KeyDown) {
            if (input_event.keyCode == VK_ESCAPE) {
#ifdef _WIN32
                m_IsRecording = false;
                return;
#else
#error Well Well Well
#endif
            }
        }

        if (input_event.type != InputType::MouseMove) {
            m_Logger.append((input_event.ToString() + "\n").c_str());
        }
    }

    void OnStartPopup() override
    {
        m_InputMonitor = InputService::Get().Subscribe(std::bind_front(&CActionReplayNode::InputCallback, this));
    }

    void OnEndPopup() override
    {
        InputService::Get().Unsubscribe(m_InputMonitor);
    }

    bool Render() override
    {
        ImGui::BeginDisabled(m_IsRecording);
        if (ImGui::Button("Start record")) {
            m_Logger.clear();
            m_IsRecording = true;
        }
        if (m_IsRecording) {
            ImGui::SameLine();
            ImGui::TextDisabled("Press ESC key to stop.");
        }
        ImGui::EndDisabled();

        ImGui::Separator();
        if (!m_IsRecording) {
            ImGui::TextDisabled("First event will have a timestamp of zero, record will end with ESC key press.");
            return true;
        }

        ImGui::BeginChild("Log", { 0, 400 });
        ImGui::TextUnformatted(m_Logger.begin(), m_Logger.end());
        if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY())
            ImGui::SetScrollHereY(1.0f);
        ImGui::EndChild();

        return true;
    }

    std::string_view GetCategory() noexcept override
    {
        return "Event";
    }

    void ExecuteNode() override
    {
    }

    void WriteExtraContext(std::string& ExtContext) const override
    {
    }

    void ReadExtraContext(const std::string& ExtContext) override
    {
    }

private:
    InputService::SubscriptionID m_InputMonitor { };

    ImGuiTextBuffer m_Logger;
    bool m_IsRecording = false;
};

REGISTER_MACROS(CActionReplayNode, CAddNode, CEntranceNode, CToStringNode, CPrintingNode, CBranchingNode, CSequenceNode)
ENABLE_IMGUI()