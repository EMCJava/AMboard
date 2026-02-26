//
// Created by LYS on 2/26/2026.
//

#include <AMboard/Macro/DataPin.hxx>
#include <AMboard/Macro/ExecuteNode.hxx>

#include <iostream>

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

NODE_EXT_EXPORT CBaseNode* create_node_ext()
{
    return new CBranchingNode;
}

NODE_EXT_EXPORT void destroy_node_ext(const CBaseNode* Node)
{
    return delete Node;
}