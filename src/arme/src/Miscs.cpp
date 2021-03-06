#include <Arme/CommonFuncs.h>

// Copyright (C) 2003 Dolphin Project.

// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, version 2.0 or later versions.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License 2.0 for more details.

// A copy of the GPL 2.0 should have been included with the program.
// If not, see http://www.gnu.org/licenses/

// Official SVN repository and contact information can be found at
// http://code.google.com/p/dolphin-emu/

#include <Arme/Common.h>
#include <string.h>

#if defined(__APPLE__)
#define __thread
#endif

#ifdef _WIN32
#include <Arme/CommonWindows.h>
#endif

// Generic function to get last error message.
// Call directly after the command or use the error num.
// This function might change the error code.
const char *GetLastErrorMsg()
{
#ifdef _WIN32
    return GetStringErrorMsg(GetLastError());
#else
    return GetStringErrorMsg(errno);
#endif
}

const char *GetStringErrorMsg(int errCode) {
    static const size_t buff_size = 1023;
#ifdef _WIN32
    static char err_str[buff_size] = {};

    FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, NULL, errCode,
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        err_str, buff_size, NULL);
#else
    static char err_str[buff_size] = {};

    // Thread safe (XSI-compliant)
    if (strerror_r(errCode, err_str, buff_size) == 0) {
        return "Unknown error";
    }
#endif

    return err_str;
}