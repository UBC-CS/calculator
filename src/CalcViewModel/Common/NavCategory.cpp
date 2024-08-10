// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#include "pch.h"
#include "NavCategory.h"
#include "AppResourceProvider.h"
#include "Common/LocalizationStringUtil.h"
#include <initializer_list>

using namespace CalculatorApp;
using namespace CalculatorApp::ViewModel::Common;
using namespace CalculatorApp::ViewModel;
using namespace Concurrency;
using namespace Platform;
using namespace Platform::Collections;
using namespace Windows::Foundation::Collections;
using namespace Windows::Management::Policies;
using namespace Windows::System;

namespace UCM = UnitConversionManager;

// Calculator categories always support negative and positive.
static constexpr bool SUPPORTS_ALL = true;

// Converter categories usually only support positive.
static constexpr bool SUPPORTS_NEGATIVE = true;
static constexpr bool POSITIVE_ONLY = false;

// vvv THESE CONSTANTS SHOULD NEVER CHANGE vvv
static constexpr int STANDARD_ID = 0;
static constexpr int SCIENTIFIC_ID = 1;
static constexpr int PROGRAMMER_ID = 2;
static constexpr int DATE_ID = 3;
static constexpr int VOLUME_ID = 4;
static constexpr int LENGTH_ID = 5;
static constexpr int WEIGHT_ID = 6;
static constexpr int TEMPERATURE_ID = 7;
static constexpr int ENERGY_ID = 8;
static constexpr int AREA_ID = 9;
static constexpr int SPEED_ID = 10;
static constexpr int TIME_ID = 11;
static constexpr int POWER_ID = 12;
static constexpr int DATA_ID = 13;
static constexpr int PRESSURE_ID = 14;
static constexpr int ANGLE_ID = 15;
static constexpr int CURRENCY_ID = 16;
static constexpr int GRAPHING_ID = 17;
// ^^^ THESE CONSTANTS SHOULD NEVER CHANGE ^^^

namespace // put the utils within this TU
{
    Platform::String^ CurrentUserId;

    bool IsGraphingModeEnabled()
    {
        static auto enabled = []
        {
            auto user = User::GetFromId(CurrentUserId);
            if (user == nullptr)
            {
                return true;
            }
            return NamedPolicy::GetPolicyFromPathForUser(user, L"Education", L"AllowGraphingCalculator")->GetBoolean();
        }();
        return enabled;
    }

    // The order of items in this list determines the order of items in the menu.
    const std::vector<NavCategoryInitializer> s_categoryManifest {
      NavCategoryInitializer{ ViewMode::Standard,
          STANDARD_ID,
          L"Standard",
          L"StandardMode",
          L"\uE8EF",
          CategoryGroupType::Calculator,
          MyVirtualKey::Number1,
          L"1",
          SUPPORTS_ALL },
      NavCategoryInitializer{ ViewMode::Scientific,
          SCIENTIFIC_ID,
          L"Scientific",
          L"ScientificMode",
          L"\uF196",
          CategoryGroupType::Calculator,
          MyVirtualKey::Number2,
          L"2",
          SUPPORTS_ALL },
    };
} // namespace unnamed


bool NavCategory::IsCalculatorViewMode(ViewModeType mode)
{
    // Historically, Calculator modes are Standard, Scientific, and Programmer.
    return !IsDateCalculatorViewMode(mode) && !IsGraphingCalculatorViewMode(mode) && IsModeInCategoryGroup(mode, CategoryGroupType::Calculator);
}

bool NavCategory::IsGraphingCalculatorViewMode(ViewModeType mode)
{
    return mode == ViewModeType::Graphing;
}

bool NavCategory::IsDateCalculatorViewMode(ViewModeType mode)
{
    return mode == ViewModeType::Date;
}

bool NavCategory::IsConverterViewMode(ViewModeType mode)
{
    return IsModeInCategoryGroup(mode, CategoryGroupType::Converter);
}

bool NavCategory::IsModeInCategoryGroup(ViewModeType mode, CategoryGroupType type)
{
    return std::any_of(
        s_categoryManifest.cbegin(),
        s_categoryManifest.cend(),
        [mode, type](const auto& initializer) {
            return initializer.viewMode == mode && initializer.groupType == type;
        });
}

NavCategoryGroup::NavCategoryGroup(const NavCategoryGroupInitializer& groupInitializer)
    : m_Categories(ref new Vector<NavCategory ^>())
{
    m_GroupType = groupInitializer.type;

    auto resProvider = AppResourceProvider::GetInstance();
    m_Name = resProvider->GetResourceString(StringReference(groupInitializer.headerResourceKey));
    String ^ groupMode = resProvider->GetResourceString(StringReference(groupInitializer.modeResourceKey));
    String ^ automationName = resProvider->GetResourceString(StringReference(groupInitializer.automationResourceKey));

    String ^ navCategoryHeaderAutomationNameFormat = resProvider->GetResourceString(L"NavCategoryHeader_AutomationNameFormat");
    m_AutomationName = LocalizationStringUtil::GetLocalizedString(navCategoryHeaderAutomationNameFormat, automationName);

    String ^ navCategoryItemAutomationNameFormat = resProvider->GetResourceString(L"NavCategoryItem_AutomationNameFormat");

    for (const NavCategoryInitializer& categoryInitializer : s_categoryManifest)
    {
        if (categoryInitializer.groupType == groupInitializer.type)
        {
            String ^ nameResourceKey = StringReference(categoryInitializer.nameResourceKey);
            String ^ categoryName = resProvider->GetResourceString(nameResourceKey + "Text");
            String ^ categoryAutomationName = LocalizationStringUtil::GetLocalizedString(navCategoryItemAutomationNameFormat, categoryName, m_Name);

            m_Categories->Append(ref new NavCategory(
                categoryName,
                categoryAutomationName,
                StringReference(categoryInitializer.glyph),
                categoryInitializer.accessKey.has_value() ? ref new String(categoryInitializer.accessKey->c_str())
                                                         : resProvider->GetResourceString(nameResourceKey + "AccessKey"),
                groupMode,
                categoryInitializer.viewMode,
                categoryInitializer.supportsNegative,
                categoryInitializer.viewMode != ViewMode::Graphing));
        }
    }
}

void NavCategoryStates::SetCurrentUser(Platform::String^ userId)
{
    CurrentUserId = userId;
}

IObservableVector<NavCategoryGroup ^> ^ NavCategoryStates::CreateMenuOptions()
{
    auto menuOptions = ref new Vector<NavCategoryGroup ^>();
    menuOptions->Append(CreateCalculatorCategoryGroup());
    return menuOptions;
}

NavCategoryGroup ^ NavCategoryStates::CreateCalculatorCategoryGroup()
{
    return ref new NavCategoryGroup(
        NavCategoryGroupInitializer{ CategoryGroupType::Calculator, L"CalculatorModeTextCaps", L"CalculatorModeText", L"CalculatorModePluralText" });
}

NavCategoryGroup ^ NavCategoryStates::CreateConverterCategoryGroup()
{
    return ref new NavCategoryGroup(
        NavCategoryGroupInitializer{ CategoryGroupType::Converter, L"ConverterModeTextCaps", L"ConverterModeText", L"ConverterModePluralText" });
}

// This function should only be used when storing the mode to app data.
int NavCategoryStates::Serialize(ViewMode mode)
{
    const auto& citer = find_if(
        cbegin(s_categoryManifest),
        cend(s_categoryManifest),
        [mode](const auto& initializer) { return initializer.viewMode == mode; });

    return (citer != s_categoryManifest.cend()) ? citer->serializationId : -1;
}

// This function should only be used when restoring the mode from app data.
ViewMode NavCategoryStates::Deserialize(Platform::Object ^ obj)
{
    // If we cast directly to ViewMode we will fail
    // because we technically store an int.
    // Need to cast to int, then ViewMode.
    auto boxed = dynamic_cast<Box<int> ^>(obj);
    if (boxed != nullptr)
    {
        int serializationId = boxed->Value;
        const auto& citer = find_if(
            cbegin(s_categoryManifest),
            cend(s_categoryManifest),
            [serializationId](const auto& initializer) { return initializer.serializationId == serializationId; });

        return citer != s_categoryManifest.cend() ?
                   (citer->viewMode == ViewMode::Graphing ?
                       (IsGraphingModeEnabled() ? citer->viewMode : ViewMode::None)
                       : citer->viewMode)
                   : ViewMode::None;
    }
    else
    {
        return ViewMode::None;
    }
}

ViewMode NavCategoryStates::GetViewModeForFriendlyName(String ^ name)
{
    const auto& citer = find_if(
        cbegin(s_categoryManifest),
        cend(s_categoryManifest),
        [name](const auto& initializer) { return wcscmp(initializer.friendlyName, name->Data()) == 0; });

    return (citer != s_categoryManifest.cend()) ? citer->viewMode : ViewMode::None;
}

String ^ NavCategoryStates::GetFriendlyName(ViewMode mode)
{
    const auto& citer = find_if(
        cbegin(s_categoryManifest),
        cend(s_categoryManifest),
        [mode](const auto& initializer) { return initializer.viewMode == mode; });

    return (citer != s_categoryManifest.cend()) ? StringReference(citer->friendlyName) : L"None";
}

String ^ NavCategoryStates::GetNameResourceKey(ViewMode mode)
{
    const auto& citer = find_if(
        cbegin(s_categoryManifest),
        cend(s_categoryManifest),
        [mode](const auto& initializer) { return initializer.viewMode == mode; });

    return (citer != s_categoryManifest.cend()) ? StringReference(citer->nameResourceKey) + "Text" : nullptr;
}

CategoryGroupType NavCategoryStates::GetGroupType(ViewMode mode)
{
    const auto& citer = find_if(
        cbegin(s_categoryManifest),
        cend(s_categoryManifest),
        [mode](const auto& initializer) { return initializer.viewMode == mode; });

    return (citer != s_categoryManifest.cend()) ? citer->groupType : CategoryGroupType::None;
}

// GetIndex is 0-based, GetPosition is 1-based
int NavCategoryStates::GetIndex(ViewMode mode)
{
    int position = GetPosition(mode);
    return std::max(-1, position - 1);
}

int NavCategoryStates::GetFlatIndex(ViewMode mode)
{
    int index = -1;
    CategoryGroupType type = CategoryGroupType::None;
    const auto& citer = find_if(
        cbegin(s_categoryManifest),
        cend(s_categoryManifest),
        [mode, &type, &index](const auto& initializer) {
            ++index;
            if (initializer.groupType != type)
            {
                type = initializer.groupType;
                ++index;
            }
            return initializer.viewMode == mode;
        });

    return (citer != s_categoryManifest.cend()) ? index : -1;
}

// GetIndex is 0-based, GetPosition is 1-based
int NavCategoryStates::GetIndexInGroup(ViewMode mode, CategoryGroupType type)
{
    int index = -1;
    const auto& citer = find_if(
        cbegin(s_categoryManifest),
        cend(s_categoryManifest),
        [mode, type, &index](const auto& initializer) {
            if (initializer.groupType == type)
            {
                ++index;
                return initializer.viewMode == mode;
            }
            return false;
        });

    return (citer != s_categoryManifest.cend()) ? index : -1;
}

// GetIndex is 0-based, GetPosition is 1-based
int NavCategoryStates::GetPosition(ViewMode mode)
{
    int position = 0;
    const auto& citer = find_if(
        cbegin(s_categoryManifest),
        cend(s_categoryManifest),
        [mode, &position](const auto& initializer) {
            ++position;
            return initializer.viewMode == mode;
        });

    return (citer != s_categoryManifest.cend()) ? position : -1;
}

ViewMode NavCategoryStates::GetViewModeForVirtualKey(MyVirtualKey virtualKey)
{
    const auto& citer = find_if(
        cbegin(s_categoryManifest),
        cend(s_categoryManifest),
        [virtualKey](const auto& initializer) { return initializer.virtualKey == virtualKey; });

    return (citer != s_categoryManifest.end()) ? citer->viewMode : ViewMode::None;
}

void NavCategoryStates::GetCategoryAcceleratorKeys(IVector<MyVirtualKey> ^ accelerators)
{
    if (accelerators != nullptr)
    {
        accelerators->Clear();
        for (const auto& category : s_categoryManifest)
        {
            if (category.virtualKey != MyVirtualKey::None)
            {
                accelerators->Append(category.virtualKey);
            }
        }
    }
}

bool NavCategoryStates::IsValidViewMode(ViewMode mode)
{
    const auto& citer = find_if(
        cbegin(s_categoryManifest),
        cend(s_categoryManifest),
        [mode](const auto& initializer) { return initializer.viewMode == mode; });

    return citer != s_categoryManifest.cend();
}

bool NavCategoryStates::IsViewModeEnabled(ViewMode mode)
{
    return mode != ViewMode::Graphing ? true : IsGraphingModeEnabled();
}

