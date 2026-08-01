#pragma once

#define IMPLEMENT_ENUM_FLAG_OPERATORS(TEnum, TBaseType)\
    inline TEnum operator|(TEnum a, TEnum b) { return (TEnum)((TBaseType)a | (TBaseType)b); }\
    inline TEnum operator&(TEnum a, TEnum b) { return (TEnum)((TBaseType)a & (TBaseType)b); }\
    inline TEnum& operator|=(TEnum& a, TEnum b) { return a = a | b; }\
    inline TEnum& operator&=(TEnum& a, TEnum b) { return a = a & b; }
#define _RELEASE(x)\
    {\
        if ((x))\
        {\
            (x)->Release();\
            (x) = nullptr;\
        }\
    }
#ifdef MASTER_GOLD
#define _SHOW_REF(msg, x) { /* do nothing */ }
#else
#define _SHOW_REF(msg, x)\
    {\
        if ((x))\
        {\
            (x)->AddRef();\
            const u32 _show_ref_count = u32((x)->Release());\
            if (_show_ref_count > 1)\
                Log(msg, _show_ref_count);\
        }\
    }
#endif
