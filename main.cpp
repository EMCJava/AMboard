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

    Node1.GetFlowOutputPins().front()->ConnectPin(Node2.GetFlowInputPins().front());

    Node1.ExecuteNode();

    return 0;
}
