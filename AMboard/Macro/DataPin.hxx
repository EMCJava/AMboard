//
// Created by LYS on 2/15/2026.
//

#pragma once

#include "MacroDefines.hxx"
#include "Pin.hxx"

#include <typeindex>

class MACRO_API CDataPin : public CPin {

protected:
    void PreConnectPin(CPin* NewPin) noexcept override;

public:
    CDataPin(CBaseNode* Owner, bool IsInputPin) noexcept;

    template <typename Ty>
    Ty TryGet() noexcept
    {
        if (m_DataType == std::type_index(typeid(Ty))) [[likely]] {
            return *reinterpret_cast<Ty*>(m_Data);
        }

        return { };
    }

    template <typename Ty>
        requires std::is_trivial_v<Ty>
    Ty& Set(Ty NewValue) noexcept
    {
        m_DataType = std::type_index(typeid(Ty));
        return *reinterpret_cast<Ty*>(m_Data) = NewValue;
    }

protected:
    struct NullTy { };
    std::type_index m_DataType { std::type_index(typeid(NullTy)) };

    alignas(double) uint8_t m_Data[8];
};
