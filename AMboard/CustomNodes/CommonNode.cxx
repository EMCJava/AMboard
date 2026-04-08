//
// Created by LYS on 2/26/2026.
//

#include <AMboard/Macro/DataPin.hxx>
#include <AMboard/Macro/ExecuteNode.hxx>
#include <AMboard/Macro/ExecutionManager.hxx>
#include <AMboard/Macro/Ext/ImGuiPopup.hxx>
#include <AMboard/Macro/Ext/NodeInnerText.hxx>

#include <AMboard/Control/InputDispatcher.hxx>
#include <AMboard/Control/InputService.hxx>

#include <iostream>
#include <variant>

#include <spdlog/spdlog.h>

#include <imgui.h>

#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#else
#include <Carbon/Carbon.h>
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

template <typename Variant>
void FromStringImpl(const std::string_view& Type, const std::string_view& Value, double& Data)
{
    bool found = false;
    Variant dummy;

    [&]<typename... Ts>(std::variant<Ts...>&) {
        found = ((Type == TypeName<Ts> ? ([&]() {
            Ts temp_val { };

            // Parse the string_view directly into the temporary typed variable
            auto [ptr, ec] = std::from_chars(Value.data(), Value.data() + Value.size(), temp_val);

            // Check for parsing errors
            if (ec != std::errc()) {
                throw std::invalid_argument(std::format("Failed to parse '{}' as {}", Value, Type));
            }

            // Cast the void* to the correct type pointer and write the value
            reinterpret_cast<Ts&>(Data) = temp_val; }(), true) : false) || ...);
    }(dummy);

    if (!found) {
        throw std::invalid_argument(std::format("Unsupported type: {}", Type));
    }
}

void FromString(const std::string_view& Type, const std::string_view& Value, double& Data)
{
    FromStringImpl<NumericVariant>(Type, Value, Data);
}

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

class CEntranceNode : public CExecuteNode {
public:
    CEntranceNode()
    {
        ErasePin(GetInputPins()[0].get());
    }
};

class COnTriggerNode : public CExecuteNode, public INodeImGuiPupUpExt {
public:
    COnTriggerNode()
    {
        ErasePin(GetInputPins()[0].get());
    }

    std::string GetTitle() override
    {
        return "On Trigger";
    }

    void Begin() noexcept override
    {
        CExecuteNode::Begin();
        m_InputMonitor = CInputService::Get().Subscribe(std::bind_front(&COnTriggerNode::InputCallback, this));
    }

    void End() noexcept override
    {
        CExecuteNode::End();
        CInputService::Get().Unsubscribe(m_InputMonitor);
    }

    void InputCallback(const SInputEvent& InputEvent)
    {
        if (InputEvent.type == EInputType::KeyDown) {
            if (m_IsRecording) {
                m_KeyCode = InputEvent.keyCode;
                m_IsRecording = false;
                return;
            }

            if (m_KeyCode == InputEvent.keyCode) {
                if (m_Manager != nullptr) {
                    if (m_Manager->StartExecuteAsync(this)) {
                        spdlog::info("[OnTrigger] Trigger accepted, async execution started");
                    } else {
                        spdlog::info("[OnTrigger] Trigger ignored: execution already in flight");
                    }
                }
            }
        }
    }

    void OnStartPopup() override
    {
        m_IsRecording = true;
    }

    void OnEndPopup() override
    {
        m_IsRecording = false;
    }

    bool Render() override
    {
        ImGui::Text("Press any key to trigger this node");
        return m_IsRecording;
    }

    std::string_view GetCategory() noexcept override
    {
        return "Flow";
    }

    void WriteExtraContext(std::string& ExtContext) const override
    {
        if (m_KeyCode.has_value()) {
            ExtContext = std::to_string(*m_KeyCode);
        }
    }
    void ReadExtraContext(const std::string& ExtContext) override
    {
        if (ExtContext.empty())
            return;

        int Value = 0;
        std::from_chars(ExtContext.data(), ExtContext.data() + ExtContext.size(), Value);
        m_KeyCode = Value;
    }

protected:
    CInputService::SubscriptionID m_InputMonitor { };

    bool m_IsRecording = false;
    std::optional<int> m_KeyCode;
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
        CExecuteNode::Execute();

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

    CExecuteNode* ExecuteNode() override
    {
        if (m_Manager == nullptr) [[unlikely]]
            return nullptr;

        for (const auto* Pin : GetFlowOutputPins())
            if (*Pin) {
                auto* Node = Pin->GetTheOnlyConnected()->GetOwner()->As<CExecuteNode>();
                m_Manager->Execute(Node);
            }

        return nullptr;
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
        Result.Set(Operate(EMathOperation::Add, ValueA.GetValueType(), ValueB.GetValueType(), ValueA.AsDouble(), ValueB.AsDouble(), &TrivialResult), TrivialResult);
        return Result.GetValueType() != "void";
    }
};

class CDelayNode : public CExecuteNode, public INodeImGuiPupUpExt, public INodeInnerText {
public:
    std::string_view GetCategory() noexcept override
    {
        return "Time";
    }

    std::string GetTitle() override
    {
        return "Delay";
    }

    bool Render() override
    {
        ImGui::InputFloat("Delay (second)", &m_Delay);
        m_Delay = std::max(m_Delay, 0.f);

        return true;
    }

    void Execute() override
    {
        const auto StartTime = std::chrono::steady_clock::now();
        const auto DelayDuration = std::chrono::duration<float>(m_Delay);
        const auto EndTime = StartTime + DelayDuration;

        // How often the GUI should update
        constexpr auto UpdateInterval = std::chrono::milliseconds(100);

        while (true) {
            auto Now = std::chrono::steady_clock::now();

            if (Now >= EndTime) {
                break;
            }

            const std::chrono::duration<float> Remaining = EndTime - Now;
            SetInnerText(std::format("{:.1f}", Remaining.count()));

            // If remaining time is less than 100ms, only sleep for the exact remaining time.
            if (Remaining < UpdateInterval) {
                std::this_thread::sleep_for(Remaining);
            } else {
                std::this_thread::sleep_for(UpdateInterval);
            }
        }

        SetInnerText(std::format("{:.1f}", m_Delay));
    }

    void WriteExtraContext(std::string& ExtContext) const override
    {
        ExtContext = std::to_string(m_Delay);
    }
    void ReadExtraContext(const std::string& ExtContext) override
    {
        m_Delay = std::stof(ExtContext);
        SetInnerText(std::format("{:.1f}", m_Delay));
    }

protected:
    float m_Delay = 0;
};

class CTrivialValueNode : public CBaseNode, public INodeImGuiPupUpExt, public INodeInnerText {

public:
    CTrivialValueNode()
    {
        EmplacePin<CDataPin>(false)->SetIsUniversalPin().AddOnConnectionChanges([this](auto, auto, auto IsConnect) {
            if (IsConnect)
                OnStartPopup();
        });
    }

    std::string GetTitle() override
    {
        return "Trivial Value";
    }

    void OnStartPopup() override
    {
        auto* OurPin = static_cast<CDataPin*>(GetOutputPins()[0].get());

        const auto* OwnerEnd = static_cast<CDataPin*>(OurPin->GetTheOnlyConnected());
        if (OwnerEnd == nullptr) {
            OurPin->SetValueType(OtherTy = "void");
            return;
        }

        OurPin->SetValueType(OtherTy = OwnerEnd->GetValueType());
        if (OtherTy != "void") {
            m_StrBuffer[ToString(OtherTy, OurPin->AsDouble()).copy(m_StrBuffer, sizeof(m_StrBuffer) - 1)] = '\0';
        }
    }

    void OnEndPopup() override
    {
        if (OtherTy == "void")
            return;

        double TrivialResult = 0;
        FromString(OtherTy, std::string_view { m_StrBuffer }, TrivialResult);
        static_cast<CDataPin*>(GetOutputPins()[0].get())->Set(OtherTy, TrivialResult);
    }

    bool Render() override
    {
        if (OtherTy == "void") {
            return false;
        }

        ImGui::InputText("Value", m_StrBuffer, std::span { m_StrBuffer }.size());

        return true;
    }

    std::string_view GetCategory() noexcept override
    {
        return "Util";
    }

    void WriteExtraContext(std::string& ExtContext) const override
    {
        if (OtherTy == "void")
            return;
        ExtContext = std::format("{} {}", OtherTy, m_StrBuffer);
    }
    void ReadExtraContext(const std::string& ExtContext) override
    {
        if (ExtContext.empty())
            return;

        const auto SpaceLocation = ExtContext.find(' ');
        OtherTy = ExtContext.substr(0, SpaceLocation);
        m_StrBuffer[ExtContext.substr(SpaceLocation + 1).copy(m_StrBuffer, sizeof(m_StrBuffer) - 1)] = '\0';
        OnEndPopup();
    }

private:
    std::string OtherTy;
    char m_StrBuffer[256] { };
};

class CActionReplayNode : public CExecuteNode, public INodeImGuiPupUpExt, public INodeInnerText {
public:
    std::string GetTitle() override
    {
        return "Event Record";
    }

    void InputCallback(const SInputEvent& InputEvent)
    {
        if (!m_IsRecording) {
            return;
        }

        using namespace std::chrono;

        const auto CurrentTime = steady_clock::now();
        if (m_PlaybackEvent.empty())
            m_LastEventTime = m_StartTime = CurrentTime;
        const auto Timepoint = duration_cast<milliseconds>(CurrentTime - m_StartTime).count();
        const auto SinceLastEvent = duration_cast<milliseconds>(CurrentTime - m_LastEventTime).count();
        m_LastEventTime = CurrentTime;

        if (InputEvent.type == EInputType::KeyDown) {
#ifdef _WIN32
            if (InputEvent.keyCode == VK_ESCAPE)
#else
            if (InputEvent.keyCode == kVK_Escape)
#endif
            {
                m_IsRecording = false;
                return;
            }
        }

        if (InputEvent.type != EInputType::MouseMove) {
            m_PlaybackEvent.emplace_back(Timepoint, InputEvent);
            SetInnerText(std::format("{:<7} action(s)", m_PlaybackEvent.size()));
            m_Logger.append(std::format("{}ms(+{}) {} \n", Timepoint, SinceLastEvent, InputEvent.ToString()).c_str());
        }
    }

    void OnStartPopup() override
    {
        m_InputMonitor = CInputService::Get().Subscribe(std::bind_front(&CActionReplayNode::InputCallback, this));
    }

    void OnEndPopup() override
    {
        CInputService::Get().Unsubscribe(m_InputMonitor);
    }

    bool Render() override
    {
        ImGui::BeginDisabled(m_IsRecording);
        if ((m_Logger.empty() && ImGui::Button("Start record")) || (!m_Logger.empty() && ImGui::Button("Restart record"))) {
            m_Logger.clear();
            m_PlaybackEvent.clear();
            m_IsRecording = true;
        }
        if (m_IsRecording) {
            ImGui::SameLine();
            ImGui::TextDisabled("Press ESC key to stop.");
        }
        ImGui::EndDisabled();

        ImGui::Separator();

        ImGui::BeginChild("Log", { 1200, 400 });
        if (m_Logger.empty()) {
            ImGui::TextDisabled("First event will have a timestamp of zero, record will end with ESC key press.");
        } else {
            if (!m_Logger.empty())
                ImGui::TextUnformatted(m_Logger.begin(), m_Logger.end());
            if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY())
                ImGui::SetScrollHereY(1.0f);
        }
        ImGui::EndChild();

        return true;
    }

    std::string_view GetCategory() noexcept override
    {
        return "Event";
    }

    void Execute() override
    {
        CInputDispatcher::Get().Play(m_PlaybackEvent);
        size_t OldProgress = -1;
        while (CInputDispatcher::Get().IsPlaying()) {
            if (!*m_Manager) {
                CInputDispatcher::Get().Stop();
                break;
            }

            if (const auto NewProgress = CInputDispatcher::Get().GetPlaybackProgress(); OldProgress != NewProgress) {
                OldProgress = NewProgress;
                SetInnerText(std::format("{:<3}/{:<3} action(s)", OldProgress, m_PlaybackEvent.size()));
            }
            std::this_thread::yield();
        }

        SetInnerText(std::format("{:<7} action(s)", m_PlaybackEvent.size()));
    }

    void WriteExtraContext(std::string& ExtContext) const override
    {
        if (m_PlaybackEvent.empty())
            return;

        const auto Data = std::as_bytes(std::span(m_PlaybackEvent));

        // Reserve a reasonable estimate to prevent reallocations
        ExtContext.reserve(ExtContext.size() + Data.size() * 4);

        char Buf[32]; // 20 chars for size_t + 1 space + 3 chars for uint8_t + 1 pipe = 25 max

        auto AppendRun = [&](size_t RunLength, std::byte ByteVal, bool IsLast) {
            // Convert RunLength
            auto [p1, ec1] = std::to_chars(Buf, Buf + sizeof(Buf), RunLength);
            *p1++ = ' ';

            // Convert Byte value
            auto [p2, ec2] = std::to_chars(p1, Buf + sizeof(Buf), std::to_integer<uint8_t>(ByteVal));

            if (!IsLast)
                *p2++ = '|';

            ExtContext.append(Buf, p2);
        };

        size_t RLE = 1;
        std::byte LastByte = Data[0];

        for (size_t Index = 1; Index < Data.size(); ++Index) {
            if (Data[Index] != LastByte) {
                AppendRun(RLE, LastByte, false);
                LastByte = Data[Index];
                RLE = 1;
            } else {
                ++RLE;
            }
        }

        AppendRun(RLE, LastByte, true);
    }
    void ReadExtraContext(const std::string& ExtContext) override
    {
        m_Logger.clear();
        m_PlaybackEvent.clear();

        if (ExtContext.empty())
            return;

        const char* Ptr = ExtContext.data();
        const char* End = Ptr + ExtContext.size();

        // ========================================================================
        //  Calculate total uncompressed size to avoid reallocations
        // ========================================================================
        size_t TotalBytes = 0;
        while (Ptr < End) {
            size_t RunLength = 0;
            auto Res = std::from_chars(Ptr, End, RunLength);
            if (Res.ec != std::errc { })
                break; // Stop on malformed data

            TotalBytes += RunLength;

            // Fast-forward to the next '|' using highly optimized C-string search
            Ptr = static_cast<const char*>(std::memchr(Res.ptr, '|', End - Res.ptr));
            if (!Ptr)
                break; // No more pipes, we are done
            ++Ptr; // Skip the '|'
        }

        using ElementType = typename decltype(m_PlaybackEvent)::value_type;
        m_PlaybackEvent.resize(TotalBytes / sizeof(ElementType));

        // ========================================================================
        //  Decode directly into the destination memory
        // ========================================================================
        uint8_t* OutPtr = reinterpret_cast<uint8_t*>(m_PlaybackEvent.data());
        Ptr = ExtContext.data(); // Reset pointer to the start of the string

        while (Ptr < End) {
            size_t RunLength = 0;
            uint8_t ByteVal = 0;

            // 1. Parse Run Length
            auto Res1 = std::from_chars(Ptr, End, RunLength);
            if (Res1.ec != std::errc { })
                break;
            Ptr = Res1.ptr;

            // 2. Skip space
            if (Ptr < End && *Ptr == ' ')
                ++Ptr;

            // 3. Parse Byte Value
            auto Res2 = std::from_chars(Ptr, End, ByteVal);
            if (Res2.ec != std::errc { })
                break;
            Ptr = Res2.ptr;

            // 4. Write directly to the final memory location using memset
            std::memset(OutPtr, ByteVal, RunLength);
            OutPtr += RunLength;

            // 5. Skip pipe
            if (Ptr < End && *Ptr == '|')
                ++Ptr;
        }

        uint32_t LastTimePoint = 0;
        for (const auto& [TimePoint, InputEvent] : m_PlaybackEvent) {
            m_Logger.append(std::format("{}ms(+{}) {} \n", TimePoint, TimePoint - LastTimePoint, InputEvent.ToString()).c_str());
            LastTimePoint = TimePoint;
        }

        SetInnerText(std::format("{:<7} action(s)", m_PlaybackEvent.size()));
    }

private:
    CInputService::SubscriptionID m_InputMonitor { };

    ImGuiTextBuffer m_Logger;
    bool m_IsRecording = false;

    std::vector<SPlaybackEvent> m_PlaybackEvent;
    std::chrono::steady_clock::time_point m_StartTime;
    std::chrono::steady_clock::time_point m_LastEventTime;
};

REGISTER_MACROS(CActionReplayNode, CDelayNode, CTrivialValueNode, CAddNode, CEntranceNode, COnTriggerNode, CToStringNode, CPrintingNode, CBranchingNode, CSequenceNode)
ENABLE_IMGUI()