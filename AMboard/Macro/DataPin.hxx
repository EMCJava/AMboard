//
// Created by LYS on 2/15/2026.
//

#pragma once

#include "MacroDefines.hxx"
#include "Pin.hxx"

#include <bit>
#include <memory>
#include <string_view>

#define PinGetTrivial(Ty) TryGetTrivial<Ty>(#Ty)
#define PinSet(Ty, Val) Set<Ty>(#Ty, Val)

class MACRO_API CDataPin : public CPin {

protected:
    void PreConnectPin(CPin* NewPin) noexcept override;
    bool Compatible(CPin* NewPin) noexcept override;

public:
    CDataPin(CBaseNode* Owner, bool IsInputPin) noexcept;

    std::string_view GetToolTips() const noexcept override;

    template <typename Ty>
        requires std::is_trivial_v<Ty>
    Ty TryGetTrivial(const std::string_view& TyStr) const noexcept
    {
        if (m_DataType == TyStr) [[likely]] {
            union {
                void* Ptr;
                Ty Data;
            } Tmp { m_SharedData.get() };
            return Tmp.Data;
        }

        return { };
    }

    template <typename Ty>
    Ty& Get(const std::string_view& TyStr) const noexcept
    {
        if (m_DataType == TyStr) [[likely]] {
            return *static_cast<Ty*>(m_SharedData.get());
        }

        std::unreachable();
    }

    template <typename Ty>
        requires std::is_trivial_v<Ty>
    Ty& Set(const std::string_view& TyStr, Ty NewValue) noexcept
    {
        return Set(TyStr, &NewValue);
    }

    template <typename Ty>
        requires std::is_trivial_v<Ty>
    Ty& Set(const std::string_view& TyStr, Ty* NewValue) noexcept
    {
        m_DataType = TyStr;
        m_SharedData.reset(const_cast<void*>(reinterpret_cast<const void*&>(*NewValue)), [](void*) static noexcept { });
        return *NewValue;
    }

    template <typename Ty>
    Ty& Set(const std::string_view& TyStr, std::shared_ptr<Ty> NewValue) noexcept
    {
        m_DataType = TyStr;
        m_SharedData = std::reinterpret_pointer_cast<Ty>(NewValue);
        return *NewValue;
    }

    [[nodiscard]] double AsDouble() const noexcept
    {
        return std::bit_cast<double>(m_SharedData.get());
    }

    void Assign(const CDataPin* Source);

    decltype(auto) SetValueType(auto&& Ty) noexcept { m_DataType = Ty; return* this; }
    [[nodiscard]] const auto& GetValueType() const noexcept { return m_DataType; }

    void SetIsUniversalPin(const bool Universal = true) noexcept { m_IsUniversalPin = Universal; }

protected:
    std::string_view m_DataType = "void";

    bool m_IsUniversalPin = false;

    std::shared_ptr<void> m_SharedData;
};
