// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++, C#, and Java: http://www.viva64.com

// License: LGPL-3.0

#ifndef NOTIFI_H
#define NOTIFI_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include <orbis/libkernel.h>

static void notifi(const char *p_Uri, const char *p_Format, ...) {
  OrbisNotificationRequest s_Request;
  memset(&s_Request, '\0', sizeof(s_Request));

  s_Request.reqId = NotificationRequest;
  s_Request.unk3 = 0;
  s_Request.useIconImageUri = 1;
  s_Request.targetId = -1;

  // Maximum size to move is destination size - 1 to allow for null terminator
  if (p_Uri != NULL && strnlen(p_Uri, sizeof(s_Request.iconUri)) + 1 > sizeof(s_Request.iconUri)) {
    strncpy(s_Request.iconUri, p_Uri, strnlen(p_Uri, sizeof(s_Request.iconUri) - 1));
  } else {
    s_Request.useIconImageUri = 0;
  }

  va_list p_Args;
  va_start(p_Args, p_Format);
  // p_Format is controlled externally, some compiler/linter options will mark this as a security issue
  vsnprintf(s_Request.message, sizeof(s_Request.message), p_Format, p_Args);
  va_end(p_Args);

  sceKernelSendNotificationRequest(NotificationRequest, &s_Request, sizeof(s_Request), 0);
}

#ifdef __cplusplus
}
#endif

#endif
