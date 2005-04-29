/*
     This file is part of GNUnet.
     (C) 2001, 2002, 2003, 2004, 2005 Christian Grothoff (and other contributing authors)

     GNUnet is free software; you can redistribute it and/or modify
     it under the terms of the GNU General Public License as published
     by the Free Software Foundation; either version 2, or (at your
     option) any later version.

     GNUnet is distributed in the hope that it will be useful, but
     WITHOUT ANY WARRANTY; without even the implied warranty of
     MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
     General Public License for more details.

     You should have received a copy of the GNU General Public License
     along with GNUnet; see the file COPYING.  If not, write to the
     Free Software Foundation, Inc., 59 Temple Place - Suite 330,
     Boston, MA 02111-1307, USA.
*/

/**
 * @file util/winproc.h
 * @brief Definitions for MS Windows
 * @author Nils Durner
 **/

#ifndef _WINPROC_H
#define _WINPROC_H

#include <io.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/timeb.h>
#include <time.h>
#include <dirent.h>
#include <windows.h>
#include <winsock.h>
#include <winerror.h>
#include <iphlpapi.h>
#include <shlobj.h>
#include <objbase.h>
#include <sys/param.h>  /* #define BYTE_ORDER */
#include "gnunet_util.h"
#include "platform.h"

#include "plibc.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef DWORD WINAPI (*TNtQuerySystemInformation) (int, PVOID, ULONG, PULONG);
typedef DWORD WINAPI (*TGetIfEntry) (PMIB_IFROW pIfRow);
typedef DWORD WINAPI (*TGetIpAddrTable) (PMIB_IPADDRTABLE pIpAddrTable,
                      PULONG pdwSize, BOOL bOrder);
typedef DWORD WINAPI (*TGetIfTable) (PMIB_IFTABLE pIfTable, PULONG pdwSize,
                      BOOL bOrder);
typedef DWORD WINAPI (*TCreateHardLink) (LPCTSTR lpFileName,
                      LPCTSTR lpExistingFileName, LPSECURITY_ATTRIBUTES
                      lpSecurityAttributes);
typedef SC_HANDLE WINAPI (*TOpenSCManager) (LPCTSTR lpMachineName,
                          LPCTSTR lpDatabaseName, DWORD dwDesiredAccess);
typedef SC_HANDLE WINAPI (*TCreateService) (SC_HANDLE hSCManager,
                           LPCTSTR lpServiceName, LPCTSTR lpDisplayName,
                           DWORD dwDesiredAccess, DWORD dwServiceType,
                           DWORD dwStartType, DWORD dwErrorControl,
                           LPCTSTR lpBinaryPathName, LPCTSTR lpLoadOrderGroup,
                           LPDWORD lpdwTagId, LPCTSTR lpDependencies,
                           LPCTSTR lpServiceStartName, LPCTSTR lpPassword);
typedef BOOL WINAPI (*TCloseServiceHandle) (SC_HANDLE hSCObject);
typedef BOOL WINAPI (*TDeleteService) (SC_HANDLE hService);
typedef SERVICE_STATUS_HANDLE WINAPI (*TRegisterServiceCtrlHandler) (
                                      LPCTSTR lpServiceName,
                                      LPHANDLER_FUNCTION lpHandlerProc);
typedef BOOL WINAPI (*TSetServiceStatus) (SERVICE_STATUS_HANDLE hServiceStatus,
                      LPSERVICE_STATUS lpServiceStatus);
typedef BOOL WINAPI (*TStartServiceCtrlDispatcher) (const LPSERVICE_TABLE_ENTRY
                     lpServiceTable);
typedef BOOL WINAPI (*TControlService) (SC_HANDLE hService, DWORD dwControl,
                     LPSERVICE_STATUS lpServiceStatus);
typedef SC_HANDLE WINAPI (*TOpenService) (SC_HANDLE hSCManager, LPCTSTR lpServiceName,
                          DWORD dwDesiredAccess);
typedef DWORD WINAPI (*TGetBestInterface) (IPAddr dwDestAddr, PDWORD pdwBestIfIndex);
typedef DWORD WINAPI (*TGetAdaptersInfo) (PIP_ADAPTER_INFO pAdapterInfo, PULONG pOutBufLen);


extern TNtQuerySystemInformation GNNtQuerySystemInformation;
extern TGetIfEntry GNGetIfEntry;
extern TGetIpAddrTable GNGetIpAddrTable;
extern TGetIfTable GNGetIfTable;
extern TCreateHardLink GNCreateHardLink;
extern TOpenSCManager GNOpenSCManager;
extern TCreateService GNCreateService;
extern TCloseServiceHandle GNCloseServiceHandle;
extern TDeleteService GNDeleteService;
extern TRegisterServiceCtrlHandler GNRegisterServiceCtrlHandler;
extern TSetServiceStatus GNSetServiceStatus;
extern TStartServiceCtrlDispatcher GNStartServiceCtrlDispatcher;
extern TControlService GNControlService;
extern TOpenService GNOpenService;
extern TGetBestInterface GNGetBestInterface;
extern TGetAdaptersInfo GGetAdaptersInfo;

BOOL CreateShortcut(const char *pszSrc, const char *pszDest);
BOOL DereferenceShortcut(char *pszShortcut);
long QueryRegistry(HKEY hMainKey, char *pszKey, char *pszSubKey,
              char *pszBuffer, long *pdLength);
int ListNICs(void (*callback) (char *, int));

void GNInitWinEnv();
void GNShutdownWinEnv();

#ifdef __cplusplus
}
#endif

#endif
