/*
 *  Copyright (C) 1999-2002 Harri Porten (porten@kde.org)
 *  Copyright (C) 2001 Peter Kelly (pmk@post.com)
 *  Copyright (C) 2004, 2007, 2008 Apple Inc. All rights reserved.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public License
 *  along with this library; see the file COPYING.LIB.  If not, write to
 *  the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *  Boston, MA 02110-1301, USA.
 *
 */

#if (defined(EMSCRIPTEN)) && !(defined(EMSCRIPTEN_NATIVE))
#include <cmath>
#include <cstring>
extern "C"
{
    // Adapted from GNUlib.
    // TODO - find properly-licensed version and upstream into Emscripten.
    // TODO - there seems to be some confusion with asm.js - the version of _signbitd
    // with the single leading underscore is required.
    int
    _signbitd (double arg)
    {
        /* This does not do the right thing for NaN, but this is irrelevant for
         *     most use cases.  */
        if (isnan (arg))
            return 0;
        if (arg < 0.0)
            return 1;
        else if (arg == 0.0)
        {
            /* Distinguish 0.0 and -0.0.  */
            static double plus_zero = 0.0;
            double arg_mem = arg;
            return (memcmp (&plus_zero, &arg_mem, sizeof(double)) != 0);
        }
        else
            return 0;
    }
    int
    __signbitd (double arg)
    {
        /* This does not do the right thing for NaN, but this is irrelevant for
         *     most use cases.  */
        if (isnan (arg))
            return 0;
        if (arg < 0.0)
            return 1;
        else if (arg == 0.0)
        {
            /* Distinguish 0.0 and -0.0.  */
            static double plus_zero = 0.0;
            double arg_mem = arg;
            return (memcmp (&plus_zero, &arg_mem, sizeof(double)) != 0);
        }
        else
            return 0;
    }
}

#endif
#include "config.h"
#include "JSNumberCell.h"

#if USE(JSVALUE32)

#include "NumberObject.h"
#include "UString.h"

namespace JSC {

JSValue JSNumberCell::toPrimitive(ExecState*, PreferredPrimitiveType) const
{
    return const_cast<JSNumberCell*>(this);
}

bool JSNumberCell::getPrimitiveNumber(ExecState*, double& number, JSValue& value)
{
    number = m_value;
    value = this;
    return true;
}

bool JSNumberCell::toBoolean(ExecState*) const
{
    return m_value < 0.0 || m_value > 0.0; // false for NaN
}

double JSNumberCell::toNumber(ExecState*) const
{
  return m_value;
}

UString JSNumberCell::toString(ExecState*) const
{
    return UString::from(m_value);
}

UString JSNumberCell::toThisString(ExecState*) const
{
    return UString::from(m_value);
}

JSObject* JSNumberCell::toObject(ExecState* exec) const
{
    return constructNumber(exec, const_cast<JSNumberCell*>(this));
}

JSObject* JSNumberCell::toThisObject(ExecState* exec) const
{
    return constructNumber(exec, const_cast<JSNumberCell*>(this));
}

bool JSNumberCell::getUInt32(uint32_t& uint32) const
{
    uint32 = static_cast<uint32_t>(m_value);
    return uint32 == m_value;
}

JSValue JSNumberCell::getJSNumber()
{
    return this;
}

JSValue jsNumberCell(ExecState* exec, double d)
{
    return new (exec) JSNumberCell(exec, d);
}

JSValue jsNumberCell(JSGlobalData* globalData, double d)
{
    return new (globalData) JSNumberCell(globalData, d);
}

} // namespace JSC

#else // USE(JSVALUE32)

// Keep our exported symbols lists happy.
namespace JSC {

JSValue jsNumberCell(ExecState*, double);

JSValue jsNumberCell(ExecState*, double)
{
    ASSERT_NOT_REACHED();
    return JSValue();
}

} // namespace JSC

#endif // USE(JSVALUE32)
