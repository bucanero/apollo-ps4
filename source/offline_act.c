#include <stdint.h>
#include <string.h>
#include <orbis/RegMgr.h>

/* https://github.com/oct0xor/ps4_registry_editor/blob/master/Ps4EditLib/PsRegistry/Preferences.cs#L133 */
#define REGMGR_SECURITY_PARENTAL_PASSCODE (0x03800800)

#define SIZE_account_id     8
#define SIZE_NP_env         17
#define SIZE_login_flag     4
#define SIZE_user_name      17
#define SIZE_passcode       17

#define KEY_account_id(a)   Get_Entity_Number(a, 16, 65536, 125830400, 125829120)
#define KEY_NP_env(a)       Get_Entity_Number(a, 16, 65536, 125874183, 125874176)
#define KEY_login_flag(a)   Get_Entity_Number(a, 16, 65536, 125831168, 125829120)
#define KEY_user_name(a)    Get_Entity_Number(a, 16, 65536, 125829632, 125829120)

/*
    based on PS4 Offline Account Activator by barthen
    https://github.com/charlyzard/PS4OfflineAccountActivator
*/

static int Get_Entity_Number(int a, int b, int c, int d, int e)
{
    if (a < 1 || a > b)
    {
        return e;
    }
    return (a - 1) * c + d;
}

int regMgr_GetAccountId(int userNumber, uint64_t* psnAccountId)
{
    return sceRegMgrGetBin(KEY_account_id(userNumber), psnAccountId, SIZE_account_id);
}

int regMgr_SetAccountId(int userNumber, uint64_t* psnAccountId)
{
    int errorCode = 0;

    errorCode |= sceRegMgrSetBin(KEY_account_id(userNumber), psnAccountId, SIZE_account_id);
    errorCode |= sceRegMgrSetStr(KEY_NP_env(userNumber), "np", 3);
    errorCode |= sceRegMgrSetInt(KEY_login_flag(userNumber), 6);

    return errorCode;
}

int regMgr_GetUserName(int userNumber, char* outString)
{
    outString[0] = 0;
    return sceRegMgrGetStr(KEY_user_name(userNumber), outString, SIZE_user_name);
}

// https://github.com/nekohaku/ps4-parental-controls/blob/main/source/main.c
int regMgr_GetParentalPasscode(char* buff)
{
    return sceRegMgrGetStr(REGMGR_SECURITY_PARENTAL_PASSCODE, buff, SIZE_passcode);
}
