#include <Macro/ExecuteNode.hxx>
#include <Macro/FlowPin.hxx>

#include <iostream>

class CPrintingNode : public CExecuteNode {
protected:
    void Execute() override
    {
        std::cout << "Printing Node..." << std::endl;
    }
};

int main()
{
    CPrintingNode Node1, Node2;

    Node1.EmplacePin<CFlowPin>(false)->ConnectPin(Node2.EmplacePin<CFlowPin>(true));
    Node1.ExecuteNode();

    return 0;
}
