#include "Macro/DataPin.hxx"

#include <Macro/ExecuteNode.hxx>
#include <Macro/FlowPin.hxx>

#include <iostream>

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

int main()
{
    CBranchingNode BranchingNode;
    if (auto DataPins = BranchingNode.GetInputPinsWith<EPinType::Data>(); !DataPins.empty())
        DataPins.front()->As<CDataPin>()->Set(false);

    CPrintingNode Node1("true"), Node2("false");

    auto OutputPins = BranchingNode.GetFlowOutputPins();
    OutputPins[0]->ConnectPin(Node1.GetFlowInputPins().front());
    OutputPins[1]->ConnectPin(Node2.GetFlowInputPins().front());

    BranchingNode.ExecuteNode();

    return 0;
}
