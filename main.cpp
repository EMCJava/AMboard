#include <Control/EventHooker.hxx>
#include <chrono>

#include <Macro/DataPin.hxx>
#include <Macro/ExecuteNode.hxx>
#include <Macro/FlowPin.hxx>

#include <iostream>

#include <windows.h>

class CPrintingNode : public CExecuteNode {
public:
    CPrintingNode(std::string Msg = "")
        : m_Msg(std::move(Msg))
    {
        AddInputOutputFlowPin();
    }

protected:
    void Execute() override
    {
        std::cout << "Printing Node...  " << m_Msg << std::endl;
    }

    std::string m_Msg;
};

class CBranchingNode : public CExecuteNode {
public:
    CBranchingNode()
    {
        AddInputOutputFlowPin();
        EmplacePin<CDataPin>(true);
        EmplacePin<CFlowPin>(false);
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

    void ExecuteNode() override
    {
        for (const auto* Pin : GetFlowOutputPins())
            if (*Pin)
                Pin->GetConnectedOwner()->As<CExecuteNode>()->ExecuteNode();
    }
};

int main()
{
    CBranchingNode BranchingNode;
    if (auto DataPins = BranchingNode.GetInputPinsWith<EPinType::Data>(); !DataPins.empty())
        DataPins.front()->As<CDataPin>()->Set(true);

    CPrintingNode Node1("true"), Node2("false");

    CSequenceNode SequenceNode;
    SequenceNode.EmplacePin<CFlowPin>(false)->ConnectPin(Node1.GetFlowInputPins().front());
    SequenceNode.EmplacePin<CFlowPin>(false)->ConnectPin(Node1.GetFlowInputPins().front());
    SequenceNode.EmplacePin<CFlowPin>(false)->ConnectPin(Node1.GetFlowInputPins().front());
    SequenceNode.EmplacePin<CFlowPin>(false)->ConnectPin(Node1.GetFlowInputPins().front());
    SequenceNode.EmplacePin<CFlowPin>(false)->ConnectPin(Node1.GetFlowInputPins().front());

    auto OutputPins = BranchingNode.GetFlowOutputPins();
    OutputPins[0]->ConnectPin(SequenceNode.GetFlowInputPins().front());
    OutputPins[1]->ConnectPin(Node2.GetFlowInputPins().front());

    BranchingNode.ExecuteNode();

    SEventHooker EH;
    EH.KeyboardEventCallback = [](const SKeyboardEventData& Data) static {
        std::cout << Data.vkCode << std::endl;
    };

    MSG msg;
    while (GetMessage(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return 0;
}
