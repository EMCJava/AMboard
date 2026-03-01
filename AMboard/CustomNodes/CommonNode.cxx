//
// Created by LYS on 2/26/2026.
//

#include <AMboard/Macro/DataPin.hxx>
#include <AMboard/Macro/ExecuteNode.hxx>

#include <iostream>

class CEntranceNode : public CExecuteNode {
public:
    CEntranceNode()
    {
        EmplacePin<CFlowPin>(false);
    }
};

class CPrintingNode : public CExecuteNode {
public:
    CPrintingNode(std::string Msg = "")
        : m_Msg(std::move(Msg))
    {
        AddInputOutputFlowPin();
    }

    std::string_view GetCategory() noexcept override
    {
        return "Logging";
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
                Pin->GetTheOnlyPin()->GetOwner()->As<CExecuteNode>()->ExecuteNode();
    }
};

REGISTER_MACROS(CEntranceNode, CPrintingNode, CBranchingNode, CSequenceNode)