#include "UiaHelper.h"

#include <OleAuto.h>
#include <comutil.h>

namespace {

QString VariantToWString(const VARIANT& var) {
    if (var.vt == VT_BSTR && var.bstrVal) {
        return QString::fromWCharArray(var.bstrVal, SysStringLen(var.bstrVal));
    }
    if (var.vt == VT_EMPTY || var.vt == VT_NULL) {
        return {};
    }
    VARIANT strVar = {0};
    VariantInit(&strVar);
    if (SUCCEEDED(VariantChangeType(&strVar, &var, 0, VT_BSTR)) && strVar.vt == VT_BSTR && strVar.bstrVal) {
        QString result = QString::fromWCharArray(strVar.bstrVal, SysStringLen(strVar.bstrVal));
        VariantClear(&strVar);
        return result;
    }
    VariantClear(&strVar);
    return {};
}

} // namespace

bool UiaHelper::Initialize() {
    if (automation_) {
        return true;
    }
    // Qt does not initialize COM on its threads, so do it here before calling
    // into UI Automation (which is a COM server).
    HRESULT cohr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
    if (FAILED(cohr) && cohr != RPC_E_CHANGED_MODE) {
        lastError_ = QStringLiteral("Cannot initialize COM (UI Automation).");
        return false;
    }
    HRESULT hr = CoCreateInstance(CLSID_CUIAutomation, nullptr, CLSCTX_INPROC_SERVER,
                                  IID_PPV_ARGS(&automation_));
    if (FAILED(hr) || !automation_) {
        lastError_ = QStringLiteral("Cannot initialize UI Automation.");
        return false;
    }
    return true;
}

std::vector<ControlInfo> UiaHelper::EnumerateEditableControls(HWND targetWindow) {
    std::vector<ControlInfo> result;
    if (!automation_ || !targetWindow || !::IsWindow(targetWindow)) {
        lastError_ = QStringLiteral("Invalid target window.");
        return result;
    }

    Microsoft::WRL::ComPtr<IUIAutomationElement> root;
    HRESULT hr = automation_->ElementFromHandle(targetWindow, &root);
    if (FAILED(hr) || !root) {
        lastError_ = QStringLiteral("Cannot get UIA element from target window.");
        return result;
    }

    Microsoft::WRL::ComPtr<IUIAutomationCondition> editCondition;
    Microsoft::WRL::ComPtr<IUIAutomationCondition> docCondition;
    Microsoft::WRL::ComPtr<IUIAutomationCondition> comboCondition;

    automation_->CreatePropertyCondition(UIA_ControlTypePropertyId,
                                         _variant_t(static_cast<int>(UIA_EditControlTypeId)),
                                         &editCondition);
    automation_->CreatePropertyCondition(UIA_ControlTypePropertyId,
                                         _variant_t(static_cast<int>(UIA_DocumentControlTypeId)),
                                         &docCondition);
    automation_->CreatePropertyCondition(UIA_ControlTypePropertyId,
                                         _variant_t(static_cast<int>(UIA_ComboBoxControlTypeId)),
                                         &comboCondition);

    SAFEARRAY* sa = SafeArrayCreateVector(VT_UNKNOWN, 0, 3);
    if (!sa) {
        lastError_ = QStringLiteral("Failed to create SAFEARRAY.");
        return result;
    }
    LONG idx = 0;
    IUnknown* unk = static_cast<IUnknown*>(editCondition.Get());
    SafeArrayPutElement(sa, &idx, unk);
    idx = 1;
    unk = static_cast<IUnknown*>(docCondition.Get());
    SafeArrayPutElement(sa, &idx, unk);
    idx = 2;
    unk = static_cast<IUnknown*>(comboCondition.Get());
    SafeArrayPutElement(sa, &idx, unk);

    Microsoft::WRL::ComPtr<IUIAutomationCondition> typeOr;
    hr = automation_->CreateOrConditionFromArray(sa, &typeOr);
    SafeArrayDestroy(sa);
    if (FAILED(hr) || !typeOr) {
        lastError_ = QStringLiteral("Failed to create OR condition.");
        return result;
    }

    Microsoft::WRL::ComPtr<IUIAutomationCondition> enabledCondition;
    automation_->CreatePropertyCondition(UIA_IsEnabledPropertyId, _variant_t(true), &enabledCondition);

    Microsoft::WRL::ComPtr<IUIAutomationCondition> valueAvailCond;
    Microsoft::WRL::ComPtr<IUIAutomationCondition> textAvailCond;
    automation_->CreatePropertyCondition(UIA_IsValuePatternAvailablePropertyId, _variant_t(true), &valueAvailCond);
    automation_->CreatePropertyCondition(UIA_IsTextPatternAvailablePropertyId, _variant_t(true), &textAvailCond);

    SAFEARRAY* saPattern = SafeArrayCreateVector(VT_UNKNOWN, 0, 2);
    if (!saPattern) {
        lastError_ = QStringLiteral("Failed to create pattern SAFEARRAY.");
        return result;
    }
    idx = 0;
    unk = static_cast<IUnknown*>(valueAvailCond.Get());
    SafeArrayPutElement(saPattern, &idx, unk);
    idx = 1;
    unk = static_cast<IUnknown*>(textAvailCond.Get());
    SafeArrayPutElement(saPattern, &idx, unk);

    Microsoft::WRL::ComPtr<IUIAutomationCondition> patternOr;
    hr = automation_->CreateOrConditionFromArray(saPattern, &patternOr);
    SafeArrayDestroy(saPattern);
    if (FAILED(hr) || !patternOr) {
        lastError_ = QStringLiteral("Failed to create pattern OR condition.");
        return result;
    }

    Microsoft::WRL::ComPtr<IUIAutomationCondition> andTypeEnabled;
    automation_->CreateAndCondition(typeOr.Get(), enabledCondition.Get(), &andTypeEnabled);

    Microsoft::WRL::ComPtr<IUIAutomationCondition> fullCondition;
    automation_->CreateAndCondition(andTypeEnabled.Get(), patternOr.Get(), &fullCondition);

    Microsoft::WRL::ComPtr<IUIAutomationElementArray> elements;
    hr = root->FindAll(TreeScope_Descendants, fullCondition.Get(), &elements);
    if (FAILED(hr) || !elements) {
        lastError_ = QStringLiteral("Cannot enumerate controls.");
        return result;
    }

    int count = 0;
    elements->get_Length(&count);

    constexpr int kMaxControls = 50;
    int resultIndex = 0;
    for (int i = 0; i < count && resultIndex < kMaxControls; ++i) {
        Microsoft::WRL::ComPtr<IUIAutomationElement> element;
        hr = elements->GetElement(i, &element);
        if (FAILED(hr) || !element) {
            continue;
        }

        BOOL offscreen = FALSE;
        if (SUCCEEDED(element->get_CurrentIsOffscreen(&offscreen)) && offscreen) {
            continue;
        }

        RECT rect = GetElementBoundingRect(element.Get());
        if (rect.right <= rect.left || rect.bottom <= rect.top) {
            continue;
        }

        ControlInfo info;
        info.element = element;
        info.index = resultIndex++;

        CONTROLTYPEID controlTypeId = 0;
        element->get_CurrentControlType(&controlTypeId);
        info.controlType = QString::fromWCharArray(ControlTypeName(controlTypeId));

        info.name = GetElementProperty(element.Get(), UIA_NamePropertyId);
        info.automationId = GetElementProperty(element.Get(), UIA_AutomationIdPropertyId);
        info.currentValue = GetElementValue(element.Get());
        info.boundingRect = rect;

        if (info.currentValue.size() > 200) {
            info.currentValue.truncate(200);
            info.currentValue += QStringLiteral("...");
        }

        result.push_back(std::move(info));
    }

    if (result.empty()) {
        lastError_ = QStringLiteral("No editable controls found in the target window.");
    }
    return result;
}

bool UiaHelper::FillControlValue(IUIAutomationElement* element, const QString& text) {
    if (!element) {
        return false;
    }

    Microsoft::WRL::ComPtr<IUIAutomationValuePattern> valuePattern;
    HRESULT hr = element->GetCurrentPatternAs(UIA_ValuePatternId, IID_PPV_ARGS(&valuePattern));
    if (SUCCEEDED(hr) && valuePattern) {
        const int len = text.length();
        BSTR bstr = SysAllocStringLen(reinterpret_cast<const OLECHAR*>(text.utf16()), len);
        if (bstr) {
            hr = valuePattern->SetValue(bstr);
            SysFreeString(bstr);
            if (SUCCEEDED(hr)) {
                return true;
            }
        }
    }

    UIA_HWND nativeHwnd = nullptr;
    hr = element->get_CurrentNativeWindowHandle(&nativeHwnd);
    if (SUCCEEDED(hr) && nativeHwnd && ::IsWindow(reinterpret_cast<HWND>(nativeHwnd))) {
        if (::SetWindowTextW(reinterpret_cast<HWND>(nativeHwnd), text.toStdWString().c_str())) {
            return true;
        }
    }

    return false;
}

const wchar_t* UiaHelper::ControlTypeName(CONTROLTYPEID id) {
    switch (id) {
    case UIA_EditControlTypeId:      return L"Edit";
    case UIA_DocumentControlTypeId:  return L"Document";
    case UIA_ComboBoxControlTypeId: return L"ComboBox";
    case UIA_ListControlTypeId:      return L"List";
    case UIA_ListItemControlTypeId:  return L"ListItem";
    case UIA_TreeItemControlTypeId:  return L"TreeItem";
    case UIA_DataItemControlTypeId:  return L"DataItem";
    case UIA_HyperlinkControlTypeId: return L"Hyperlink";
    case UIA_TextControlTypeId:      return L"Text";
    default:                         return L"Control";
    }
}

QString UiaHelper::GetElementProperty(IUIAutomationElement* el, PROPERTYID id) const {
    if (!el) {
        return {};
    }
    VARIANT var = {0};
    VariantInit(&var);
    HRESULT hr = el->GetCurrentPropertyValue(id, &var);
    if (FAILED(hr)) {
        VariantClear(&var);
        return {};
    }
    QString result = VariantToWString(var);
    VariantClear(&var);
    return result;
}

RECT UiaHelper::GetElementBoundingRect(IUIAutomationElement* el) const {
    RECT result{};
    if (!el) {
        return result;
    }
    VARIANT var = {0};
    VariantInit(&var);
    HRESULT hr = el->GetCurrentPropertyValue(UIA_BoundingRectanglePropertyId, &var);
    if (FAILED(hr) || !(var.vt & VT_ARRAY) || !(var.vt & VT_R8)) {
        VariantClear(&var);
        return result;
    }

    SAFEARRAY* sa = var.parray;
    double values[4] = {0, 0, 0, 0};
    for (long i = 0; i < 4 && i < static_cast<long>(sa->rgsabound[0].cElements); ++i) {
        double d = 0.0;
        SafeArrayGetElement(sa, &i, &d);
        values[i] = d;
    }
    VariantClear(&var);

    result.left = static_cast<LONG>(values[0]);
    result.top = static_cast<LONG>(values[1]);
    result.right = static_cast<LONG>(values[0] + values[2]);
    result.bottom = static_cast<LONG>(values[1] + values[3]);
    return result;
}

QString UiaHelper::GetElementValue(IUIAutomationElement* el) const {
    Microsoft::WRL::ComPtr<IUIAutomationValuePattern> valuePattern;
    HRESULT hr = el->GetCurrentPatternAs(UIA_ValuePatternId, IID_PPV_ARGS(&valuePattern));
    if (SUCCEEDED(hr) && valuePattern) {
        BSTR bstr = nullptr;
        hr = valuePattern->get_CurrentValue(&bstr);
        if (SUCCEEDED(hr) && bstr) {
            QString result = QString::fromWCharArray(bstr, SysStringLen(bstr));
            SysFreeString(bstr);
            return result;
        }
    }
    return {};
}
