
void make_bb_balloon(
    plugin_info * PI,
    systemTray *pIcon,
    systemTrayBalloon *pBalloon,
    RECT *r
    );


void exit_bb_balloon(void);

void ClearToolTips(HWND hwnd);
void SetToolTip(HWND hwnd, RECT *tipRect, char *tipText);
void InitToolTips(HINSTANCE hInstance);
void ExitToolTips();

