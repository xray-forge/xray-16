// IInputReceiver.h: interface for the IInputReceiver class.
//
//////////////////////////////////////////////////////////////////////

#pragma once
#ifndef IINPUTRECEIVERH
#define IINPUTRECEIVERH
#include "xrCore/_flags.h"
#include "xrEngine/xr_level_controller.h"

// fwd. decl.
template <class T>
struct _vector2;
using Fvector2 = _vector2<float>;
using Ivector2 = _vector2<int>;

class ENGINE_API IInputReceiver
{
public:
    virtual ~IInputReceiver() = default;

    [[nodiscard]]
    bool IR_GetKeyState(int dik) const;

    virtual void IR_Capture();
    virtual void IR_Release();

    virtual void IR_OnActivate();
    virtual void IR_OnDeactivate();

    virtual void IR_OnMousePress(int /*btn*/) {}
    virtual void IR_OnMouseRelease(int /*btn*/) {}
    virtual void IR_OnMouseHold(int /*btn*/) {}
    virtual void IR_OnMouseWheel(float /*x*/, float /*y*/) {}
    virtual void IR_OnMouseMove(int /*x*/, int /*y*/) {}

    virtual void IR_OnKeyboardPress(int /*dik*/) {}
    virtual void IR_OnKeyboardRelease(int /*dik*/) {}
    virtual void IR_OnKeyboardHold(int /*dik*/) {}
    virtual void IR_OnTextInput(pcstr /*text*/) {}

    virtual void IR_OnControllerPress(int /*dik*/, const ControllerAxisState& /*state*/) {}
    virtual void IR_OnControllerRelease(int /*dik*/, const ControllerAxisState& /*state*/) {}
    virtual void IR_OnControllerHold(int /*dik*/, const ControllerAxisState& /*state*/) {}

    virtual void IR_OnControllerAttitudeChange(Fvector /*change*/) {}
};

ENGINE_API extern float psMouseSens;
ENGINE_API extern float psMouseSensScale;
ENGINE_API extern Flags32 psMouseInvert;

ENGINE_API extern float psControllerStickSensX;
ENGINE_API extern float psControllerStickSensY;
ENGINE_API extern float psControllerStickSensScale;
ENGINE_API extern float psControllerStickInnerDeadZone;
ENGINE_API extern float psControllerStickOuterDeadZone;
ENGINE_API extern float psControllerStickAngularDeadZone;
ENGINE_API extern float psControllerSensorSens;
ENGINE_API extern float psControllerSensorDeadZone;
ENGINE_API extern float psControllerCursorAutohideTime;
ENGINE_API extern Flags32 psControllerFlags;

enum EControllerFlags
{
    ControllerInvertX,
    ControllerInvertY,
    ControllerEnableSensors,
};

#endif
