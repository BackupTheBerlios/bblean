/* ==========================================================================

  This file is part of the bbLean source code
  Copyright © 2001-2003 The Blackbox for Windows Development Team
  Copyright © 2004-2009 grischka

  http://bb4win.sourceforge.net/bblean
  http://developer.berlios.de/projects/bblean

  bbLean is free software, released under the GNU General Public License
  (GPL version 2). For details see:

  http://www.fsf.org/licenses/gpl.html

  This program is distributed in the hope that it will be useful, but
  WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
  or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
  for more details.

  ========================================================================== */

// Implements a system tray and keeps icons in a linked list of
// 'systemTray' structures so that plugins can display them.

#include "BB.h"
#include "Settings.h"
#include "MessageManager.h"
#include "bbshell.h"
#define INCLUDE_NIDS
#include "Tray.h"

#define ST static

//===========================================================================
/*
#define NIM_ADD         0x00000000
#define NIM_MODIFY      0x00000001
#define NIM_DELETE      0x00000002

#define NIM_SETFOCUS    0x00000003
#define NIM_SETVERSION  0x00000004
#define NOTIFYICON_VERSION 3

#define NIF_MESSAGE     0x00000001
#define NIF_ICON        0x00000002
#define NIF_TIP         0x00000004
#define NIF_STATE       0x00000008
#define NIF_INFO        0x00000010
#define NIF_GUID        0x00000020

#define NIS_HIDDEN      0x00000001
#define NIS_SHAREDICON  0x00000002

// Notify Icon Infotip flags
#define NIIF_NONE       0x00000000
#define NIIF_INFO       0x00000001
#define NIIF_WARNING    0x00000002
#define NIIF_ERROR      0x00000003

#define NIN_SELECT      WM_USER
#define NINF_KEY        1
#define NIN_KEYSELECT   (NIN_SELECT | NINF_KEY)

#define NIN_BALLOONSHOW (WM_USER + 2)
#define NIN_BALLOONHIDE (WM_USER + 3)
#define NIN_BALLOONTIMEOUT (WM_USER + 4)
#define NIN_BALLOONUSERCLICK (WM_USER + 5)
*/

//===========================================================================
#pragma pack(push,4)
typedef struct _SHELLTRAYDATA
{
    DWORD dwMagic; // e.g. 0x34753423;
    DWORD dwMessage;
    NOTIFYICONDATA iconData;
} SHELLTRAYDATA;
#pragma pack(pop)

typedef struct systemTrayNode
{
    struct systemTrayNode *next;
    bool hidden;
    bool shared;
    bool referenced;
    bool showTip;
    bool popup;
    unsigned version;
    HICON orig_icon;
    systemTrayBalloon* pBalloon;
    GUID guidItem;
    systemTray t;
} systemTrayNode;

//===========================================================================
// the 'Shell_TrayWnd'
ST HWND hTrayWnd;

// the icon vector
ST systemTrayNode *trayIconList;

// under explorer, setting hTrayWnd to topmost lets it receive the
// messages before explorer does.
ST bool tray_on_top;
ST bool tray_utf8;

ST void RemoveTrayIcon(systemTrayNode *p, bool post);

// private flag to indicate that a shared icon was not found
#define SHARED_NOT_FOUND 0x80

void LoadShellServiceObjects(void);
void UnloadShellServiceObjects(void);

//===========================================================================
// API: GetTraySize
//===========================================================================

int GetTraySize(void)
{
    systemTrayNode *p;
    int n = 0;
    dolist (p, trayIconList)
        if (false == p->hidden)
            n++;
    return n;
}

//===========================================================================
// API: GetTrayIcon
//===========================================================================

static systemTrayNode *nth_icon(int i)
{
    systemTrayNode *p; int n = 0;
    dolist (p, trayIconList)
        if (false == p->hidden && n++ == i)
            return p;
    return NULL;
}

systemTray* GetTrayIcon(int icon_index)
{
    systemTrayNode *p = nth_icon(icon_index);
    return p ? &p->t : NULL;
}

systemTrayBalloon* GetTrayBalloon(int icon_index)
{
    systemTrayNode *p = nth_icon(icon_index);
    if (!p || 0 == (p->t.uChanged & NIF_INFO))
        return NULL;
    p->t.uChanged &= ~NIF_INFO;
    return p->pBalloon;
}

//===========================================================================
// (API:) EnumTray
//===========================================================================

void EnumTray (TRAYENUMPROC lpEnumFunc, LPARAM lParam)
{
    systemTrayNode *p;
    dolist (p, trayIconList)
        if (false == p->hidden && FALSE == lpEnumFunc(&p->t, lParam))
            break;
}

//===========================================================================
// API: CleanTray
//===========================================================================

void CleanTray(void)
{
    systemTrayNode *p = trayIconList;
    while (p) {
        systemTrayNode *n = p->next;
        if (FALSE == IsWindow(p->t.hWnd))
            RemoveTrayIcon(p, true);
        p = n;
    }
}

//===========================================================================
// API: ForwardTrayMessage
//===========================================================================

ST void tray_notify(systemTrayNode *p, UINT message)
{
    if (p->version == 4) {
        POINT pt;
        GetCursorPos(&pt);
        SendNotifyMessage(p->t.hWnd, p->t.uCallbackMessage,
            MAKEWPARAM(pt.x, pt.y), MAKELPARAM(message, p->t.uID));
    } else {
        SendNotifyMessage(p->t.hWnd, p->t.uCallbackMessage, p->t.uID, message);
    }
}

// Reroute the mouse message to the tray icon's host window...
int ForwardTrayMessage(int icon_index, UINT message)
{
    systemTrayNode *p = nth_icon(icon_index);

    if (NULL == p || FALSE == IsWindow(p->t.hWnd)) {
        CleanTray();
        return 0;
    }

    if (WM_MOUSEMOVE != message) {
        /* Move/resize the hidden "Shell_TrayWnd" accordingly to the plugin where the
           mouseclick was on, since some tray-apps want to align their menu with it */
        HWND hwnd; POINT pt; RECT r;

        GetCursorPos(&pt);
        hwnd = WindowFromPoint(pt);
        if (is_bbwindow(hwnd)) {
            GetWindowRect(hwnd, &r);
            SetWindowPos(hTrayWnd, NULL,
                r.left, r.top, r.right-r.left, r.bottom-r.top, SWP_NOACTIVATE|SWP_NOZORDER);
        }

        /* allow the tray-app to grab focus */
        if (have_imp(pAllowSetForegroundWindow))
            pAllowSetForegroundWindow(ASFW_ANY);
        else
            SetForegroundWindow(p->t.hWnd);
    }

    tray_notify(p, message);
    // dbg_printf("ForwardTrayMessage %x %x", p->t.hWnd, message);

    if (p->version >= 3) {
        if (message == WM_RBUTTONUP)
            tray_notify(p, WM_CONTEXTMENU);
#if 0
        if (message == WM_MOUSEMOVE) {
            if (false == p->popup)
                tray_notify(p, NIN_POPUPOPEN), p->popup = true;
        } else if (message == WM_MOUSELEAVE) {
            tray_notify(p, NIN_POPUPCLOSE), p->popup = false;
        } else {
            systemTrayNode *q;
            dolist (q, trayIconList)
                if (q->popup)
                    tray_notify(q, NIN_POPUPCLOSE), q->popup = false;
        }
#endif
    }

    return 1;
}

//===========================================================================
// Function: reset_icon - clear the HICON and related entries
//===========================================================================

ST void reset_icon(systemTrayNode *p)
{
    if (false == p->shared)
    {
        if (p->referenced)
        {
            systemTrayNode *s;
            dolist (s, trayIconList)
                if (s->shared && s->orig_icon == p->orig_icon)
                    reset_icon(s);
            p->referenced = false;
        }
        if (p->t.hIcon)
            DestroyIcon(p->t.hIcon);
    }
    p->t.hIcon = NULL;
    p->orig_icon = NULL;
}

//===========================================================================
// Function: send_tray_message
//===========================================================================

ST void send_tray_message(systemTrayNode *p, unsigned uChanged, unsigned msg)
{
    if (uChanged && false == p->hidden) {
        p->t.uChanged = uChanged;
        MessageManager_Send(BB_TRAYUPDATE, (WPARAM)&p->t, msg);
    }
}

//===========================================================================
// Function: RemoveTrayIcon
//===========================================================================

ST void RemoveTrayIcon(systemTrayNode *p, bool post)
{
    reset_icon(p);
    if (p->pBalloon)
        m_free(p->pBalloon);
    remove_node(&trayIconList, p);
    if (post)
        send_tray_message(p, NIF_ICON, TRAYICON_REMOVED);
    m_free(p);
}

//===========================================================================
// Function: convert_string
//===========================================================================

ST bool convert_string(char *dest, const void *src, int nmax, bool is_unicode)
{
    char buffer[256];
    if (is_unicode) {
        bbWC2MB((const WCHAR*)src, buffer, sizeof buffer);
        src = buffer;
    }
    if (strcmp(dest, (const char *)src)) {
        strcpy_max(dest, (const char *)src, nmax);
        return true;
    }
    return false;
}

//===========================================================================
// Function: ModifyTrayIcon
//===========================================================================

ST UINT ModifyTrayIcon(systemTrayNode *p, NIDBB *pNid)
{
    UINT uChanged = 0;

    if ((pNid->uFlags & NIF_MESSAGE)
        && (pNid->uCallbackMessage != p->t.uCallbackMessage)) {
        p->t.uCallbackMessage = pNid->uCallbackMessage;
        uChanged |= NIF_MESSAGE;
    }

    if ((pNid->uFlags & NIF_TIP)) {
        if (convert_string(p->t.szTip, pNid->pTip,
                sizeof p->t.szTip, pNid->is_unicode))
            uChanged |= NIF_TIP;
    }

    if ((pNid->uFlags & NIF_INFO) && pNid->pInfo) {
        systemTrayBalloon *b = p->pBalloon;
        if (NULL == b)
            p->pBalloon = b = c_new(systemTrayBalloon);

        convert_string(b->szInfoTitle, pNid->pInfoTitle,
            sizeof b->szInfoTitle, pNid->is_unicode);
        convert_string(b->szInfo, pNid->pInfo,
            sizeof b->szInfo, pNid->is_unicode);

        b->dwInfoFlags = *pNid->pInfoFlags;
        if (b->szInfo[0] || b->szInfoTitle[0]) {
            uChanged |= NIF_INFO;
            b->uInfoTimeout = imax(2000, *pNid->pVersion_Timeout);
        } else {
            b->uInfoTimeout = 0;
        }
    }

    if ((pNid->uFlags & NIF_GUID) && pNid->pGuid) {
#if 0//ndef BBTINY
        if (Settings_LogFlag & LOG_TRAY) {
            WCHAR* pOle; char s_guid[200];
            StringFromCLSID(*pNid->pGuid, &pOle);
            convert_string(s_guid, pOle, 200, true);
            SHMalloc_Free(pOle);
            log_printf((LOG_TRAY, "Tray: update guidItem: %s", s_guid));
        }
#endif
        p->guidItem = *pNid->pGuid;
    }

    if (pNid->uFlags & NIF_SHOWTIP) {
        p->showTip = true;
    }

    if ((pNid->uFlags & NIF_ICON)) {
        if (p->shared) {
            systemTrayNode *o;
            dolist (o, trayIconList)
                if (o->orig_icon == pNid->hIcon && false == o->shared)
                    break;

            if (NULL == o || NULL == o->orig_icon) {
                uChanged |= SHARED_NOT_FOUND;
            } else if (p->orig_icon != pNid->hIcon) {
                p->t.hIcon = o->t.hIcon;
                o->referenced = true;
                p->orig_icon = pNid->hIcon;
                uChanged |= NIF_ICON;
            }
        } else {
            reset_icon(p);
            if (pNid->hIcon) {
                p->t.hIcon = CopyIcon(pNid->hIcon);
                p->orig_icon = pNid->hIcon;
            }
            uChanged |= NIF_ICON;
        }
    }
    return uChanged;
}

//===========================================================================

ST void log_tray(DWORD trayCommand, NIDBB *pNid)
{
    char class_name[100], tip[100];
    tip[0] = class_name[0] = 0;

    GetClassName(pNid->hWnd, class_name, sizeof class_name);

    if ((trayCommand == NIM_ADD || trayCommand == NIM_MODIFY)
        && (pNid->uFlags & NIF_TIP))
        convert_string(tip, pNid->pTip, sizeof tip, pNid->is_unicode);

    log_printf((LOG_TRAY,
        "Tray: %s(%d) "
        "hwnd:%X(%d) "
        "class:\"%s\" "

        "id:%d "
        "flag:%02X "
        "state:%X,%x "
        "msg:%X "
        "icon:%X "
        "tip:\"%s\"",

        trayCommand==NIM_ADD    ? "add" :
        trayCommand==NIM_MODIFY ? "mod" :
        trayCommand==NIM_DELETE ? "del" :
        trayCommand==NIM_SETVERSION ? "ver" : "ukn",
        trayCommand,
        pNid->hWnd, 0 != IsWindow(pNid->hWnd),
        class_name,

        pNid->uID,
        pNid->uFlags,
        pNid->pState ? pNid->pState[1] : 0,
        pNid->pState ? pNid->pState[0] : 0,
        pNid->uCallbackMessage,
        pNid->hIcon,
        tip
        ));
}

//===========================================================================

int pass_back(DWORD pid, HANDLE shmem, void *mem, int size)
{
    static LPVOID (WINAPI *pSHLockShared) (HANDLE, DWORD);
    static BOOL (WINAPI *pSHUnlockShared) (LPVOID lpData);
    LPVOID hMem;
    int ret = 0;

    if (load_imp(&pSHLockShared, "shlwapi.dll", "SHLockShared")
     && load_imp(&pSHUnlockShared, "shlwapi.dll", "SHUnlockShared")) {
        hMem = pSHLockShared (shmem, pid);
        if (NULL == hMem) {
            //dbg_printf("AppBarEvent: couldn't lock memory");
        } else {
            memcpy(hMem, mem, size);
            pSHUnlockShared(hMem);
            ret = 1;
        }
    } else {
        //dbg_printf("couldn't get SH(Un)LockShared function pointers");
    }
    return ret;
}

//===========================================================================

LRESULT AppBarEvent(void *data, unsigned size)
{
    DWORD *p_message;
    DWORD *p_pid;
    APPBARDATAV1 *abd, abd_ret;
    HANDLE32 *p_shmem;

    abd = (APPBARDATAV1*)data;

#if 0
    dbg_window((HWND)((APPBARDATAV1*)abd)->hWnd, "appevent");
    dbg_printf("%d %d %d", sizeof(APPBARDATAV1), sizeof(APPBARMSGDATAV1),
        offsetof(APPBARMSGDATAV1,dwMessage)) ;
    dbg_printf("%d %d %d", sizeof(APPBARDATAV2), sizeof(APPBARMSGDATAV2),
        offsetof(APPBARMSGDATAV2,dwMessage)) ;
#endif

    switch (size) {
    case sizeof(APPBARMSGDATAV1):
        p_message = &((APPBARMSGDATAV1*)abd)->dwMessage;
        p_shmem = (HANDLE32*)&((APPBARMSGDATAV1*)abd)->hSharedMemory;
        p_pid = &((APPBARMSGDATAV1*)abd)->dwSourceProcessId;
        break;

    case sizeof(APPBARMSGDATAV2):
        p_message = &((APPBARMSGDATAV2*)abd)->dwMessage;
        p_shmem = (HANDLE32*)&((APPBARMSGDATAV2*)abd)->hSharedMemory;
        p_pid = &((APPBARMSGDATAV2*)abd)->dwSourceProcessId;
        break;

    default:
        dbg_printf("AppBarEvent: unknown size: %d", size);
        return 0;
    }

    // dbg_printf("AppBarEventV%d message:%d hwnd:%x pid:%d shmem:%x size:%d", size == sizeof(APPBARMSGDATAV2) ? 2 : 1, *p_message, abd->hWnd, *p_pid, *p_shmem, size);

    log_printf((LOG_TRAY, "AppBarEventV%d message:%d hwnd:%x pid:%d shmem:%x size:%d",
        size == sizeof(APPBARMSGDATAV2) ? 2 : 1,
        *p_message, abd->hWnd, *p_pid, *p_shmem, size));

    switch (*p_message) {

    case ABM_NEW:  //0
        return TRUE;

    case ABM_REMOVE: //1
        return TRUE;

    case ABM_GETSTATE: //4
        return ABS_ALWAYSONTOP;

    case ABM_GETAUTOHIDEBAR:
        return FALSE; //7

    case ABM_GETTASKBARPOS: //5
        memcpy(&abd_ret, abd, sizeof abd_ret);
        GetWindowRect(hTrayWnd, &abd_ret.rc);
        if (abd_ret.rc.top < VScreenX + VScreenHeight/2)
            abd_ret.uEdge = ABE_TOP;
        else
            abd_ret.uEdge = ABE_BOTTOM;
        return pass_back(*p_pid, (HANDLE)*p_shmem, &abd_ret, sizeof abd_ret);
    }

    return FALSE;
}

//===========================================================================

ST LRESULT TrayEvent(void *data, unsigned size)
{
    DWORD trayCommand = ((SHELLTRAYDATA*)data)->dwMessage;
    void *pData = &((SHELLTRAYDATA*)data)->iconData;

    NIDBB nid;
    unsigned uChanged;
    systemTrayNode *p;

#ifdef _WIN64
/*
    The IsWow64Message function determines if the last message read from
    the current thread's queue originated from a WOW64 process. (User32.dll)
        BOOL IsWow64Message(void);

    The IsWow64Process function determines whether the specified process
    is running under WOW64. (Kernel32.dll)
        BOOL IsWow64Process(HANDLE hProcess, PBOOL Wow64Process);
*/
    // bool isWin64Icon = !have_imp(pIsWow64Message) || !pIsWow64Message();
    // if (false == isWin64Icon)
    {
        size                    = ((NIDNT_32*)pData)->cbSize        ;
        nid.hWnd                = (HWND)((NIDNT_32*)pData)->hWnd    ;
        nid.uID                 = ((NIDNT_32*)pData)->uID           ;
        nid.uFlags              = ((NIDNT_32*)pData)->uFlags        ;
        nid.uCallbackMessage    = ((NIDNT_32*)pData)->uCallbackMessage  ;
        nid.hIcon        = (HICON)((NIDNT_32*)pData)->hIcon         ;
        if (size >= sizeof(NID2KW_32)) {
        nid.is_unicode          = true;
        nid.pInfoFlags          = &((NID2KW_32*)pData)->dwInfoFlags ;
        nid.pInfoTitle          = &((NID2KW_32*)pData)->szInfoTitle ;
        nid.pVersion_Timeout    = &((NID2KW_32*)pData)->uVersion    ;
        nid.pInfo               = &((NID2KW_32*)pData)->szInfo      ;
        nid.pState              = &((NID2KW_32*)pData)->dwState     ;
        nid.pTip                = &((NID2KW_32*)pData)->szTip       ;
        if (size >= sizeof(NID2KW6_32))
            nid.pGuid           = &((NID2KW6_32*)pData)->guidItem   ;
        } else if (size >= sizeof(NID2K_32)) {
        nid.is_unicode          = false;
        nid.pInfoFlags          = &((NID2K_32*)pData)->dwInfoFlags  ;
        nid.pInfoTitle          = &((NID2K_32*)pData)->szInfoTitle  ;
        nid.pVersion_Timeout    = &((NID2K_32*)pData)->uVersion     ;
        nid.pInfo               = &((NID2K_32*)pData)->szInfo       ;
        nid.pState              = &((NID2K_32*)pData)->dwState      ;
        nid.pTip                = &((NID2K_32*)pData)->szTip        ;
        } else {
        nid.is_unicode          = (size == sizeof(NIDNT_32))        ;
        nid.pInfoFlags          = NULL;
        nid.pInfoTitle          = NULL;
        nid.pVersion_Timeout    = NULL;
        nid.pInfo               = NULL;
        nid.pState              = NULL;
        nid.pTip                = &((NIDNT_32*)pData)->szTip        ;
        }
    }
    // else
#else
    {
        size                    = ((NIDNT*)pData)->cbSize           ;
        nid.hWnd                = ((NIDNT*)pData)->hWnd             ;
        nid.uID                 = ((NIDNT*)pData)->uID              ;
        nid.uFlags              = ((NIDNT*)pData)->uFlags           ;
        nid.uCallbackMessage    = ((NIDNT*)pData)->uCallbackMessage ;
        nid.hIcon               = ((NIDNT*)pData)->hIcon            ;
        if (size >= sizeof(NID2KW)) {
        nid.is_unicode          = true;
        nid.pInfoFlags          = &((NID2KW*)pData)->dwInfoFlags    ;
        nid.pInfoTitle          = &((NID2KW*)pData)->szInfoTitle    ;
        nid.pVersion_Timeout    = &((NID2KW*)pData)->uVersion       ;
        nid.pInfo               = &((NID2KW*)pData)->szInfo         ;
        nid.pState              = &((NID2KW*)pData)->dwState        ;
        nid.pTip                = &((NID2KW*)pData)->szTip          ;
        if (size >= sizeof(NID2KW6))
            nid.pGuid           = &((NID2KW6*)pData)->guidItem      ;
        } else if (size >= sizeof(NID2K)) {
        nid.is_unicode          = false;
        nid.pInfoFlags          = &((NID2K*)pData)->dwInfoFlags     ;
        nid.pInfoTitle          = &((NID2K*)pData)->szInfoTitle     ;
        nid.pVersion_Timeout    = &((NID2K*)pData)->uVersion        ;
        nid.pInfo               = &((NID2K*)pData)->szInfo          ;
        nid.pState              = &((NID2K*)pData)->dwState         ;
        nid.pTip                = &((NID2K*)pData)->szTip           ;
        } else {
        nid.is_unicode          = (size == sizeof(NIDNT))           ;
        nid.pInfoFlags          = NULL;
        nid.pInfoTitle          = NULL;
        nid.pVersion_Timeout    = NULL;
        nid.pInfo               = NULL;
        nid.pState              = NULL;
        nid.pTip                = &((NIDNT*)pData)->szTip           ;
        }
    }
#endif

    // search the list
    dolist (p, trayIconList)
        if (p->t.hWnd == nid.hWnd && p->t.uID == nid.uID)
            break;

    if (Settings_LogFlag & LOG_TRAY)
        log_tray(trayCommand, &nid);

    if (NIM_DELETE == trayCommand) {
        // NIM_DELETE does not care for a valid hwnd
        if (NULL == p)
            return FALSE;
        RemoveTrayIcon(p, true);
        return TRUE;
    }

    if (FALSE == IsWindow(nid.hWnd)) {
        // has been seen even with NIM_ADD
        if (p)
            RemoveTrayIcon(p, true);
        return FALSE;
    }

    if (p) {
        nid.hidden = p->hidden;
        nid.shared = p->shared;
    } else {
        nid.hidden = nid.shared = false;
    }

    if ((nid.uFlags & NIF_STATE) && nid.pState) {
        if (nid.pState[1] & NIS_HIDDEN)
            nid.hidden = 0 != (nid.pState[0] & NIS_HIDDEN);

        if (nid.pState[1] & NIS_SHAREDICON)
            nid.shared = 0 != (nid.pState[0] & NIS_SHAREDICON);
    }

    if (NIM_MODIFY == trayCommand) {
        if (NULL == p)
            return FALSE;

        p->hidden = nid.hidden;
        if (p->shared != nid.shared) {
            // just in case, dont know if it ever happens
            reset_icon(p);
            p->shared = nid.shared;
        }

        uChanged = ModifyTrayIcon(p, &nid);
        if (uChanged & SHARED_NOT_FOUND)
            return FALSE; // icon remains unchanged

        send_tray_message(p, uChanged, TRAYICON_MODIFIED);
        return TRUE;
    }

    if (NIM_ADD == trayCommand) {
        if (p)
            return FALSE;

        p = c_new(systemTrayNode);
        append_node(&trayIconList, p);
        p->t.hWnd = nid.hWnd;
        p->t.uID  = nid.uID;
        p->hidden = nid.hidden;
        p->shared = nid.shared;

        uChanged = ModifyTrayIcon(p, &nid);
        if (uChanged & SHARED_NOT_FOUND) {
            // shared icons with an invalid reference are not added
            RemoveTrayIcon(p, false);
            return FALSE;
        }

        send_tray_message(p, uChanged, TRAYICON_ADDED);
        return TRUE;
    }

    if (NIM_SETVERSION == trayCommand) {
        if (NULL == p || NULL == nid.pVersion_Timeout)
            return FALSE;
        p->version = *nid.pVersion_Timeout;
        return TRUE;
    }

    return FALSE;
}

//===========================================================================
// TrayWnd Callback
//===========================================================================

ST LRESULT CALLBACK TrayWndProc(
    HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    void *data;
    unsigned size;
    int id;

    if (message == WM_WINDOWPOSCHANGED && tray_on_top) {
        SetWindowPos(hwnd, HWND_TOPMOST, 0,0,0,0,
            SWP_NOSIZE|SWP_NOMOVE|SWP_NOACTIVATE|SWP_NOSENDCHANGING);
        return 0;
    }

    if (message != WM_COPYDATA) {
        //dbg_printf("other message: %x %x %x", message, wParam, lParam);
        return DefWindowProc(hwnd, message, wParam, lParam);
    }

    data = ((COPYDATASTRUCT*)lParam)->lpData;
    size = ((COPYDATASTRUCT*)lParam)->cbData;
    id = ((COPYDATASTRUCT*)lParam)->dwData;

    if (id == 1)
        return TrayEvent(data, size);
    if (id == 0)
        return AppBarEvent(data, size);
#if 0
    unsigned n;
    dbg_printf("Tray: other WM_COPYDATA: %d", id);
    for (n = 0; (n+1) * 4 <= size; ++n)
        dbg_printf("   member %d: %08x", n, ((DWORD*)data)[n]);
#endif
    return FALSE;
}

//===========================================================================
// Function: LoadShellServiceObjects
// Purpose: Initializes COM, then starts all ShellServiceObjects listed in
//          the ShellServiceObjectDelayLoad registry key, e.g. Volume,
//          Power Management, PCMCIA, etc.
// In: void
// Out: void
//===========================================================================
#ifndef BBTINY

#include <docobj.h>

typedef struct sso_list_t
{
    struct sso_list_t *next;
    IOleCommandTarget *pOCT;
    char name[1];
} sso_list_t;

// the shell service objects vector
ST sso_list_t *sso_list;
ST HANDLE BBSSO_Stop;
ST HANDLE BBSSO_Thread;

#if defined __GNUC__ && __GNUC__ < 3
MDEFINE_GUID(CGID_ShellServiceObject, 0x000214D2L, 0, 0,0xC0,0,0,0,0,0,0,0x46);
#endif

// vista uses stobject.dll to load ShellServiceObjects
// {35CEC8A3-2BE6-11D2-8773-92E220524153}
// {730F6CDC-2C86-11D2-8773-92E220524153}

ST int sso_load(const char *name, const char *guid)
{
    WCHAR wszCLSID[200];
    CLSID clsid;
    IOleCommandTarget *pOCT = NULL;
    HRESULT hr;
    sso_list_t *t;

    MultiByteToWideChar(CP_ACP, 0, guid, 1+strlen(guid), wszCLSID, array_count(wszCLSID));
    CLSIDFromString(wszCLSID, &clsid);

    hr = CoCreateInstance(
        COMREF(clsid),
        NULL, 
        CLSCTX_INPROC_SERVER | CLSCTX_INPROC_HANDLER,
        COMREF(IID_IOleCommandTarget),
        (void **) &pOCT
        );

    log_printf((LOG_TRAY, "Tray: Load ShellServiceObject(%s): %s: %s",
        SUCCEEDED(hr)?"ok":"failed", name, guid));

    if (!SUCCEEDED(hr))
        return 1;

    // dbg_printf("Starting ShellService %d %s %s", (pOCT->AddRef(),pOCT->Release()), name, guid);
    COMCALL5(pOCT, Exec,
        &CGID_ShellServiceObject,
        2, // start
        0,
        NULL,
        NULL
        );

    t = (sso_list_t*)m_alloc(sizeof(sso_list_t) + strlen(name));
    t->pOCT = pOCT;
    strcpy(t->name, name);
    append_node(&sso_list, t);
    return 1;
}

ST DWORD WINAPI SSO_Thread(void *pv)
{
    HKEY hk0 = HKEY_LOCAL_MACHINE, hk1;
    const char *key =
        "Software\\Microsoft\\Windows\\CurrentVersion\\ShellServiceObjectDelayLoad";

    // CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_NOOLE1DDE);
    CoInitialize(0); // win95 compatible

    if (usingVista)
        sso_load("stobject", "{35CEC8A3-2BE6-11D2-8773-92E220524153}");


    if (ERROR_SUCCESS == RegOpenKeyEx(hk0, key, 0, KEY_READ, &hk1)) {
        int index;
        for (index = 0; ; ++index) {
            char szValueName[MAX_PATH];
            char szData[MAX_PATH];
            DWORD cbValueName, cbData, dwDataType;

            cbValueName = sizeof szValueName;
            cbData = sizeof szData;

            if (ERROR_SUCCESS != RegEnumValue(hk1, index, szValueName, &cbValueName,
                    0, &dwDataType, (LPBYTE) szData, &cbData))
                break;

            sso_load(szValueName, szData);
        }
        RegCloseKey(hk1);
    }

    // Wait for the exit event
    BBWait(0, 1, &BBSSO_Stop);

#ifndef _WIN64 // crashes on x64
    // Go through each element of the array and stop it..
    sso_list_t *t;
    dolist(t, sso_list) {
        IOleCommandTarget *pOCT = t->pOCT;
        /* sometimes for some reason trying to access the SSObject's vtbl
        here causes a GPF. Maybe it was already released. */
        if (IsBadReadPtr(*(void**)pOCT, 5*sizeof(void*)/*&Exec+1*/)) {
#ifdef BBOPT_MEMCHECK
            BBMessageBox(MB_OK, "Bad ShellService Object: %s", t->name);
#endif
            continue;
        }
        COMCALL5(pOCT, Exec,
            &CGID_ShellServiceObject,
            3, // stop
            0,
            NULL,
            NULL
            );
        // dbg_printf("Stopped ShellService %d %s", (pOCT->AddRef(),pOCT->Release()), t->name);
        COMCALL0(pOCT, Release);
    }
    // exception: ole32.dll,0x001288CE -> 766B88CE
#endif
    freeall(&sso_list);
    CoUninitialize();
    return 0;
}

void LoadShellServiceObjects(void)
{
    DWORD threadid;
    BBSSO_Stop = CreateEvent(NULL, FALSE, FALSE, "BBSSO_Stop");
    BBSSO_Thread = CreateThread(NULL, 0, SSO_Thread, NULL, 0, &threadid);
}

void UnloadShellServiceObjects(void)
{
    if (BBSSO_Thread) {
        SetEvent(BBSSO_Stop);
        BBWait(0, 1, &BBSSO_Thread);
        CloseHandle(BBSSO_Stop);
        CloseHandle(BBSSO_Thread);
        BBSSO_Thread = NULL;
    }
}

//===========================================================================
#endif //ndef BBTINY
//===========================================================================

ST void broadcast_tbcreated(void)
{
    SendNotifyMessage(HWND_BROADCAST, RegisterWindowMessage("TaskbarCreated"), 0, 0);
}

ST const char TrayNotifyClass [] = "TrayNotifyWnd";
ST const char TrayClockClass [] = "TrayClockWClass";

ST LRESULT CALLBACK TrayNotifyWndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    //log_printf((LOG_TRAY, "Tray: TrayNotifyWnd %04x msg %04x wp %08x lp %08x", hwnd, message, wParam, lParam));
    //dbg_printf("Tray: TrayNotifyWnd %04x msg %04x wp %08x lp %08x", hwnd, message, wParam, lParam);
    return DefWindowProc(hwnd, message, wParam, lParam);
}

ST HWND create_tray_child(HWND hwndParent, const char *class_name)
{
    BBRegisterClass(class_name, TrayNotifyWndProc, 0);
    return CreateWindow(
        class_name,
        NULL,
        WS_CHILD,
        0, 0, 0, 0,
        hwndParent,
        NULL,
        hMainInstance,
        NULL
        );
}

void Tray_SetEncoding(void)
{
    systemTrayNode *p;
    bool utf8 = 0 != Settings_UTF8Encoding;
    if (utf8 != tray_utf8)
        dolist (p, trayIconList) {
            WCHAR wstr[256];
            MultiByteToWideChar(tray_utf8 ? CP_UTF8 : CP_ACP, 0,
                p->t.szTip, -1, wstr, array_count(wstr));
            bbWC2MB(wstr, p->t.szTip, sizeof p->t.szTip);
        }
    tray_utf8 = utf8;
}

//===========================================================================
// hook tray when running under explorer

ST const char *trayClassName;
ST int (*trayHookDll_EntryFunc)(HWND);


//===========================================================================
// Public Tray interface

void Tray_Init(void)
{
    if (Settings_disableTray)
        return;

    log_printf((LOG_TRAY, "Tray: Starting"));

    trayClassName = "Shell_TrayWnd";
    tray_on_top = false;
    Tray_SetEncoding();

    if (underExplorer) {
        if (load_imp(&trayHookDll_EntryFunc, "trayhook.dll", "EntryFunc")) {
            // the trayhook will redirect messages from the real
            // "Shell_TrayWnd" to our window
            trayClassName = "bbTrayWnd";
        } else {
            // otherwise we hope that our window will get messages
            // first if it is on top.
            tray_on_top = true;
        }
    }

    BBRegisterClass(trayClassName, TrayWndProc, 0);
    hTrayWnd = CreateWindowEx(
        tray_on_top ? WS_EX_TOOLWINDOW|WS_EX_TOPMOST : WS_EX_TOOLWINDOW,
        trayClassName,
        NULL,
        WS_POPUP,
        0, 0, 0, 0,
        NULL,
        NULL,
        hMainInstance,
        NULL
        );

    if (have_imp(trayHookDll_EntryFunc)) {
        trayHookDll_EntryFunc(hTrayWnd);
    } else {
        // Some programs want these child windows so they can
        // figure out the presence/location of the tray.
        create_tray_child(
            create_tray_child(hTrayWnd, TrayNotifyClass),
            TrayClockClass);
    }

    broadcast_tbcreated();

    if (false == underExplorer)
        LoadShellServiceObjects();
}

void Tray_Exit(void)
{
    bool use_hook = have_imp(trayHookDll_EntryFunc);
    UnloadShellServiceObjects();
    if (hTrayWnd) {
        DestroyWindow(hTrayWnd);
        hTrayWnd = NULL;
        UnregisterClass(trayClassName, hMainInstance);
        if (false == use_hook) {
            UnregisterClass(TrayNotifyClass, hMainInstance);
            UnregisterClass(TrayClockClass, hMainInstance);
            if (underExplorer)
                broadcast_tbcreated();
        }
    }
    while (trayIconList)
        RemoveTrayIcon(trayIconList, false);
}

//===========================================================================
