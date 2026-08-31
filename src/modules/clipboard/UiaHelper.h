#pragma once

#include <windows.h>
#include <UIAutomation.h>
#include <UIAutomationClient.h>
#include <wrl/client.h>

#include <QString>
#include <vector>

struct ControlInfo {
    Microsoft::WRL::ComPtr<IUIAutomationElement> element;
    int index = 0;
    QString controlType;
    QString name;
    QString automationId;
    QString currentValue;
    RECT boundingRect = {};
};

// Windows UI Automation helper. Ported from wtl_clipboard's UiaHelper, using
// WRL::ComPtr instead of ATL CComPtr.
class UiaHelper {
public:
    bool Initialize();
    std::vector<ControlInfo> EnumerateEditableControls(HWND targetWindow);
    bool FillControlValue(IUIAutomationElement* element, const QString& text);
    QString LastError() const { return lastError_; }

private:
    Microsoft::WRL::ComPtr<IUIAutomation> automation_;
    QString lastError_;

    static const wchar_t* ControlTypeName(CONTROLTYPEID id);
    QString GetElementProperty(IUIAutomationElement* el, PROPERTYID id) const;
    RECT GetElementBoundingRect(IUIAutomationElement* el) const;
    QString GetElementValue(IUIAutomationElement* el) const;
};
