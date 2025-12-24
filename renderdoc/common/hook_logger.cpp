/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2019-2025 Baldur Karlsson
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 ******************************************************************************/

#include "common/hook_logger.h"
#include "common/common.h"
#include "os/os_specific.h"
#include "strings/string_utils.h"

Threading::CriticalSection HookLogger::m_LogLock;
FILE *HookLogger::m_LogFile = NULL;
bool HookLogger::m_UseFile = false;

void HookLogger::Initialize(const rdcstr &outputPath)
{
  SCOPED_LOCK(m_LogLock);

  if(m_LogFile != NULL)
  {
    // Already initialized
    return;
  }

  if(outputPath.empty())
  {
    m_UseFile = false;
    m_LogFile = NULL;
  }
  else
  {
    m_UseFile = true;
#if ENABLED(RDOC_WIN32)
    rdcwstr wpath = StringFormat::UTF82Wide(outputPath);
    FILE *f = NULL;
    errno_t err = _wfopen_s(&f, wpath.c_str(), L"a");
    if(err == 0)
    {
      m_LogFile = f;
    }
    else
    {
      m_LogFile = NULL;
      m_UseFile = false;
    }
#else
    m_LogFile = fopen(outputPath.c_str(), "a");
    if(m_LogFile == NULL)
    {
      m_UseFile = false;
    }
#endif
  }
}

void HookLogger::LogFunctionCall(const char *functionName)
{
  if(functionName == NULL)
    return;

  SCOPED_LOCK(m_LogLock);

  uint64_t timestamp = Timing::GetTick();
  double timestampMs = double(timestamp) / Timing::GetTickFrequency() * 1000.0;

  if(m_UseFile && m_LogFile != NULL)
  {
    fprintf(m_LogFile, "[%.6f] %s\n", timestampMs, functionName);
    fflush(m_LogFile);
  }
  else
  {
    // Output to console using RDCLOG
    RDCLOG("[HOOK_LOG] [%.6f] %s", timestampMs, functionName);
  }
}

void HookLogger::Shutdown()
{
  SCOPED_LOCK(m_LogLock);

  if(m_LogFile != NULL)
  {
    fclose(m_LogFile);
    m_LogFile = NULL;
  }
  m_UseFile = false;
}

