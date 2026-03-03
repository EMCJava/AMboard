//
// Created by LYS on 2/15/2026.
//

#pragma once

#include "MacroDefines.hxx"
#include "Pin.hxx"

#include <string_view>
#include <typeindex>

#define PinGet(Ty) TryGet<Ty>(#Ty)
#define PinSet(Ty, Val) Set<Ty>(#Ty, Val)

class MACRO_API CDataPin : public CPin {

protected:
    void PreConnectPin(CPin* NewPin) noexcept override;

public:
    CDataPin(CBaseNode* Owner, bool IsInputPin) noexcept;

    template <typename Ty>
    Ty TryGet(const std::string_view& TyStr) noexcept
    {
        if (m_DataType == TyStr) [[likely]] {
            return *reinterpret_cast<Ty*>(m_Data);
        }

        return { };
    }

    template <typename Ty>
        requires std::is_trivial_v<Ty>
    Ty& Set(const std::string_view& TyStr, Ty NewValue) noexcept
    {
        m_DataType = TyStr;
        return *reinterpret_cast<Ty*>(m_Data) = NewValue;
    }

    void Assign(const CDataPin* Source);


    template <typename Self>
    [[nodiscard]] auto* GetData(this Self&& s) noexcept { return s.m_Data; }
    auto SetValueType(auto Ty) noexcept { return m_DataType = Ty; }

    [[nodiscard]] const auto& GetDataType() const noexcept { return m_DataType; }

protected:
    std::string_view m_DataType = "void";
    alignas(double) uint8_t m_Data[8];
};
