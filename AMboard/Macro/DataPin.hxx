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

    void Assign(const CDataPin* Source);


    template <typename Self>
    [[nodiscard]] auto* GetData(this Self&& s) noexcept { return s.m_Data; }
    auto SetValueType(auto Ty) noexcept { return m_DataType = Ty; }

    [[nodiscard]] operator std::type_index() const noexcept { return m_DataType; }

protected:
    std::type_index m_DataType { std::type_index(typeid(void)) };

    alignas(double) uint8_t m_Data[8];
};
