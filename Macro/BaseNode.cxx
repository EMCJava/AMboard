//
// Created by LYS on 2/15/2026.
//

#include "BaseNode.hxx"

CBaseNode::~CBaseNode()
{
    m_IsDestructing = true;
}