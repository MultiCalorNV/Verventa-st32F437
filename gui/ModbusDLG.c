/*********************************************************************
	*          					MULTICALOR NV			                 *
	*          												             *
	*        													         *
	**********************************************************************
	*                                                                    *
	*        (c) 2014  MULTICALOR Eng.							         *
	*                                                                    *
	*        														     *
	*                                                                    *
	**********************************************************************
	
	** V1.0.0														    **
	All  Intellectual Property rights  in the Software belongs to  
	MULTICALOR.
	Knowledge of thesource code may not be used to write a similar product.  
	
	We appreciate your understanding and fairness.
	----------------------------------------------------------------------
	File        : GUI_verventa.c
	Purpose     : Sets up a multi Window GUI + Buttons
	----------------------------------------------------------------------
*/

/**
	******************************************************************************
	* @attention
	*
	* Licensed under MCD-ST Liberty SW License Agreement V2, (the "License");
	* You may not use this file except in compliance with the License.
	* You may obtain a copy of the License at:
	*
	*        http://www.st.com/software_license_agreement_liberty_v2
	*
	* Unless required by applicable law or agreed to in writing, software 
	* distributed under the License is distributed on an "AS IS" BASIS, 
	* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
	* See the License for the specific language governing permissions and
	* limitations under the License.
	*
	******************************************************************************
*/
/* Includes ------------------------------------------------------------------*/
#include <string.h>
#include <stdio.h>
#include "GUI_Verventa.h"

/* Defines -------------------------------------------------------------------*/

/* Exported types ------------------------------------------------------------*/
extern WM_HWIN _hWindow4;
WM_HWIN hWinModbusStats;

/*********************************************************************
	*
*       _aDialogCreate
*/
static const GUI_WIDGET_CREATE_INFO _aDialogCreate[] = {
	{ FRAMEWIN_CreateIndirect, "Verventa Thermostat", ID_FRAMEWIN_1, 0, 0, 320, 240, 0, 0, 0 },

	{ TEXT_CreateIndirect, "modbus_TimeOut",	ID_TEXT_10, 10, 25, 150, 12, 0, 100, 0 },
	{ TEXT_CreateIndirect, "modbus_Slaves",		ID_TEXT_11, 10, 50, 150, 12, 0, 100, 0 },
	{ TEXT_CreateIndirect, "modbus_connects",	ID_TEXT_21, 10, 75, 200, 12, 0, 100, 0 },
	{ TEXT_CreateIndirect, "modbus_reserve",	ID_TEXT_31, 10, 100, 150, 12, 0, 100, 0 },
	{ BUTTON_CreateIndirect, "btnSetup",		ID_BUTTON_10, 85, 155, 100, 45, 0, 0, 0 }
};

/*********************************************************************
*
*       _cbDialog
*/
static void _cbDialog(WM_MESSAGE * pMsg){
  WM_HWIN hItem;
  int Id, NCode, t1;

  switch (pMsg->MsgId) {
	  case WM_INIT_DIALOG:
		// Initialization of 'Thermostat'
		hItem = pMsg->hWin;
		FRAMEWIN_SetFont(hItem, &GUI_Font8x16);
		FRAMEWIN_SetTextAlign(hItem, GUI_TA_HCENTER | GUI_TA_VCENTER);
		FRAMEWIN_SetTextColor(hItem, 0x00000000);
		// Initialization of 'modbus_TimeOut'
		hItem = WM_GetDialogItem(pMsg->hWin, ID_TEXT_10);
		TEXT_SetText(hItem, "modbus timeout");
		TEXT_SetFont(hItem, GUI_FONT_8X16);
		TEXT_SetTextAlign(hItem, GUI_TA_HCENTER | GUI_TA_VCENTER);
		// Initialization of 'modbus_Slaves'
		hItem = WM_GetDialogItem(pMsg->hWin, ID_TEXT_11);
		TEXT_SetFont(hItem, GUI_FONT_8X16);
		TEXT_SetText(hItem, "modbus slaves");
		TEXT_SetTextAlign(hItem, GUI_TA_HCENTER | GUI_TA_VCENTER);
		// Initialization of 'modbus_connects'
		hItem = WM_GetDialogItem(pMsg->hWin, ID_TEXT_21);
		TEXT_SetFont(hItem, GUI_FONT_8X16);
		TEXT_SetText(hItem, "modbus connections");
		TEXT_SetTextAlign(hItem, GUI_TA_HCENTER | GUI_TA_VCENTER);
		// Initialization of 'modbus_reserve'
		hItem = WM_GetDialogItem(pMsg->hWin, ID_TEXT_31);
		TEXT_SetFont(hItem, GUI_FONT_8X16);
		TEXT_SetText(hItem, "modbus stats");
		TEXT_SetTextAlign(hItem, GUI_TA_HCENTER | GUI_TA_VCENTER);
		// Initialization of 'btnSetup'
		hItem = WM_GetDialogItem(pMsg->hWin, ID_BUTTON_10);
		BUTTON_SetText(hItem, "main");
		BUTTON_SetFont(hItem, GUI_FONT_16B_ASCII);
		break;
	  case WM_TOUCH:
		break;
	  case WM_NOTIFY_PARENT:
		Id    = WM_GetId(pMsg->hWinSrc);
		NCode = pMsg->Data.v;
		switch(Id) {
			case ID_BUTTON_10: // Notifications sent by 'btnSetup'
			  switch(NCode) {
				  case WM_NOTIFICATION_CLICKED:
					break;
				  case WM_NOTIFICATION_RELEASED:
					ShowWindow(_hWindow4);
					break;
			  }
			break;
		}
	  break;
	  default:
		WM_DefaultProc(pMsg);
		break;
	}
}

void UpdateModbus_gui(uint8_t *text){
	uint8_t buf[8];
	WM_HWIN h;
	h = WM_GetDialogItem(hWinModbusStats, ID_TEXT_11);	
	//sprintf(buf,"%2d",g_manSetTmp);
	TEXT_SetText(h, buf);
}

/*********************************************************************
*
*       CreateThermostat
**********************************************************************
*/

WM_HWIN CreateModbusStats(void) {
  WM_HWIN hWin;
  
  FRAMEWIN_SetDefaultSkin(FRAMEWIN_SKIN_FLEX);
  BUTTON_SetDefaultSkin(BUTTON_SKIN_FLEX);

  hWin = GUI_CreateDialogBox(_aDialogCreate, GUI_COUNTOF(_aDialogCreate), &_cbDialog, WM_HBKWIN, 0, 0);
  
  WM_HideWindow(hWin);
  
  return hWin;
}
