//{{NO_DEPENDENCIES}}
// Resource IDs for FindFiles.rc
#pragma once

#define IDD_FINDFILES           100
#define IDD_PAGE_NAME           101
#define IDD_PAGE_DATE           102
#define IDD_PAGE_ADV            103
#define IDD_PAGE_DUP            104
#define IDR_MAINMENU            110
#define IDR_CONTEXTMENU         111
#define IDR_ACCEL               112

// Main dialog
#define IDC_TAB                 1000
#define IDC_FINDNOW             1001
#define IDC_STOP                1002
#define IDC_NEWSEARCH           1003
#define IDC_ANIM                1004
#define IDC_RESULTS             1005
#define IDC_STATUSBAR           1006
#define IDC_DELETEDUPS          1007

// Name & Location page
#define IDC_NAMED               1100
#define IDC_LOOKIN              1101
#define IDC_BROWSE              1102
#define IDC_SUBFOLDERS          1103

// Date Modified page (radio IDs must stay consecutive)
#define IDC_DATE_ALL            1200
#define IDC_DATE_BETWEEN        1201
#define IDC_DATE_MONTHS         1202
#define IDC_DATE_DAYS           1203
#define IDC_DATE_FROM           1204
#define IDC_DATE_TO             1205
#define IDC_MONTHS              1206
#define IDC_MONTHS_SPIN         1207
#define IDC_DAYS                1208
#define IDC_DAYS_SPIN           1209

// Advanced page
#define IDC_OFTYPE              1300
#define IDC_CONTAINING          1301
#define IDC_SIZEOP              1302
#define IDC_SIZEKB              1303
#define IDC_SIZE_SPIN           1304
#define IDC_HIDDEN              1305

// Duplicates page (radio IDs must stay consecutive, in DupMode order)
#define IDC_DUPS                1400
#define IDC_DUP_NAMESIZE        1401
#define IDC_DUP_CONTENT         1402
#define IDC_DUP_ALL             1403
#define IDC_DUP_GROUPBOX        1404
#define IDC_DUP_MINLABEL        1405
#define IDC_DUP_MINSIZE         1406
#define IDC_DUP_KBLABEL         1407
#define IDC_DUP_SKIPEMPTY       1408

// Menu commands
#define ID_FILE_OPEN_ITEM       32771
#define ID_FILE_OPENFOLDER      32772
#define ID_FILE_DELETE_ITEM     32773
#define ID_FILE_MOVETO          32774
#define ID_FILE_PROPERTIES      32775
#define ID_FILE_SAVERESULTS     32776
#define ID_FILE_CLOSEAPP        32777
#define ID_EDIT_COPYPATH        32780
#define ID_EDIT_SELECTALL       32781
#define ID_EDIT_INVERT          32782
#define ID_EDIT_CHECKSEL        32783   // keep CHECKSEL/UNCHECKSEL consecutive
#define ID_EDIT_UNCHECKSEL      32784
#define ID_EDIT_UNCHECKALL      32785
#define ID_DUP_KEEPFIRST        32790
#define ID_DUP_KEEPNEWEST       32791
#define ID_DUP_KEEPOLDEST       32792
#define ID_DUP_KEEPSHORTEST     32793
#define ID_DUP_CHECKFOLDER      32794   // keep CHECKFOLDER/UNCHECKFOLDER consecutive
#define ID_DUP_UNCHECKFOLDER    32795
#define ID_VIEWMODE_LARGEICONS      32800
#define ID_VIEWMODE_SMALLICONS      32801
#define ID_VIEWMODE_LIST            32802
#define ID_VIEWMODE_DETAILS         32803
#define ID_ARRANGE_NAME         32810
#define ID_ARRANGE_FOLDER       32811
#define ID_ARRANGE_SIZE         32812
#define ID_ARRANGE_TYPE         32813
#define ID_ARRANGE_DATE         32814
#define ID_GROUPBY_SET          32815   // keep GROUPBY_SET/GROUPBY_FOLDER consecutive
#define ID_GROUPBY_FOLDER       32816
#define ID_VIEW_REFRESH         32820
#define ID_OPT_CASE             32830
#define ID_OPT_DUPS             32831
#define ID_HELP_ABOUT           32840

#ifdef APSTUDIO_INVOKED
#ifndef APSTUDIO_READONLY_SYMBOLS
#define _APS_NEXT_RESOURCE_VALUE        120
#define _APS_NEXT_COMMAND_VALUE         32850
#define _APS_NEXT_CONTROL_VALUE         1500
#define _APS_NEXT_SYMED_VALUE           101
#endif
#endif
